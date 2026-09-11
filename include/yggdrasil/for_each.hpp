#ifndef YGGDRASIL_FOR_EACH_HPP
#define YGGDRASIL_FOR_EACH_HPP

#include <meta>
#include "state_root.hpp"
#include <iostream>

namespace yggdrasil {

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

            static constexpr const auto name = std::meta::identifier_of(member);

            if (!already_dispatched)
            {
                func(obj, std::meta::identifier_of(member), obj.[:member:]);
            }
        }
    }
}

} // namespace yggdrasil

#endif // !defined(YGGDRASIL_FOR_EACH_HPP)