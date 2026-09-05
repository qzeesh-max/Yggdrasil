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

#include <unordered_map>
#include <yggdrasil/state_machine.hpp>
#include <string>
#include <string_view>
#include <cstdint>
#include <gtest/gtest.h>

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
    
    [[=transition(any_of<state::partially_filled, state::filled>{})]]
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
    [[=transition(any_of<state::partially_filled, state::open, state::order_canceled>{})]]
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

TEST(OrderStateMachineTest, InitialStateAndAccessors) {
    auto fsm = build_state_machine_type<order_state>{};
    
    // Initializer method called
    fsm.initialize("AAPL", 100, 150.5, order_state::limit, 0.0);
    
    // Check state using accessor
    EXPECT_EQ(fsm.order_state(), order_state::state::order_received);
    EXPECT_EQ(fsm.symbol(), "AAPL");
    EXPECT_EQ(fsm.orderSize(), 100);
}

TEST(OrderStateMachineTest, NewOrderEvent) {
    auto fsm = build_state_machine_type<order_state>{};
    fsm.initialize("AAPL", 100, 150.5, order_state::limit, 0.0);
    
    auto result = fsm.new_order(155.0);
    EXPECT_TRUE(result.has_value());
    
    EXPECT_EQ(fsm.order_state(), order_state::state::open);
    EXPECT_EQ(fsm.orderPrice(), 155.0);
}

TEST(OrderStateMachineTest, TradeFormula) {
    auto fsm = build_state_machine_type<order_state>{};
    fsm.initialize("AAPL", 100, 150.5, order_state::limit, 0.0);
    EXPECT_TRUE(fsm.new_order(150.5)); // moves to open
    
    auto res1 = fsm.trade("ID1", 40, 150.0);
    EXPECT_TRUE(res1.has_value());
    EXPECT_EQ(fsm.order_state(), order_state::state::partially_filled);
    EXPECT_EQ(fsm.leavesQty(), 60);
    EXPECT_EQ(fsm.cumQty(), 40);
    EXPECT_EQ(fsm.avgPx(), 150.0);
    
    auto res2 = fsm.trade("ID2", 60, 160.0);
    EXPECT_TRUE(res2.has_value());
    EXPECT_EQ(fsm.order_state(), order_state::state::filled);
    EXPECT_EQ(fsm.leavesQty(), 0);
    EXPECT_EQ(fsm.cumQty(), 100);
    EXPECT_EQ(fsm.avgPx(), 156.0); // (40*150 + 60*160) / 100
}

TEST(OrderStateMachineTest, InvalidTransitions) {
    auto fsm = build_state_machine_type<order_state>{};
    
    // Test that an event allowed only in state::open is rejected in uninited
    auto res1 = fsm.trade("ID1", 40, 150.0);
    EXPECT_FALSE(res1.has_value());
    EXPECT_EQ(res1.error(), "Invalid transition from current state"); // default error since uninited has no on_error

    fsm.initialize("AAPL", 100, 150.5, order_state::limit, 0.0);
    
    // In state uninited (after to_order_received, before new_order), trade should still fail
    auto res2 = fsm.trade("ID2", 40, 150.0);
    EXPECT_FALSE(res2.has_value());

    // Move to open
    EXPECT_TRUE(fsm.new_order(150.5));
    
    // In state open, trade should succeed
    auto res3 = fsm.trade("ID1", 40, 150.0);
    EXPECT_TRUE(res3.has_value());
    EXPECT_EQ(fsm.order_state(), order_state::state::partially_filled);
    
    // In state partially_filled, trade to filled
    auto res4 = fsm.trade("ID2", 60, 160.0);
    EXPECT_TRUE(res4.has_value());
    EXPECT_EQ(fsm.order_state(), order_state::state::filled);
    
    // In state filled, any event should be rejected with the custom error
    auto res5 = fsm.trade("ID3", 10, 150.0);
    EXPECT_FALSE(res5.has_value());
    EXPECT_EQ(res5.error(), "Order already filled");
}

TEST(OrderStateMachineTest, StateHelpers) {
    auto fsm = build_state_machine_type<order_state>{};
    
    EXPECT_FALSE(fsm.is_inited());
    EXPECT_FALSE(fsm.is_final());

    fsm.initialize("AAPL", 100, 150.5, order_state::limit, 0.0);
    
    // Now it is inited because the initial setup moved the state to order_received
    EXPECT_TRUE(fsm.is_inited());
    EXPECT_FALSE(fsm.is_final());

    EXPECT_TRUE(fsm.new_order(150.5)); // moves to open
    
    EXPECT_TRUE(fsm.is_inited());
    EXPECT_FALSE(fsm.is_final());

    EXPECT_TRUE(fsm.trade("ID1", 100, 160.0)); // moves to filled
    
    EXPECT_TRUE(fsm.is_inited());
    EXPECT_TRUE(fsm.is_final());
}

TEST(OrderStateMachineTest, TradeMapping) {
    auto fsm = build_state_machine_type<order_state>{};
    fsm.initialize("AAPL", 100, 150.0, order_state::limit, 0.0);
    EXPECT_TRUE(fsm.new_order(150.0)); // open

    // First trade fills 40 shares
    auto res1 = fsm.trade("T001", 40, 151.0);
    EXPECT_TRUE(res1.has_value());
    EXPECT_EQ(fsm.order_state(), order_state::state::partially_filled);

    // The trade should be stored in the map
    auto& trades = fsm.trades();
    ASSERT_EQ(trades.size(), 1u);
    ASSERT_NE(trades.find("T001"), trades.end());
    EXPECT_EQ(trades.at("T001").fillQty, 40u);
    EXPECT_DOUBLE_EQ(trades.at("T001").fillPx, 151.0);

    // Duplicate trade ID should be rejected with the on_error message
    auto res_dup = fsm.trade("T001", 20, 152.0);
    EXPECT_FALSE(res_dup.has_value());
    EXPECT_EQ(res_dup.error(), "Duplicate Trade ID");
    EXPECT_EQ(fsm.order_state(), order_state::state::partially_filled);
    EXPECT_EQ(fsm.leavesQty(), 60);
    EXPECT_EQ(fsm.cumQty(), 40);
    EXPECT_EQ(fsm.avgPx(), 151.0);

    // A different ID goes through fine
    auto res2 = fsm.trade("T002", 60, 152.0);
    EXPECT_TRUE(res2.has_value());
    EXPECT_EQ(fsm.order_state(), order_state::state::filled);
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades.at("T002").fillQty, 60u);
    EXPECT_DOUBLE_EQ(trades.at("T002").fillPx, 152.0);
    
    // bust a trade
    auto res3 = fsm.bust_trade("T001");
    EXPECT_TRUE(res3.has_value());
    EXPECT_EQ(fsm.order_state(), order_state::state::order_canceled);
    EXPECT_EQ(fsm.leavesQty(), 0);
    EXPECT_EQ(fsm.cumQty(), 60);
    EXPECT_EQ(fsm.avgPx(), 152.0);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades.find("T001"), trades.end());   

    // try busting the trade again
    auto res4 = fsm.bust_trade("T001");
    EXPECT_FALSE(res4.has_value());
    EXPECT_EQ(res4.error(), "Trade to bust not found");
    EXPECT_EQ(fsm.order_state(), order_state::state::order_canceled);
    EXPECT_EQ(fsm.leavesQty(), 0);
    EXPECT_EQ(fsm.cumQty(), 60);
    EXPECT_EQ(fsm.avgPx(), 152.0);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades.find("T001"), trades.end());   
}
