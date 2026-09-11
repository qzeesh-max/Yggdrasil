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
// Shared order_state definition used by test_order and test_contextualized.

#pragma once

#include <unordered_map>
#include <string>
#include <string_view>
#include <cstdint>
#include <yggdrasil/state_machine.hpp>

using namespace yggdrasil;

struct order_state : state_machine
{
    enum class state
    {
        uninited,
        order_received,
        open,
        partially_filled,
        filled,
        order_rejected,
        order_canceled,
    };
    
    enum class event
    {
        new_order,
        replaced,
        trade,
        bust_trade,
        cancel,
        rejected,
        replaced_rejected,
        cancel_rejected,
        cancel_sent,
        replace_sent,
    };
    
    enum type_t
    {
        market,
        limit,
        regular_peg,
        mid_peg,
        market_peg,
    };
    
    state order_state {};
    std::string symbol;
    uint32_t orderSize{};
    double orderPrice{};
    type_t type{};
    double pegOffset{};
    
    [[=init_from<^^orderSize>{}]]
    uint32_t leavesQty{};
    
    [[=init_val<0>{}]]
    uint32_t cumQty{};
    
    [[=init_val<0.0>{}]]
    double avgPx{};
    
    std::string rejectText;
    
    [[=initial{}]]
    auto to_order_received(std::string_view symbol, uint32_t orderSize, double orderPrice,
                           type_t type, double pegOffset) -> any_of<state::open, state::order_rejected>;

    [[=on_error("Order already open")]]
    auto to_open() -> any_of<state::order_canceled, state::partially_filled, state::filled>;
    
    auto to_order_canceled() -> final;
    auto to_partially_filled() -> any_of<state::order_canceled, state::partially_filled, state::filled>;
    
    [[=on_error("Order already filled")]]
    auto to_filled() -> final;
    
    auto to_order_rejected() -> final;
    
    [[=transition(state::open)]]
    on new_order(double price)
    {
        orderPrice = price;
        return accepted;
    }
    
    [[=transition(state::order_rejected)]]
    on rejected(std::string_view text)
    {
        rejectText = text;
        return accepted;
    }
    
    [[=transition(state::order_canceled)]]
    on cancel()
    {
        return accepted;
    }

    struct trade_data_t {
        uint32_t fillQty {};
        double fillPx {};
    };

    struct hasher
    {
        using is_transparent = std::true_type;

        size_t operator()(std::string_view sv) const {
            return std::hash<std::string_view>{}(sv);
        }

        size_t operator()(const std::string& s) const {
            return std::hash<std::string>{}(s);
        }

        size_t operator()(const char* s) const {
            return std::hash<std::string_view>{}(s);
        }
    };

    using trades_t = std::unordered_map<std::string, trade_data_t, hasher, std::equal_to<>>;

    trades_t trades;
    
    [[=transition(any_of<state::partially_filled, state::filled>{})]]\
    [[=mapping<^^trades>{}]]
    [[=on_error("Duplicate Trade ID")]]
    on trade([[=storage_key{}]]std::string_view tradeId, uint32_t fillQty, double fillPx)
    {
        avgPx = (avgPx * cumQty + fillQty * fillPx) / (fillQty + cumQty);
        leavesQty -= fillQty;
        cumQty += fillQty;
        return leavesQty ? to(state::partially_filled) : to(state::filled);
    }

    [[=can_revert_final{}]]
    [[=transition(any_of<state::partially_filled, state::open, state::order_canceled>{})]]\
    [[=on_error("Trade to bust not found")]]
    on bust_trade(std::string_view tradeId)
    {
        if (auto it = trades.find(tradeId); it != trades.end()) {            
            avgPx = (avgPx * cumQty - it->second.fillQty * it->second.fillPx) / (cumQty - it->second.fillQty);            
            cumQty -= it->second.fillQty;
            trades.erase(it);

            if (order_state == state::filled) {
                return to(state::order_canceled);
            } else if (order_state == state::partially_filled) {
                if (cumQty != 0)
                {
                    return to(state::partially_filled);
                }
                return to(state::open);
            }
            return to(order_state);
        }
        return state_machine::rejected;
    }
};
