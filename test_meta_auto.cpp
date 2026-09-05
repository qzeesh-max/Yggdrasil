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
#include <string_view>
#include <vector>

struct Rule {
    void trade_bust(auto& trades, std::string_view refExecID) {}
};

int main() {
    constexpr auto members = std::meta::members_of(^^Rule, std::meta::access_context::current());
    for (auto mem : members) {
        if (std::meta::has_identifier(mem) && std::meta::identifier_of(mem) == "trade_bust") {
            std::cout << "Found trade_bust\n";
            std::cout << "Is template: " << std::meta::is_template(mem) << "\n";
            if (std::meta::is_function_template(mem)) {
                auto params = std::meta::parameters_of(mem);
                for (auto p : params) {
                    std::cout << "Param: " << std::meta::identifier_of(p) << "\n";
                }
            }
        }
    }
}
