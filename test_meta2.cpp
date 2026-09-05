#include <yggdrasil/state_machine.hpp>
#include <iostream>
#include <meta>
#include <type_traits>

using namespace yggdrasil;

struct my_state : state_machine {
    [[=initial{}]]
    auto to_a() -> void;
};

consteval void print_annos() {
    static constexpr auto mems = std::define_static_array(std::meta::members_of(^^my_state, std::meta::access_context::current()));
    template for (constexpr auto mem : mems) {
        if constexpr (std::meta::has_identifier(mem) && std::meta::identifier_of(mem) == "to_a") {
            static constexpr auto annos = std::define_static_array(std::meta::annotations_of(mem));
            template for (constexpr auto a : annos) {
                constexpr bool is_init = detail::is_initial<std::remove_cvref_t<typename [: std::meta::type_of(a) :]>>::value;
                static_assert(is_init, "is_init should be true");
            }
        }
    }
}

int main() { print_annos(); }
