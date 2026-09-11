// Yggdrasil
// Copyright (C) 2026 Zeeshan Qazi
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#ifndef YGGDRASIL_CONTEXTUALIZE_HPP
#define YGGDRASIL_CONTEXTUALIZE_HPP

#include <tuple>
#include <type_traits>
#include <utility>
#include <string_view>
#include <meta>
#include <vector>
#include <array>
#include <string>
#include <expected>

namespace yggdrasil {

// ─── Internal helpers ─────────────────────────────────────────────────────────

namespace detail {

struct empty_agg {};

// Evaluate whether a call result constitutes success.
// Supports: bool-convertible types, std::expected / std::optional (.has_value()).
template <typename T>
constexpr bool evaluate_chain_result(const T& res) {
    if constexpr (requires { static_cast<bool>(res); }) {
        return static_cast<bool>(res);
    } else if constexpr (requires { res.has_value(); }) {
        return res.has_value();
    } else {
        return true;
    }
}

consteval void suffix_name(std::string& str, int index) {
    if (index == 0) { str += "_0"; return; }
    std::string num;
    while (index > 0) {
        num = static_cast<char>('0' + (index % 10)) + num;
        index /= 10;
    }
    str += "_" + num;
}

// Build a new aggregate type whose fields are:
//   [PrevAgg fields as refs] + [Target non-empty fields as refs] + [Args as refs]
//
// callable     – the function actually being called (for return-type deduction).
// param_source – the function whose parameter list is used for naming the arg_*
//   fields.  For direct functions these are the same.  For Yggdrasil proxy
//   members, callable is the proxy's operator() (no named params) while
//   param_source is the original on-handler that carries named params.
template <typename PrevAgg, typename Target,
          std::meta::info callable, std::meta::info param_source,
          typename... Args>
consteval auto generate_chain_agg_type() {
    static constexpr auto tgt_members = std::define_static_array(
        std::meta::nonstatic_data_members_of(^^Target, std::meta::access_context::current()));

    constexpr size_t tgt_valid_count = []() {
        size_t cnt = 0;
        template for (constexpr auto amem : tgt_members) {
            using MemType = std::remove_cvref_t<typename [: std::meta::type_of(amem) :]>;
            if constexpr (!(std::is_class_v<MemType> && std::is_empty_v<MemType>)) {
                cnt++;
            }
        }
        return cnt;
    }();

    if constexpr (tgt_valid_count == 0 && sizeof...(Args) == 0 && !std::is_same_v<PrevAgg, empty_agg>) {
        return std::type_identity<PrevAgg>{};
    } else {
        struct GeneratedAgg;
        consteval {
            std::vector<std::meta::info> members;
            std::vector<std::string> used_names;

            auto add_member = [&](std::meta::info type, std::string_view base_name,
                                  std::vector<std::meta::info> anns = {}) {
                std::string name(base_name);
                size_t count = 0;
                while (true) {
                    bool conflict = false;
                    for (const auto& un : used_names) {
                        if (un == name) { conflict = true; break; }
                    }
                    if (!conflict) break;
                    count++;
                    std::string num;
                    size_t idx = count;
                    while (idx > 0) {
                        num = static_cast<char>('0' + (idx % 10)) + num;
                        idx /= 10;
                    }
                    name = std::string(base_name) + "_" + num;
                }
                used_names.push_back(name);
                members.push_back(std::meta::data_member_spec(type, {
                    .name        = name,
                    .annotations = std::move(anns),
                }));
            };

            if constexpr (!std::is_same_v<PrevAgg, empty_agg>) {
                static constexpr auto prev_members = std::define_static_array(
                    std::meta::nonstatic_data_members_of(^^PrevAgg, std::meta::access_context::current()));
                template for (constexpr auto amem : prev_members) {
                    // Propagate annotations that were carried from the original member
                    // into the previous aggregate — keep them alive through the chain.
                    add_member(std::meta::type_of(amem), std::meta::identifier_of(amem),
                               std::vector(std::meta::annotations_of(amem)));
                }
            }

            template for (constexpr auto amem : tgt_members) {
                using MemType = std::remove_cvref_t<typename [: std::meta::type_of(amem) :]>;
                if constexpr (!(std::is_class_v<MemType> && std::is_empty_v<MemType>)) {
                    using RefType = std::add_lvalue_reference_t<typename [: std::meta::type_of(amem) :]>;
                    // Preserve all annotations from the original data member so that
                    // downstream visitors (e.g. for_each, json_serializer) see the
                    // same annotation set on the context aggregate's reference fields.
                    add_member(^^RefType, std::meta::identifier_of(amem),
                               std::vector(std::meta::annotations_of(amem)));
                }
            }

            size_t param_idx = std::is_same_v<PrevAgg, empty_agg> ? 0 : 1;
            // Use param_source (not callable) for parameter name lookup — for proxy
            // members these differ: callable is operator() with no named params,
            // while param_source is the original function with named params.
            constexpr bool has_params = std::meta::is_function(param_source);
            static constexpr auto arg_types = std::define_static_array(std::vector<std::meta::info>{ ^^Args... });

            auto suffix_name = [](std::string& name, size_t count) {
                std::string num;
                if (count == 0) num = "0";
                else {
                    size_t idx = count;
                    while (idx > 0) {
                        num = static_cast<char>('0' + (idx % 10)) + num;
                        idx /= 10;
                    }
                }
                name += "_" + num;
            };

            template for (constexpr auto arg_type : arg_types) {
                std::string p_name = "arg";
                if constexpr (has_params) {
                    static constexpr auto params = std::define_static_array(std::meta::parameters_of(param_source));
                    if (param_idx < params.size()) {
                        auto p = params[param_idx];
                        if (std::meta::has_identifier(p)) {
                            p_name = std::meta::identifier_of(p);
                        } else {
                            suffix_name(p_name, param_idx);
                        }
                    } else {
                        suffix_name(p_name, param_idx);
                    }
                } else {
                    suffix_name(p_name, param_idx);
                }
                add_member(arg_type, p_name);
                param_idx++;
            }

            std::meta::define_aggregate(^^GeneratedAgg, members);
        }
        return std::type_identity<GeneratedAgg>{};
    }
}

template <typename T>
consteval std::meta::info get_callable_from_type_helper() {
    if constexpr (std::is_class_v<T>) {
        if constexpr (requires { sizeof(T); }) {
            for (auto mm : std::meta::members_of(^^T, std::meta::access_context::current())) {
                if (std::meta::is_function(mm)) return mm;
            }
        }
    }
    return ^^void;
}

template <std::meta::info mem>
consteval std::meta::info get_chain_callable_info() {
    if constexpr (std::meta::is_function(mem)) return mem;
    else if constexpr (std::meta::is_nonstatic_data_member(mem)) {
        constexpr auto t = std::meta::type_of(mem);
        std::meta::info callable = ^^void;
        auto extract_fn = [&]<typename T>() {
            callable = get_callable_from_type_helper<std::remove_cvref_t<T>>();
        };
        template for (constexpr auto type_info : {t}) {
            extract_fn.template operator()<typename [:type_info:]>();
        }
        if (callable != ^^void) return callable;
    }
    if constexpr (!std::meta::is_type(mem) &&
        !std::meta::is_nonstatic_data_member(mem) &&
        !std::meta::is_enumerator(mem)) {
        return mem;
    }
    return ^^void;
}

// For proxy data members that expose a `static constexpr std::meta::info original_method_v`
// (e.g. Yggdrasil's EventProxyMethod), discover the original on-handler function so that
// its parameter names can be used to name the arg_* fields in the generated aggregate.
// Falls back to get_chain_callable_info (the proxy's operator()) when no such member exists.
template <typename T>
consteval std::meta::info get_param_source_callable_helper() {
    if constexpr (std::is_class_v<T>) {
        if constexpr (requires { sizeof(T); }) {
            for (auto mm : std::meta::members_of(^^T, std::meta::access_context::current())) {
                if (std::meta::is_variable(mm) && std::meta::has_identifier(mm) &&
                    std::meta::identifier_of(mm) == "original_method_v") {
                    auto original_fn = std::meta::extract<std::meta::info>(mm);
                    if (std::meta::is_function(original_fn)) return original_fn;
                }
            }
        }
    }
    return ^^void;
}

template <std::meta::info mem>
consteval std::meta::info get_param_source_callable() {
    if constexpr (std::meta::is_function(mem)) return mem;
    else if constexpr (std::meta::is_nonstatic_data_member(mem)) {
        constexpr auto t = std::meta::type_of(mem);
        std::meta::info original = ^^void;
        auto extract_fn = [&]<typename T>() {
            original = get_param_source_callable_helper<std::remove_cvref_t<T>>();
        };
        template for (constexpr auto type_info : {t}) {
            extract_fn.template operator()<typename [:type_info:]>();
        }
        if (original != ^^void) return original;
    }
    // Fall back: use the same callable as for invocation.
    return get_chain_callable_info<mem>();
}


// ─── chain_state ─────────────────────────────────────────────────────────────

template <typename ChainTuple_, typename ResultTuple_, typename PrevAgg_>
struct chain_state {
    using ChainTuple  = ChainTuple_;
    using ResultTuple = ResultTuple_;
    using PrevAgg     = PrevAgg_;

    ChainTuple  chain;
    ResultTuple results;
    PrevAgg     prev_agg;
    bool        has_error;
};

// ─── ChainProxy / ChainProxyMethod ───────────────────────────────────────────

template <typename Proxy, typename State, size_t NextObjectIndex,
          std::meta::info mem, std::meta::info callable, ptrdiff_t objectOffset>
struct ChainProxyMethod;

template <typename State, size_t NextObjectIndex>
consteval auto generate_chain_proxy();

template <typename T> struct is_expected_type : std::false_type {};
template <typename T, typename E> struct is_expected_type<std::expected<T, E>> : std::true_type {};

template <typename T> struct expected_err_type { using type = std::string; };
template <typename T, typename E> struct expected_err_type<std::expected<T, E>> { using type = E; };

template <typename Proxy, typename State, size_t NextObjectIndex,
          std::meta::info mem, std::meta::info callable, ptrdiff_t objectOffset>
struct ChainProxyMethod {
    template <typename... Args>
    auto operator()(Args&&... args) {
        auto& state = *reinterpret_cast<State*>(
            reinterpret_cast<char*>(this) + objectOffset);

        constexpr bool is_last =
            (NextObjectIndex == (std::tuple_size_v<typename State::ChainTuple> - 1));

        using TargetType = std::remove_cvref_t<std::tuple_element_t<NextObjectIndex, typename State::ChainTuple>>;
        using PrevAgg    = typename State::PrevAgg;

        auto& target = std::get<NextObjectIndex>(state.chain);

        // param_source: the function whose parameter list supplies field names for
        // the arg_* members of the aggregate.  For direct functions this equals
        // callable.  For Yggdrasil proxy members it is the original on-handler
        // (embedded as a template argument of the proxy type) which carries the
        // real parameter names (tradeId, fillQty, fillPx, …).
        constexpr auto param_source = get_param_source_callable<mem>();
        constexpr auto agg_type_id = generate_chain_agg_type<PrevAgg, TargetType, callable, param_source, Args...>();
        using AggType = typename decltype(agg_type_id)::type;
        auto args_tuple = std::forward_as_tuple(args...);

        auto prev_tup = []<typename P>(P& p) {
            if constexpr (std::is_same_v<P, empty_agg>) return std::tuple<>{};
            else {
                static constexpr auto prev_members = std::define_static_array(
                    std::meta::nonstatic_data_members_of(^^P, std::meta::access_context::current()));
                return [&]<size_t... Is>(std::index_sequence<Is...>) {
                    return std::tuple_cat(
                        []<size_t I>(auto& p_) {
                            constexpr auto pm = prev_members[I];
                            return std::forward_as_tuple(p_.[:pm:]);
                        }.template operator()<Is>(p)...
                    );
                }(std::make_index_sequence<prev_members.size()>{});
            }
        }(state.prev_agg);

        auto tgt_tup = []<typename T>(T& t) {
            static constexpr auto tgt_members = std::define_static_array(
                std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()));
            return [&]<size_t... Is>(std::index_sequence<Is...>) {
                return std::tuple_cat(
                    []<size_t I>(auto& t_) {
                        constexpr auto tm = tgt_members[I];
                        using MemType = std::remove_cvref_t<typename [: std::meta::type_of(tm) :]>;
                        if constexpr (!(std::is_class_v<MemType> && std::is_empty_v<MemType>)) {
                            return std::forward_as_tuple(t_.[:tm:]);
                        } else {
                            return std::tuple<>{};
                        }
                    }.template operator()<Is>(t)...
                );
            }(std::make_index_sequence<tgt_members.size()>{});
        }(target);

        auto all_fields = std::tuple_cat(prev_tup, tgt_tup, args_tuple);
        auto next_agg   = std::make_from_tuple<AggType>(all_fields);

        auto _res_type_id = [&]() {
            if constexpr (std::is_same_v<PrevAgg, empty_agg>) {
                return std::type_identity<std::remove_cvref_t<
                    decltype(target.[:mem:](std::forward<Args>(args)...))>>{};
            } else {
                return std::type_identity<std::remove_cvref_t<
                    decltype(target.[:mem:](state.prev_agg, std::forward<Args>(args)...))>>{};
            }
        }();
        using ResType  = typename decltype(_res_type_id)::type;
        using SlotType = std::conditional_t<std::is_void_v<ResType>,
                                            std::expected<void, std::string>,
                                            ResType>;

        using CombinedResultsType = decltype(std::tuple_cat(
            std::declval<typename State::ResultTuple>(),
            std::declval<std::tuple<SlotType>>()
        ));

        SlotType slot = [&]() -> SlotType {
            if (state.has_error) {
                if constexpr (std::is_void_v<ResType>)
                    return SlotType{std::unexpected(std::string("skipped"))};
                else if constexpr (is_expected_type<ResType>::value)
                    return SlotType{std::unexpected(typename expected_err_type<ResType>::type("skipped"))};
                else
                    return SlotType{};
            }
            if constexpr (std::is_same_v<PrevAgg, empty_agg>) {
                if constexpr (std::is_void_v<ResType>) {
                    target.[:mem:](std::forward<Args>(args)...);
                    return SlotType{};
                } else {
                    return target.[:mem:](std::forward<Args>(args)...);
                }
            } else {
                if constexpr (std::is_void_v<ResType>) {
                    target.[:mem:](state.prev_agg, std::forward<Args>(args)...);
                    return SlotType{};
                } else {
                    return target.[:mem:](state.prev_agg, std::forward<Args>(args)...);
                }
            }
        }();

        bool new_has_error = state.has_error || !evaluate_chain_result(slot);
        CombinedResultsType combined_results = std::tuple_cat(
            std::move(state.results),
            std::make_tuple(std::move(slot))
        );

        if constexpr (is_last) {
            return combined_results;
        } else {
            using NextState = chain_state<typename State::ChainTuple, CombinedResultsType, AggType>;
            constexpr auto next_proxy_id = generate_chain_proxy<NextState, NextObjectIndex + 1>();
            using NextProxy = typename decltype(next_proxy_id)::type;
            return NextProxy{ NextState{state.chain, std::move(combined_results), std::move(next_agg), new_has_error} };
        }
    }
};

template <typename State, size_t NextObjectIndex>
consteval auto generate_chain_proxy() {
    struct Proxy;
    consteval {
        std::vector<std::meta::info> proxy_members;
        proxy_members.push_back(std::meta::data_member_spec(^^State, {.name = "state"}));

        struct dummy {
            alignas(alignof(State)) char state[sizeof(State)];
        };
        constexpr ptrdiff_t objectOffset = -offsetof(dummy, state);

        using CurrentTarget = std::tuple_element_t<NextObjectIndex, typename State::ChainTuple>;
        using TargetType    = std::remove_cvref_t<CurrentTarget>;

        static constexpr auto tgt_members = std::define_static_array(
            std::meta::members_of(^^TargetType, std::meta::access_context::current()));
        template for (constexpr auto mem : tgt_members) {
            constexpr auto callable = get_chain_callable_info<mem>();
            if constexpr ((callable != ^^void) && std::meta::has_identifier(mem)) {
                proxy_members.push_back(std::meta::data_member_spec(
                    ^^ChainProxyMethod<Proxy, State, NextObjectIndex, mem, callable, objectOffset>,
                    {.name = std::meta::identifier_of(mem), .no_unique_address = true}
                ));
            }
        }
        std::meta::define_aggregate(^^Proxy, proxy_members);
    }
    return std::type_identity<Proxy>{};
}

} // namespace detail

// ─── Public API ──────────────────────────────────────────────────────────────

/// contextualize(obj1, obj2, ...)
///
/// Returns a chain proxy that lets you call methods on obj1, then obj2, etc.
/// Each step receives a context aggregate containing all previous outputs plus
/// the current object's fields as references.  If any step returns a falsy or
/// error result, subsequent steps are skipped and a neutral slot is returned.
///
/// Usage:
///   auto [r1, r2] = yggdrasil::contextualize(fsm, serializer)
///       .trade("T001", 40u, 150.0)
///       .to_json();
template <typename... Objects>
auto contextualize(Objects&... objects) {
    using InitialState = detail::chain_state<
        std::tuple<Objects&...>,
        std::tuple<>,
        detail::empty_agg>;
    constexpr auto proxy_id = detail::generate_chain_proxy<InitialState, 0>();
    using ProxyType = typename decltype(proxy_id)::type;
    return ProxyType{InitialState{
        std::forward_as_tuple(objects...),
        std::tuple<>{},
        detail::empty_agg{},
        false
    }};
}

} // namespace yggdrasil

#endif // !defined(YGGDRASIL_CONTEXTUALIZE_HPP)
