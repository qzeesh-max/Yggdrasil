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

#include <string>
#include <string_view>
#include <expected>
#include <gtest/gtest.h>

#include <yggdrasil/state_machine.hpp>
#include <yggdrasil/contextualize.hpp>
#include <yggdrasil/json.hpp>

#include "order_state.hpp"

// ─── GTest suite ─────────────────────────────────────────────────────────────

TEST(ContextualizedTest, TradeAndSerialize) {
    yggdrasil::build_state_machine_type<order_state> fsm;
    yggdrasil::json_serializer serializer;

    // Walk the FSM to an open state
    fsm.initialize("AAPL", 100u, 150.0, order_state::limit, 0.0);
    ASSERT_EQ(fsm.order_state(), order_state::state::order_received);

    auto r1 = fsm.new_order(150.0);
    ASSERT_TRUE(r1.has_value()) << r1.error();

    // Contextualized chain: trade then serialize
    auto result_tuple = yggdrasil::contextualize(fsm, serializer)
        .trade("T002", 60u, 152.0)
        .to_json();

    using ResultTupleType = decltype(result_tuple);
    static_assert(std::is_same_v<ResultTupleType,
        std::tuple<std::expected<void, std::string>, std::string>>,
        "Expected tuple<expected<void,string>, string>");

    auto& [trade_result, json_str] = result_tuple;
    EXPECT_TRUE(trade_result.has_value()) << trade_result.error();
    EXPECT_FALSE(json_str.empty());

    // The JSON should contain properly serialized enumerations "name(int)"
    EXPECT_NE(json_str.find("\"order_state\": \"partially_filled(3)\""), std::string::npos)
        << "Enum formatting incorrect or not found.\nJSON: " << json_str;
    
    // Check that original parameter annotations are preserved for trade:
    // trade(tradeId, fillQty, fillPx) should retain those parameter names.
    EXPECT_NE(json_str.find("\"tradeId\": \"T002\""), std::string::npos);
    EXPECT_NE(json_str.find("\"fillQty\": 60"), std::string::npos);
    EXPECT_NE(json_str.find("\"fillPx\": 152"), std::string::npos);

    // Other state checks
    EXPECT_NE(json_str.find("\"cumQty\": 60"), std::string::npos);
    EXPECT_NE(json_str.find("\"avgPx\": 152"), std::string::npos);
}

TEST(ContextualizedTest, StringEscaping) {
    yggdrasil::build_state_machine_type<order_state> fsm;
    yggdrasil::json_serializer serializer;

    // Use a symbol with special characters to test string escaping
    // 'A', '"', 'B', '\n', 'C' -> total 5 chars, fits in 8 char array.
    fsm.initialize("A\"B\nC", 100u, 150.0, order_state::limit, 0.0);
    
    auto result_tuple = yggdrasil::contextualize(fsm, serializer)
        .new_order(150.0)
        .to_json();

    auto& [order_result, json_str] = result_tuple;
    EXPECT_TRUE(order_result.has_value()) << order_result.error();
    EXPECT_FALSE(json_str.empty());

    // The symbol should be escaped: "A\"B\nC" becomes "A\"B\nC" in JSON (with literal backslashes)
    EXPECT_NE(json_str.find("\"symbol\": \"A\\\"B\\nC\""), std::string::npos)
        << "String escaping failed.\nJSON: " << json_str;
}

TEST(ContextualizedTest, ChainShortCircuitsOnError) {
    yggdrasil::build_state_machine_type<order_state> fsm;
    yggdrasil::json_serializer serializer;

    // Walk to open state then fill completely
    fsm.initialize("AAPL", 100u, 150.0, order_state::limit, 0.0);
    ASSERT_TRUE(fsm.new_order(150.0));
    ASSERT_TRUE(fsm.trade("T001", 100u, 151.0));

    // bust a non-existent trade — should fail and poison the chain
    auto result_tuple = yggdrasil::contextualize(fsm, serializer)
        .bust_trade("NONEXISTENT")
        .to_json();

    auto& [bust_result, json_str] = result_tuple;
    EXPECT_FALSE(bust_result.has_value());
    EXPECT_TRUE(json_str.empty());
}
