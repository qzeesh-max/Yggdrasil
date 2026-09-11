#ifndef YGGDRASIL_FOR_EACH_HPP
#define YGGDRASIL_FOR_EACH_HPP

#include <meta>
#include "state_root.hpp"
#include <string_view>

namespace yggdrasil {

namespace detail {

// Helper: Annotations... are the individual std::meta::info values passed as
// non-type template parameters (structural type). All std::meta::extract calls
// happen during template instantiation, so func is never required to be
// constexpr.
template<std::meta::info... Annotations, typename T, typename Member, typename Func>
void dispatch_with_annotations(
    T& obj,
    std::string_view member_name,
    Member& member_value,
    Func&& func)
{
    func(obj, member_name, member_value,
        std::meta::extract< typename [: std::meta::type_of(Annotations) :] >(Annotations)...);
}

// const overload — same logic, T and Member are const-qualified by the caller.
template<std::meta::info... Annotations, typename T, typename Member, typename Func>
void dispatch_with_annotations_const(
    const T& obj,
    std::string_view member_name,
    const Member& member_value,
    Func&& func)
{
    func(obj, member_name, member_value,
        std::meta::extract< typename [: std::meta::type_of(Annotations) :] >(Annotations)...);
}

} // namespace detail

// Non-const overload: allows func to mutate the object and its fields.
template<typename T, typename Func>
void for_each(T& obj, Func&& func) {
    constexpr static const auto members = std::define_static_array(
        std::meta::nonstatic_data_members_of(^^T,
            std::meta::access_context::current()));

    template for (constexpr const auto& member : members)
    {
        if constexpr (std::meta::has_identifier(member))
        {
            constexpr static const auto annotations = std::define_static_array(std::meta::annotations_of(member));

            bool already_dispatched = false;

            template for (constexpr const auto& annotation : annotations)
            {
                // Flatten nested state roots (state_root-annotated members are
                // themselves state machines — recurse into them).
                if constexpr(std::meta::type_of(annotation) == ^^const state_root)
                {
                    for_each(obj.[:member:], func);
                    already_dispatched = true;
                    break;
                }
            }

            if (!already_dispatched)
            {
                std::string_view member_name = std::meta::identifier_of(member);
                auto& member_value = obj.[:member:];

                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    detail::dispatch_with_annotations<annotations[Is]...>(
                        obj, member_name, member_value, func);
                }(std::make_index_sequence<annotations.size()>{});
            }
        }
    }
}

// Const overload: for read-only traversal (e.g., serialization).
template<typename T, typename Func>
void for_each(const T& obj, Func&& func) {
    constexpr static const auto members = std::define_static_array(
        std::meta::nonstatic_data_members_of(^^T,
            std::meta::access_context::current()));

    template for (constexpr const auto& member : members)
    {
        if constexpr (std::meta::has_identifier(member))
        {
            constexpr static const auto annotations = std::define_static_array(std::meta::annotations_of(member));

            bool already_dispatched = false;

            template for (constexpr const auto& annotation : annotations)
            {
                if constexpr(std::meta::type_of(annotation) == ^^const state_root)
                {
                    for_each(obj.[:member:], func);
                    already_dispatched = true;
                    break;
                }
            }

            if (!already_dispatched)
            {
                std::string_view member_name = std::meta::identifier_of(member);
                const auto& member_value = obj.[:member:];

                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    detail::dispatch_with_annotations_const<annotations[Is]...>(
                        obj, member_name, member_value, func);
                }(std::make_index_sequence<annotations.size()>{});
            }
        }
    }
}

} // namespace yggdrasil

#endif // !defined(YGGDRASIL_FOR_EACH_HPP)