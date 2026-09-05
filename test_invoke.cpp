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

struct Rules {
    int trade_bust(std::unordered_map<int, int>& trades, std::string_view execID) {
        std::cout << "trade_bust called with execID=" << execID << ", trades size=" << trades.size() << "\n";
        return 0;
    }
    
    int trade(std::string_view execID, double price) {
        std::cout << "trade called with " << execID << ", " << price << "\n";
        return 1;
    }
};

struct FSM {
    Rules __rules__;
    std::unordered_map<int, int> trades;
};

consteval int get_tuple_index(std::meta::info method, size_t param_idx) {
    auto params = std::meta::parameters_of(method);
    int tuple_idx = 0;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i == param_idx) {
            auto p = params[i];
            if (std::meta::type_of(p) == ^^std::unordered_map<int, int>&) {
                // Actually we should check if it's a reference and if name matches FSM field
                return -1;
            }
            return tuple_idx;
        }
        auto p = params[i];
        if (std::meta::type_of(p) != ^^std::unordered_map<int, int>&) {
            tuple_idx++;
        }
    }
    return -1;
}

consteval std::meta::info get_fsm_field(std::meta::info fsm_type, std::meta::info method, size_t param_idx) {
    auto params = std::meta::parameters_of(method);
    auto p = params[param_idx];
    auto fsm_mems = std::meta::members_of(fsm_type, std::meta::access_context::current());
    for (auto m : fsm_mems) {
        if (std::meta::is_nonstatic_data_member(m) && std::meta::has_identifier(m) && std::meta::identifier_of(m) == std::meta::identifier_of(p)) {
            return m;
        }
    }
    return ^^void;
}

template <typename FSMType, std::meta::info method, size_t PI, typename TupleArgs>
decltype(auto) get_arg(FSMType& fsm, TupleArgs&& tuple_args) {
    constexpr int ti = get_tuple_index(method, PI);
    if constexpr (ti == -1) {
        constexpr std::meta::info fsm_field = get_fsm_field(^^FSMType, method, PI);
        return (fsm.[:fsm_field:]);
    } else {
        return std::get<ti>(tuple_args);
    }
}

template <typename FSMType, std::meta::info method, typename TupleArgs, size_t... PIs>
decltype(auto) invoke_helper(FSMType& fsm, TupleArgs&& tuple_args, std::index_sequence<PIs...>) {
    return fsm.__rules__.[:method:]( get_arg<FSMType, method, PIs, TupleArgs>(fsm, tuple_args)... );
}

int main() {
    FSM fsm;
    fsm.trades[1] = 100;
    
    constexpr auto mems = std::meta::members_of(^^Rules, std::meta::access_context::current());
    constexpr auto trade_bust_method = mems[0]; // assuming trade_bust is first
    
    auto args = std::make_tuple("exec1");
    invoke_helper<FSM, trade_bust_method>(fsm, args, std::make_index_sequence<2>{});
}
