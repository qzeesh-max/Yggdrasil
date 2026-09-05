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

#include <meta>
#include <iostream>
#include <string>

namespace yggdrasil {
    struct state_machine {
        template <typename Key>
        class collection_ref {};
    };
}

struct Foo {
    void method(yggdrasil::state_machine::collection_ref<std::string> trades, int x);
};

int main() {
    constexpr auto mems = std::define_static_array(std::meta::members_of(^^Foo, std::meta::access_context::current()));
    constexpr auto method = mems[0];
    constexpr auto params = std::define_static_array(std::meta::parameters_of(method));
    constexpr auto p = params[0];

    constexpr bool is_col_ref = []() {
        auto t = std::meta::type_of(p);
        if (t != ^^void) {
            auto t2 = std::meta::remove_cvref(t);
            if (std::meta::has_template_arguments(t2)) {
                if (std::meta::template_of(t2) == ^^yggdrasil::state_machine::collection_ref) return true;
            }
        }
        return false;
    }();
    std::cout << "is_col_ref: " << is_col_ref << "\n";
}
