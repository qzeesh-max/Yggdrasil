#ifndef YGGDRASIL_FOR_EACH_HPP
#define YGGDRASIL_FOR_EACH_HPP

#include <meta>
#include "state_root.hpp"
#include <iostream>
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

} // namespace detail

template<typename T, typename Func>
void for_each(T& obj, Func && func) {
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
                // we must flatten state if we see this, because of how our
                // proxies are generated.
                if constexpr(std::meta::type_of(annotation) == ^^const state_root)
                {
                    for_each(obj.[:member:], func);
                    already_dispatched = true;
                    break;
                }    
            }

            if (!already_dispatched)
            {
                // Capture consteval results as plain runtime values first.
                std::string_view member_name = std::meta::identifier_of(member);
                auto& member_value = obj.[:member:];

                // Expand the annotations span into individual NTTP values by
                // using an index_sequence + lambda to instantiate the helper.
                // std::meta::info is a structural type, so each element can be
                // passed as a non-type template parameter directly.
                [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    detail::dispatch_with_annotations<annotations[Is]...>(
                        obj, member_name, member_value, func);
                }(std::make_index_sequence<annotations.size()>{});
            }
        }
    }
}

} // namespace yggdrasil

#endif // !defined(YGGDRASIL_FOR_EACH_HPP)