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

#include <iostream>
#include <string>
#include <unordered_map>
#include <meta>
#include <vector>

template <typename T>
struct store_event {};

struct trade_store_t;

struct order_state {
    [[store_event<trade_store_t>{}]]
    void trade(int x, double y) {}
};

consteval void generate_stuff() {
    constexpr auto mems = std::meta::members_of(^^order_state);
    constexpr auto trade_mem = mems[0];
    constexpr auto annos = std::meta::annotations_of(trade_mem);
    
    std::meta::info store_event_type = ^^void;
    for (auto a : annos) {
        if (std::meta::has_template_arguments(std::meta::type_of(a)) && std::meta::template_of(std::meta::type_of(a)) == ^^store_event) {
            store_event_type = std::meta::template_arguments_of(std::meta::type_of(a))[0];
        }
    }
    
    if (store_event_type != ^^void) {
        std::vector<std::meta::data_member_spec> payload_members;
        payload_members.push_back(std::meta::data_member_spec(^^int, {.name = "x"}));
        payload_members.push_back(std::meta::data_member_spec(^^double, {.name = "y"}));
        std::meta::define_aggregate(store_event_type, payload_members);
    }
}

int main() {
    generate_stuff();
    trade_store_t t{.x = 1, .y = 2.0};
    std::cout << t.x << " " << t.y << std::endl;
}
