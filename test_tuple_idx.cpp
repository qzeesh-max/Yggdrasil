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
#include <unordered_map>
#include <tuple>
#include <type_traits>

struct Rules {
    int trade_bust(std::unordered_map<int, int>& trades, std::string_view execID) {
        return 0;
    }
};

struct FSM {
    Rules __rules__;
    std::unordered_map<int, int> trades;
};

template <std::meta::info method, std::meta::info FSMType, size_t ParamIdx>
consteval size_t get_tuple_idx() {
    size_t current_param_idx = 0;
    size_t tuple_idx = 0;
    static constexpr auto params = std::define_static_array(std::meta::parameters_of(method));
    template for (constexpr auto p : params) {
        if (current_param_idx == ParamIdx) return tuple_idx;
        constexpr bool is_p_ref = std::is_reference_v<typename [: std::meta::type_of(p) :]>;
        bool has_field = false;
        if constexpr (is_p_ref && std::meta::has_identifier(p)) {
            static constexpr auto mems = std::define_static_array(std::meta::members_of(FSMType, std::meta::access_context::current()));
            template for (constexpr auto m : mems) {
                if constexpr (std::meta::is_nonstatic_data_member(m) && std::meta::has_identifier(m)) {
                    if constexpr (std::meta::identifier_of(m) == std::meta::identifier_of(p)) has_field = true;
                }
            }
        }
        if (!has_field) tuple_idx++;
        current_param_idx++;
    }
    return tuple_idx;
}

int main() {
    constexpr auto mems = std::define_static_array(std::meta::members_of(^^Rules, std::meta::access_context::current()));
    constexpr auto method = mems[0];
    constexpr size_t idx = get_tuple_idx<method, ^^FSM, 1>();
    std::cout << idx << "\n";
}
