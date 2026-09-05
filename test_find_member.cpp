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
        static consteval std::meta::info find_data_member(std::meta::info t, std::string_view name) {
            for (auto m : std::meta::members_of(t, std::meta::access_context::current())) {
                if (std::meta::is_nonstatic_data_member(m) && std::meta::has_identifier(m) && std::meta::identifier_of(m) == name) {
                    return m;
                }
            }
            for (auto b : std::meta::bases_of(t, std::meta::access_context::current())) {
                auto res = find_data_member(std::meta::type_of(b), name);
                if (res != ^^void) return res;
            }
            return ^^void;
        }
    };
}

struct MapBase {
    int trades;
};

struct FSMClass {};

template <typename... Maps>
struct GeneratedStateMachine : public FSMClass, public Maps... {};

struct order_state {
    void trade_bust(int trades);
};

int main() {
    using FSMType = GeneratedStateMachine<MapBase>;
    constexpr auto mem = yggdrasil::state_machine::find_data_member(^^FSMType, "trades");
    constexpr bool is_found = (mem != ^^void);
    std::cout << "mem found: " << is_found << "\n";
}
