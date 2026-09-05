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

template <int ID>
struct Payload;

consteval void generate_payload() {
    std::vector<std::meta::data_member_spec> members;
    members.push_back(std::meta::data_member_spec(^^int, {.name = "qty"}));
    members.push_back(std::meta::data_member_spec(^^double, {.name = "price"}));
    std::meta::define_aggregate(^^Payload<1>, members);
}

int main() {
    generate_payload();
    Payload<1> p{10, 15.5};
    std::cout << p.qty << " " << p.price << "\n";
}
