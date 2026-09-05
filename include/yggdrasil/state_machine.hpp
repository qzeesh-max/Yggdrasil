#ifndef YGGDRASIL_STATE_MACHINE_HPP
#define YGGDRASIL_STATE_MACHINE_HPP

#include <variant>
#include <tuple>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <string_view>
#include <expected>
#include <array>
#include <string>

#if __has_include(<meta>)
#include <meta>
#endif

namespace yggdrasil {

struct state_machine {
    template <auto... t>
    struct any_of {
        constexpr static const auto allowed = {t...};
    };

    struct initial {};
    struct final {};
    struct can_revert_final {};

    template<typename T>
    struct transition
    {
        T target;
        consteval transition(T target) : target(target) {}
    };

    template <auto V>
    struct init_val {
        constexpr static auto value = V;
    };

    template <size_t N>
    struct fixed_string {
        char buf[N + 1]{};
        consteval fixed_string(const char (&s)[N + 1]) {
            for (size_t i = 0; i <= N; ++i) buf[i] = s[i];
        }
        constexpr std::string_view view() const { return {buf, N}; }
    };
    
    template <size_t N>
    fixed_string(const char (&)[N]) -> fixed_string<N - 1>;

    template <std::meta::info field_>
    struct init_from {
        static constexpr const auto field = field_;
    };
    
    template <size_t N>
    struct on_error {
        fixed_string<N> msg;
        consteval on_error(const char (&s)[N+1]) : msg(s) {}
        constexpr std::string_view message() const { return msg.view(); }
    };
    
    template <size_t N>
    on_error(const char (&)[N]) -> on_error<N - 1>;

    enum class transition_result {
        accepted,
        rejected,
    };

    struct on {
        transition_result result{};
        uint32_t targetState{};
    };

    constexpr static const on accepted{transition_result::accepted, {}};
    constexpr static const on rejected{transition_result::rejected, {}};

    template <typename T>
    static on to(T target) {
        return on{transition_result::accepted, static_cast<uint32_t>(target)};
    }

    struct format {};

    template <std::meta::info field>
    struct mapping{
    };

    struct storage_key{
    };
};

namespace detail {

template <typename T>
struct is_transition : std::false_type {};

template <typename T>
struct is_transition<state_machine::transition<T>> : std::true_type {};

template <typename T> struct is_can_revert_final : std::false_type {};

template <> struct is_can_revert_final<state_machine::can_revert_final> : std::true_type {};

template <typename T>
struct is_init_val : std::false_type {};

template <auto V>
struct is_init_val<state_machine::init_val<V>> : std::true_type {};

template <typename T>
struct is_init_from : std::false_type {};

template <std::meta::info F>
struct is_init_from<state_machine::init_from<F>> : std::true_type {
    static constexpr std::meta::info field = F;
};

template <typename T> struct is_initial : std::false_type {};
template <> struct is_initial<state_machine::initial> : std::true_type {};

template <typename T> struct is_final_state : std::false_type {};
template <> struct is_final_state<state_machine::final> : std::true_type {};

template <typename T> struct is_on_error : std::false_type {};
template <size_t N> struct is_on_error<state_machine::on_error<N>> : std::true_type {};

template <typename T> struct is_mapping : std::false_type {};
template <std::meta::info F> struct is_mapping<state_machine::mapping<F>> : std::true_type {
    static constexpr std::meta::info field = F;
};

template <typename T> struct is_storage_key : std::false_type {};
template <> struct is_storage_key<state_machine::storage_key> : std::true_type {};


template <size_t N>
struct targets_array {
    uint32_t data[N]{};
    uint32_t size = N;
};

template <typename T>
struct extract_targets {
    static consteval targets_array<0> get() { return {}; }
};

template <auto... ts>
struct extract_targets<state_machine::any_of<ts...>> {
    static consteval targets_array<sizeof...(ts)> get() {
        return {{static_cast<uint32_t>(ts)...}, sizeof...(ts)};
    }
};

struct fixed_state_rules {
    struct final_state_rule {
        uint32_t state;
        char error_msg[128]{};
        uint32_t error_msg_len{0};
    };
    final_state_rule final_states[64]{};
    uint32_t num_final_states{0};

    struct transition_rule {
        uint32_t state;
        uint32_t num_targets;
        uint32_t targets[32];
        char error_msg[128]{};
        uint32_t error_msg_len{0};
    };
    transition_rule transitions[64]{};
    uint32_t num_transitions{0};
};

template <auto... ts>
constexpr bool is_valid_target(uint32_t chosen, state_machine::any_of<ts...>) {
    for (auto t : {ts...}) {
        if (chosen == static_cast<uint32_t>(t)) return true;
    }
    return false;
}

template <typename T>
constexpr bool is_valid_target(uint32_t chosen, T single_target) {
    return chosen == static_cast<uint32_t>(single_target);
}

template <auto... ts>
constexpr uint32_t resolve_target_state(const state_machine::on& result, state_machine::any_of<ts...>) {
    return result.targetState; 
}

template <typename T>
constexpr uint32_t resolve_target_state(const state_machine::on& result, T single_target) {
    return static_cast<uint32_t>(single_target);
}

template <auto... ts>
constexpr bool is_any_target_allowed(uint32_t current_state, const fixed_state_rules& rules, state_machine::any_of<ts...>) {
    for (uint32_t i = 0; i < rules.num_transitions; ++i) {
        if (rules.transitions[i].state == current_state) {
            for (auto t : {ts...}) {
                for (uint32_t j = 0; j < rules.transitions[i].num_targets; ++j) {
                    if (rules.transitions[i].targets[j] == static_cast<uint32_t>(t)) {
                        return true;
                    }
                }
            }
            break;
        }
    }
    return false;
}

template <typename T>
constexpr bool is_any_target_allowed(uint32_t current_state, const fixed_state_rules& rules, T single_target) {
    for (uint32_t i = 0; i < rules.num_transitions; ++i) {
        if (rules.transitions[i].state == current_state) {
            for (uint32_t j = 0; j < rules.transitions[i].num_targets; ++j) {
                if (rules.transitions[i].targets[j] == static_cast<uint32_t>(single_target)) {
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

template <typename T, std::meta::info method, size_t I, typename ArgsTuple>
constexpr void assign_arg(T& obj, ArgsTuple& args_tuple) {
    static constexpr auto params = std::define_static_array(std::meta::parameters_of(method));
    static constexpr auto param = params[I];
    if constexpr (std::meta::has_identifier(param)) {
        constexpr auto param_name = std::meta::identifier_of(param);
        static constexpr auto members = std::define_static_array(std::meta::members_of(^^T, std::meta::access_context::current()));
        template for (constexpr auto mem : members) {
            if constexpr (std::meta::is_nonstatic_data_member(mem) && std::meta::has_identifier(mem)) {
                if constexpr (std::meta::identifier_of(mem) == param_name) {
                    obj.[:mem:] = std::get<I>(args_tuple);
                }
            }
        }
    }
}

template <typename T, std::meta::info method, typename... Args, size_t... Is>
constexpr void assign_all_args(T& obj, std::tuple<Args...>& args_tuple, std::index_sequence<Is...>) {
    (assign_arg<T, method, Is, std::tuple<Args...>>(obj, args_tuple), ...);
}

template <typename FSM, typename RulesType, std::meta::info method, ptrdiff_t offset, uint32_t TargetState>
struct InitialProxyMethod {
    template <typename... Args>
    auto operator()(Args&&... args) {
        auto& fsm_proxy = *reinterpret_cast<FSM*>(reinterpret_cast<char*>(this) + offset);
        auto& rules = fsm_proxy.__rules__;

        std::tuple<Args...> args_tuple(std::forward<Args>(args)...);
        assign_all_args<RulesType, method, Args...>(rules, args_tuple, std::make_index_sequence<sizeof...(Args)>{});
        
        static constexpr auto members = std::define_static_array(std::meta::members_of(^^RulesType, std::meta::access_context::current()));
        template for (constexpr auto mem : members) {
            if constexpr (std::meta::is_nonstatic_data_member(mem) && std::meta::has_identifier(mem)) {
                if constexpr (std::meta::identifier_of(mem) == std::meta::identifier_of(^^RulesType)) {
                    using StateEnum = typename [: (std::meta::type_of(mem)) :];
                    rules.[:mem:] = static_cast<StateEnum>(TargetState);
                }
                
                static constexpr auto annos = std::define_static_array(std::meta::annotations_of(mem));
                template for (constexpr auto anno : annos) {
                    using AnnoType = std::remove_cvref_t<typename [: std::meta::type_of(anno) :]>;
                    if constexpr (detail::is_init_val<AnnoType>::value) {
                        rules.[:mem:] = static_cast<typename [: std::meta::type_of(mem) :]>(AnnoType::value);
                    } else if constexpr (detail::is_init_from<AnnoType>::value) {
                        rules.[:mem:] = rules.[:AnnoType::field:];
                    }
                }
            }
        }
    }
};

template <typename FSM, typename RulesType, std::meta::info method, ptrdiff_t offset, fixed_state_rules RulesData, std::meta::info state_mem>
struct EventProxyMethod {
    template <typename... Args>
    std::expected<void, std::string> operator()(Args&&... args) {
        auto& fsm_proxy = *reinterpret_cast<FSM*>(reinterpret_cast<char*>(this) + offset);
        auto& rules = fsm_proxy.__rules__;

        uint32_t current_state = static_cast<uint32_t>(rules.[:state_mem:]);

        bool early_valid = false;
        static constexpr auto annos = std::define_static_array(std::meta::annotations_of(method));
        template for (constexpr auto anno : annos) {
            using AnnoType = std::remove_cvref_t<typename [: std::meta::type_of(anno) :]>;
            if constexpr (is_can_revert_final<AnnoType>::value) {
                early_valid = true;
                break;
            }
            if constexpr (is_transition<AnnoType>::value) {
                constexpr auto extracted = std::meta::extract<AnnoType>(anno);
                if (is_any_target_allowed(current_state, RulesData, extracted.target)) {
                    early_valid = true;
                }
            }
        }
        
        if (!early_valid) {
            std::string_view err = "Invalid transition from current state";
            for (uint32_t i = 0; i < RulesData.num_transitions; ++i) {
                if (RulesData.transitions[i].state == current_state) {
                    if (RulesData.transitions[i].error_msg_len > 0) {
                        err = std::string_view(RulesData.transitions[i].error_msg, RulesData.transitions[i].error_msg_len);
                    }
                    break;
                }
            }
            for (uint32_t i = 0; i < RulesData.num_final_states; ++i) {
                if (RulesData.final_states[i].state == current_state) {
                    if (RulesData.final_states[i].error_msg_len > 0) {
                        err = std::string_view(RulesData.final_states[i].error_msg, RulesData.final_states[i].error_msg_len);
                    }
                    break;
                }
            }
            return std::unexpected(std::string(err));
        }

        // --- mapping + storage_key: duplicate-key check (pre-transition) ---
        // Detect if this method has [[=mapping<^^field>{}]] and [[=storage_key{}]] annotations.
        // If so, find the key argument, look it up in the map, and reject if it already exists.
        static constexpr std::meta::info mapping_field = []() consteval {
            template for (constexpr auto anno : annos) {
                using AnnoType = std::remove_cvref_t<typename [: std::meta::type_of(anno) :]>;
                if constexpr (is_mapping<AnnoType>::value) {
                    return is_mapping<AnnoType>::field;
                }
            }
            return ^^void;
        }();

        static constexpr std::meta::info storage_key_param = []() consteval {
            if constexpr (mapping_field == ^^void) return ^^void;
            static constexpr auto params = std::define_static_array(std::meta::parameters_of(method));
            template for (constexpr auto p : params) {
                static constexpr auto p_annos = std::define_static_array(std::meta::annotations_of(p));
                template for (constexpr auto pa : p_annos) {
                    using PAType = std::remove_cvref_t<typename [: std::meta::type_of(pa) :]>;
                    if constexpr (is_storage_key<PAType>::value) {
                        return p;
                    }
                }
            }
            return ^^void;
        }();

        // Compute the index of the storage_key param in the Args pack.
        static constexpr size_t key_arg_idx = []() consteval {
            if constexpr (storage_key_param == ^^void) return size_t(-1);
            static constexpr auto params = std::define_static_array(std::meta::parameters_of(method));
            size_t idx = 0;
            template for (constexpr auto p : params) {
                if constexpr (p == storage_key_param) return idx;
                idx++;
            }
            return size_t(-1);
        }();

        // safe_key_idx: clamp to 0 to avoid std::get<size_t(-1)> instantiation errors.
        // The template for guard ensures this code only executes at runtime when mapping is active.
        static constexpr size_t safe_key_idx = (key_arg_idx == size_t(-1)) ? 0 : key_arg_idx;

        // --- mapping + storage_key: duplicate-key check (pre-transition) ---
        // Use template for over a 0- or 1-element array (lazy splicer evaluation).
        // safe_key_idx == 0 means "no mapping" (clamped); only push when key_arg_idx was valid.
        static constexpr auto mapping_fields_arr = []() consteval {
            std::vector<std::meta::info> v;
            if constexpr (key_arg_idx != size_t(-1)) v.push_back(mapping_field);
            return std::define_static_array(v);
        }();

        template for (constexpr auto mf : mapping_fields_arr) {
            auto& map_ref = rules.[:mf:];
            auto key = std::string(std::get<safe_key_idx>(std::forward_as_tuple(args...)));
            if (map_ref.count(key)) {
                std::string_view dup_err = "Duplicate key";
                template for (constexpr auto anno : annos) {
                    using AnnoType = std::remove_cvref_t<typename [: std::meta::type_of(anno) :]>;
                    if constexpr (is_on_error<AnnoType>::value) {
                        constexpr auto err_obj = std::meta::extract<AnnoType>(anno);
                        dup_err = err_obj.message();
                    }
                }
                return std::unexpected(std::string(dup_err));
            }
        }

        auto result = rules.[:method:](std::forward<Args>(args)...);
        if (result.result == state_machine::transition_result::rejected) {
            std::string_view event_reject_error = "Event rejected by handler";
            template for (constexpr auto anno : annos) {
                using AnnoType = std::remove_cvref_t<typename [: std::meta::type_of(anno) :]>;
                if constexpr (is_on_error<AnnoType>::value) {
                    constexpr auto err_obj = std::meta::extract<AnnoType>(anno);
                    event_reject_error = err_obj.message();
                }
            }
            return std::unexpected(std::string(event_reject_error));
        }

        uint32_t chosen_target = 0;
        bool valid = false;
        bool allowed = false;

        template for (constexpr auto anno : annos) {
            using AnnoType = std::remove_cvref_t<typename [: std::meta::type_of(anno) :]>;
            if constexpr (is_can_revert_final<AnnoType>::value) {
                chosen_target = result.targetState;
                allowed = true;
            } else if constexpr (is_transition<AnnoType>::value) {
                constexpr auto extracted = std::meta::extract<AnnoType>(anno);
                chosen_target = resolve_target_state(result, extracted.target);
                if (is_valid_target(chosen_target, extracted.target)) {
                    valid = true;
                }
            }
        }

        if (!valid) {
            return std::unexpected("Invalid target state returned from handler");
        }

        if (!allowed)
        {
            for (uint32_t i = 0; i < RulesData.num_transitions; ++i) {
                if (RulesData.transitions[i].state == current_state) {
                    for (uint32_t j = 0; j < RulesData.transitions[i].num_targets; ++j) {
                        if (RulesData.transitions[i].targets[j] == chosen_target) {
                            allowed = true;
                            break;
                        }
                    }
                    break;
                }
            }
        }
        
        if (!allowed) {
            std::string_view err = "Invalid transition from current state";
            for (uint32_t i = 0; i < RulesData.num_transitions; ++i) {
                if (RulesData.transitions[i].state == current_state) {
                    if (RulesData.transitions[i].error_msg_len > 0) {
                        err = std::string_view(RulesData.transitions[i].error_msg, RulesData.transitions[i].error_msg_len);
                    }
                    break;
                }
            }
            for (uint32_t i = 0; i < RulesData.num_final_states; ++i) {
                if (RulesData.final_states[i].state == current_state) {
                    if (RulesData.final_states[i].error_msg_len > 0) {
                        err = std::string_view(RulesData.final_states[i].error_msg, RulesData.final_states[i].error_msg_len);
                    }
                    break;
                }
            }
            return std::unexpected(std::string(err));
        }

        // Update state
        static constexpr auto members = std::define_static_array(std::meta::members_of(^^RulesType, std::meta::access_context::current()));
        template for (constexpr auto mem : members) {
            if constexpr (std::meta::is_nonstatic_data_member(mem) && std::meta::has_identifier(mem)) {
                if constexpr (std::meta::identifier_of(mem) == std::meta::identifier_of(^^RulesType)) {
                    using StateEnum = typename [: (std::meta::type_of(mem)) :];
                    rules.[:mem:] = static_cast<StateEnum>(chosen_target);
                }
            }
        }

        // --- mapping + storage_key: store value into map (post-transition) ---
        // Reuse mapping_fields_arr (0 or 1 element) as the lazy guard.
        template for (constexpr auto mf : mapping_fields_arr) {
            using MapType   = typename [: std::meta::type_of(mf) :];
            using ValueType = typename MapType::mapped_type;

            static constexpr auto params = std::define_static_array(std::meta::parameters_of(method));
            static constexpr auto value_members = std::define_static_array(
                std::meta::members_of(^^ValueType, std::meta::access_context::current()));

            auto args_tuple = std::forward_as_tuple(args...);
            auto key = std::string(std::get<safe_key_idx>(args_tuple));

            // Build the value by default-init then assigning each field whose name matches a param.
            ValueType val{};
            template for (constexpr auto vm : value_members) {
                if constexpr (std::meta::is_nonstatic_data_member(vm) && std::meta::has_identifier(vm)) {
                    constexpr std::string_view field_name = std::meta::identifier_of(vm);
                    // Compute which param index has this field name (excluding storage_key param).
                    static constexpr size_t matched_param_idx = []() consteval -> size_t {
                        auto ps = std::meta::parameters_of(method);
                        for (size_t i = 0; i < ps.size(); ++i) {
                            auto p = ps[i];
                            if (p == storage_key_param) continue;
                            if (std::meta::has_identifier(p) && std::meta::identifier_of(p) == field_name)
                                return i;
                        }
                        return size_t(-1);
                    }();
                    if constexpr (matched_param_idx != size_t(-1)) {
                        static constexpr size_t safe_param_idx = (matched_param_idx == size_t(-1)) ? 0 : matched_param_idx;
                        val.[:vm:] = static_cast<typename [: std::meta::type_of(vm) :]>(
                            std::get<safe_param_idx>(args_tuple));
                    }
                }
            }

            rules.[:mf:].emplace(std::move(key), std::move(val));
        }



        return {};
    }

};

template <typename FSM, typename RulesType, std::meta::info method, ptrdiff_t offset>
struct AccessorProxyMethod {
    const auto& operator()() const {
        auto& fsm_proxy = *reinterpret_cast<const FSM*>(reinterpret_cast<const char*>(this) + offset);
        return fsm_proxy.__rules__.[:method:];
    }
};

template <typename FSM, typename RulesType, std::meta::info state_mem, ptrdiff_t offset, fixed_state_rules RulesData>
struct IsFinalProxyMethod {
    auto operator()() const {
        auto& fsm_proxy = *reinterpret_cast<const FSM*>(reinterpret_cast<const char*>(this) + offset);
        uint32_t current_state = static_cast<uint32_t>(fsm_proxy.__rules__.[:state_mem:]);
        for (uint32_t i = 0; i < RulesData.num_final_states; ++i) {
            if (current_state == RulesData.final_states[i].state) return true;
        }
        return false;
    }
};

template <typename FSM, typename RulesType, std::meta::info state_mem, ptrdiff_t offset>
struct IsInitedProxyMethod {
    auto operator()() const {
        auto& fsm_proxy = *reinterpret_cast<const FSM*>(reinterpret_cast<const char*>(this) + offset);
        return static_cast<uint32_t>(fsm_proxy.__rules__.[:state_mem:]) != 0;
    }
};

} // namespace detail

template <typename Definition>
consteval auto generate_state_machine_type() {
    struct GeneratedStateMachine;
    
    consteval {
        std::vector<std::meta::info> membersOfInterest;
        membersOfInterest.push_back(std::meta::data_member_spec(^^Definition, {.name = "__rules__"}));

        struct dummy {
            alignas(alignof(Definition)) char __rules__[sizeof(Definition)];
        };
        constexpr ptrdiff_t objectOffset = -offsetof(dummy, __rules__);

        static constexpr auto definition_type = ^^Definition;
        static constexpr auto members = std::define_static_array(std::meta::members_of(definition_type, std::meta::access_context::current()));
        
        static constexpr std::meta::info enum_type = []() {
            auto mems = std::meta::members_of(definition_type, std::meta::access_context::current());
            for (auto m : mems) {
                if (std::meta::is_nonstatic_data_member(m) && std::meta::has_identifier(m)) {
                    if (std::meta::identifier_of(m) == std::meta::identifier_of(definition_type)) {
                        return std::meta::type_of(m);
                    }
                }
            }
            return ^^void;
        }();
        
        if constexpr (enum_type == ^^void) throw "State tracking field must exist and match definition struct name";
        
        static constexpr auto enumerators = std::define_static_array(std::meta::enumerators_of(enum_type));
        if constexpr (enumerators.size() == 0) throw "State enum must not be empty";
        if constexpr (std::meta::identifier_of(enumerators[0]) != "uninited") throw "First state must be 'uninited'";
        if constexpr (static_cast<int>(std::meta::extract<typename [: (enum_type) :]>(enumerators[0])) != 0) throw "'uninited' state must have value 0";

        // Discover the 'event' enum (nested enum class named "event" inside Definition)
        static constexpr std::meta::info event_enum_type = []() {
            auto mems = std::meta::members_of(definition_type, std::meta::access_context::current());
            for (auto m : mems) {
                if (std::meta::is_type(m) && std::meta::has_identifier(m)) {
                    if (std::meta::identifier_of(m) == "event") {
                        return m; // info for the enum type itself
                    }
                }
            }
            return ^^void;
        }();

        constexpr std::meta::info state_mem = []() {
            auto mems = std::meta::members_of(definition_type, std::meta::access_context::current());
            for (auto m : mems) {
                if (std::meta::is_nonstatic_data_member(m) && std::meta::has_identifier(m)) {
                    if (std::meta::identifier_of(m) == std::meta::identifier_of(definition_type)) {
                        return m;
                    }
                }
            }
            return ^^void;
        }();

        constexpr detail::fixed_state_rules rules_data = []() {
            detail::fixed_state_rules rules{};
            auto es = std::meta::enumerators_of(enum_type);
            template for (constexpr auto mem : members) {
                if constexpr (std::meta::is_function(mem) && std::meta::has_identifier(mem)) {
                    constexpr std::string_view name = std::meta::identifier_of(mem);
                    
                    constexpr bool is_initializer = []() {
                        bool found = false;
                        static constexpr auto annos = std::define_static_array(std::meta::annotations_of(mem));
                        template for (constexpr auto a : annos) {
                            if constexpr (detail::is_initial<std::remove_cvref_t<typename [: std::meta::type_of(a) :]>>::value) {
                                found = true;
                            }
                        }
                        return found;
                    }();

                    if constexpr (is_initializer) {
                        constexpr std::string_view state_name = name.starts_with("to_") ? name.substr(3) : name;
                        uint32_t state_val = 0;
                        for (auto e : es) {
                            if (std::meta::identifier_of(e) == state_name) {
                                state_val = static_cast<uint32_t>(std::meta::extract<typename [: enum_type :]>(e));
                                break;
                            }
                        }
                        
                        constexpr auto ret_type = std::meta::return_type_of(mem);
                        constexpr auto targets = detail::extract_targets<std::remove_cvref_t<typename [: ret_type :]>>::get();
                        uint32_t state_idx = rules.num_transitions++;
                        rules.transitions[state_idx].state = state_val;
                        for (size_t i = 0; i < targets.size; ++i) rules.transitions[state_idx].targets[rules.transitions[state_idx].num_targets++] = targets.data[i];
                    } else if constexpr (name.starts_with("to_")) {
                        constexpr std::string_view state_name = name.substr(3);
                        uint32_t state_val = 0;
                        bool found_state = false;
                        for (auto e : es) {
                            if (std::meta::identifier_of(e) == state_name) {
                                state_val = static_cast<uint32_t>(std::meta::extract<typename [: enum_type :]>(e));
                                found_state = true;
                                break;
                            }
                        }
                        if (found_state) {
                            char err_buf[128]{};
                            size_t err_len = 0;
                            static constexpr auto mem_annos = std::define_static_array(std::meta::annotations_of(mem));
                            template for (constexpr auto anno : mem_annos) {
                                using AnnoType = std::remove_cvref_t<typename [: std::meta::type_of(anno) :]>;
                                if constexpr (detail::is_on_error<AnnoType>::value) {
                                    constexpr auto err_obj = std::meta::extract<AnnoType>(anno);
                                    err_len = err_obj.message().size();
                                    for (size_t i = 0; i < err_len; ++i) err_buf[i] = err_obj.message()[i];
                                }
                            }
                            
                            constexpr auto ret_type = std::meta::return_type_of(mem);
                            if constexpr (detail::is_final_state<std::remove_cvref_t<typename [: ret_type :]>>::value) {
                                uint32_t idx = rules.num_final_states++;
                                rules.final_states[idx].state = state_val;
                                for (size_t i = 0; i < err_len; ++i) rules.final_states[idx].error_msg[i] = err_buf[i];
                                rules.final_states[idx].error_msg_len = err_len;
                            } else {
                                constexpr auto targets = detail::extract_targets<std::remove_cvref_t<typename [: ret_type :]>>::get();
                                uint32_t state_idx = rules.num_transitions++;
                                rules.transitions[state_idx].state = state_val;
                                for (size_t i = 0; i < err_len; ++i) rules.transitions[state_idx].error_msg[i] = err_buf[i];
                                rules.transitions[state_idx].error_msg_len = err_len;
                                for (size_t i = 0; i < targets.size; ++i) rules.transitions[state_idx].targets[rules.transitions[state_idx].num_targets++] = targets.data[i];
                            }
                        }
                    } else if constexpr (std::meta::has_identifier(std::meta::return_type_of(mem)) &&
                                         std::meta::identifier_of(std::meta::return_type_of(mem)) == "on") {
                        // Validate that the event handler name matches an enumerator in the 'event' enum
                        if constexpr (event_enum_type != ^^void) {
                            static constexpr std::string_view event_name = std::meta::identifier_of(mem);
                            constexpr bool event_name_valid = []() {
                                auto event_es = std::meta::enumerators_of(event_enum_type);
                                for (auto e : event_es) {
                                    if (std::meta::identifier_of(e) == event_name) return true;
                                }
                                return false;
                            }();
                            if constexpr (!event_name_valid) {
                                throw "Event handler name does not match any enumerator in the 'event' enum";
                            }
                        }
                    }
                }
            }
            return rules;
        }();

        membersOfInterest.push_back(std::meta::data_member_spec(
            ^^detail::IsFinalProxyMethod<GeneratedStateMachine, Definition, state_mem, objectOffset, rules_data>,
            {.name = "is_final", .no_unique_address = true}
        ));
        membersOfInterest.push_back(std::meta::data_member_spec(
            ^^detail::IsInitedProxyMethod<GeneratedStateMachine, Definition, state_mem, objectOffset>,
            {.name = "is_inited", .no_unique_address = true}
        ));

        template for (constexpr auto mem : members) {
            if constexpr (std::meta::is_function(mem) && std::meta::has_identifier(mem)) {
                constexpr std::string_view name = std::meta::identifier_of(mem);
                static constexpr auto annos = std::define_static_array(std::meta::annotations_of(mem));
                
                constexpr bool is_initializer = []() {
                    bool found = false;
                    template for (constexpr auto a : annos) {
                        if constexpr (detail::is_initial<std::remove_cvref_t<typename [: (std::meta::type_of(a)) :]>>::value) {
                            found = true;
                        }
                    }
                    return found;
                }();

                if constexpr (is_initializer && name.starts_with("to_")) {
                    constexpr std::string_view state_name = name.substr(3);
                    constexpr uint32_t target_val = []() {
                        auto es = std::meta::enumerators_of(enum_type);
                        for (auto e : es) {
                            if (std::meta::identifier_of(e) == state_name) {
                                return static_cast<uint32_t>(std::meta::extract<typename [: (enum_type) :]>(e));
                            }
                        }
                        return 0u;
                    }();
                    
                    membersOfInterest.push_back(std::meta::data_member_spec(
                        ^^detail::InitialProxyMethod<GeneratedStateMachine, Definition, mem, objectOffset, target_val>,
                        {.name = "initialize", .no_unique_address = true}
                    ));
                } else if constexpr (std::meta::has_identifier(std::meta::return_type_of(mem)) && std::meta::identifier_of(std::meta::return_type_of(mem)) == "on") {
                    membersOfInterest.push_back(std::meta::data_member_spec(
                        ^^detail::EventProxyMethod<GeneratedStateMachine, Definition, mem, objectOffset, rules_data, state_mem>,
                        {.name = name, .no_unique_address = true}
                    ));
                }
            } else if constexpr (std::meta::is_nonstatic_data_member(mem) && std::meta::has_identifier(mem)) {
                constexpr std::string_view name = std::meta::identifier_of(mem);
                membersOfInterest.push_back(std::meta::data_member_spec(
                    ^^detail::AccessorProxyMethod<GeneratedStateMachine, Definition, mem, objectOffset>,
                    {.name = name, .no_unique_address = true}
                ));
            }
        }

        std::meta::define_aggregate(^^GeneratedStateMachine, membersOfInterest);
    }
    return GeneratedStateMachine{};
}

template <typename T>
using build_state_machine_type = decltype(generate_state_machine_type<T>());

} // namespace yggdrasil

#endif // YGGDRASIL_STATE_MACHINE_HPP
