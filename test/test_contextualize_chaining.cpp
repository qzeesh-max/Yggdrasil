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

#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <expected>
#include "order_state.hpp"
#include <yggdrasil/contextualize.hpp>

// Dummy target 1: Returns a status string (Success)
struct string_writer {
    template <typename T>
    std::string trade(const T& obj, std::string_view tradeId, uint32_t fillQty, double fillPx) const {
        return "TRADE:" + std::string(tradeId);
    }

    template <typename T>
    std::string new_order(const T& obj, double price) const {
        return "NEW_ORDER";
    }
};

// Dummy target 2: Fails selectively based on fillQty for audit purposes
struct audit_target {
    template <typename T>
    std::expected<void, std::string> trade(const T& obj, std::string_view tradeId, uint32_t fillQty, double fillPx) const {
        if (fillQty > 1000) {
            return std::unexpected("Fill quantity too large for audit");
        }
        return {};
    }

    template <typename T>
    std::expected<void, std::string> new_order(const T& obj, double price) const {
        if (price < 0.0) {
            return std::unexpected("Negative price forbidden");
        }
        return {};
    }
};

// Dummy target 3: Void returning target that modifies external state
struct stat_counter {
    int trade_count = 0;
    int order_count = 0;

    template <typename T>
    void trade(const T& obj, std::string_view tradeId, uint32_t fillQty, double fillPx) {
        trade_count++;
    }

    template <typename T>
    void new_order(const T& obj, double price) {
        order_count++;
    }
};

// Dummy target 4: Integer returning
struct metrics_target {
    template <typename T>
    int trade(const T& obj, std::string_view tradeId, uint32_t fillQty, double fillPx) const {
        return static_cast<int>(fillQty);
    }
    
    template <typename T>
    int new_order(const T& obj, double price) const {
        return 1;
    }
};

TEST(ContextualizedChainingTest, OneObjectSuccess) {
    auto fsm = build_state_machine_type<order_state>{};
    fsm.initialize("AAPL", 100, 150.0, order_state::limit, 0.0);
    
    auto res_open = fsm.new_order(150.0);
    ASSERT_TRUE(res_open.has_value());

    string_writer writer;
    auto proxy = yggdrasil::contextualize(fsm, writer);
    
    auto result = proxy.trade("T001", 10, 150.5).trade("T001", 10, 150.5);
    
    auto& [fsm_res, writer_res] = result;
    ASSERT_TRUE(fsm_res.has_value());
    ASSERT_EQ(writer_res, "TRADE:T001");
}

TEST(ContextualizedChainingTest, TwoObjectsFSMFailure) {
    auto fsm = build_state_machine_type<order_state>{};
    // Don't transition to open! Try to call trade on uninited
    
    string_writer writer;
    audit_target auditor;
    auto proxy = yggdrasil::contextualize(fsm, writer, auditor);
    
    // Fails on FSM due to state constraints
    auto result = proxy.trade("T001", 10, 150.5).trade("T001", 10, 150.5).trade("T001", 10, 150.5);
    
    auto& [fsm_res, writer_res, audit_res] = result;
    ASSERT_FALSE(fsm_res.has_value()); // Short circuited at FSM
    // Writer result is a string, which default constructs to "" on error/skip
    ASSERT_EQ(writer_res, "");
    ASSERT_FALSE(audit_res.has_value());
    ASSERT_EQ(audit_res.error(), "skipped");
}

TEST(ContextualizedChainingTest, ThreeObjectsTargetFailure) {
    auto fsm = build_state_machine_type<order_state>{};
    fsm.initialize("AAPL", 2000, 150.0, order_state::limit, 0.0);
    fsm.new_order(150.0);
    
    string_writer writer;
    audit_target auditor;
    stat_counter counter;
    auto proxy = yggdrasil::contextualize(fsm, writer, auditor, counter);
    
    // Fails on auditor because fillQty > 1000
    auto result = proxy.trade("T001", 1500, 150.5)
                       .trade("T001", 1500, 150.5)
                       .trade("T001", 1500, 150.5)
                       .trade("T001", 1500, 150.5);
    
    auto& [fsm_res, writer_res, audit_res, counter_res] = result;
    // FSM succeeds
    ASSERT_TRUE(fsm_res.has_value());
    // Writer succeeds
    ASSERT_EQ(writer_res, "TRADE:T001");
    // Auditor fails
    ASSERT_FALSE(audit_res.has_value());
    ASSERT_EQ(audit_res.error(), "Fill quantity too large for audit");
    // Counter skipped (short circuits)
    ASSERT_FALSE(counter_res.has_value());
    ASSERT_EQ(counter_res.error(), "skipped");
    
    // Assert counter didn't count
    ASSERT_EQ(counter.trade_count, 0);
}

TEST(ContextualizedChainingTest, FourObjectsSuccess) {
    auto fsm = build_state_machine_type<order_state>{};
    fsm.initialize("AAPL", 500, 150.0, order_state::limit, 0.0);
    
    string_writer writer;
    audit_target auditor;
    stat_counter counter;
    metrics_target metrics;
    auto proxy = yggdrasil::contextualize(fsm, writer, auditor, counter, metrics);
    
    auto res_open = proxy.new_order(150.0).new_order(150.0).new_order(150.0).new_order(150.0).new_order(150.0);
    auto& [o_fsm, o_writer, o_audit, o_count, o_metrics] = res_open;
    ASSERT_TRUE(o_fsm.has_value());
    ASSERT_EQ(o_writer, "NEW_ORDER");
    ASSERT_TRUE(o_audit.has_value());
    // Since void returning returns an expected<void>, accessing it directly in has_value() works.
    // However, counter is void, so its SlotType is std::expected<void, std::string>
    ASSERT_TRUE(o_count.has_value());
    ASSERT_EQ(o_metrics, 1);
    
    auto result = proxy.trade("T001", 100, 150.5).trade("T001", 100, 150.5).trade("T001", 100, 150.5).trade("T001", 100, 150.5).trade("T001", 100, 150.5);
    
    auto& [fsm_res, writer_res, audit_res, counter_res, metrics_res] = result;
    ASSERT_TRUE(fsm_res.has_value());
    ASSERT_EQ(writer_res, "TRADE:T001");
    ASSERT_TRUE(audit_res.has_value());
    ASSERT_TRUE(counter_res.has_value());
    ASSERT_EQ(metrics_res, 100);
    
    // Assert counter counted
    ASSERT_EQ(counter.order_count, 1);
    ASSERT_EQ(counter.trade_count, 1);
}

TEST(ContextualizedChainingTest, DuplicateKeyFailure) {
    auto fsm = build_state_machine_type<order_state>{};
    fsm.initialize("AAPL", 500, 150.0, order_state::limit, 0.0);
    fsm.new_order(150.0);
    
    string_writer writer;
    stat_counter counter;
    auto proxy = yggdrasil::contextualize(fsm, writer, counter);
    
    auto result1 = proxy.trade("T001", 100, 150.5).trade("T001", 100, 150.5).trade("T001", 100, 150.5);
    ASSERT_TRUE(std::get<0>(result1).has_value());
    ASSERT_EQ(counter.trade_count, 1);
    
    // Second trade with same ID
    auto result2 = proxy.trade("T001", 100, 150.5).trade("T001", 100, 150.5).trade("T001", 100, 150.5);
    auto& [fsm_res, writer_res, counter_res] = result2;
    
    // FSM fails with duplicate key error
    ASSERT_FALSE(fsm_res.has_value());
    ASSERT_EQ(fsm_res.error(), "Duplicate Trade ID");
    
    // Chain short circuits
    ASSERT_EQ(writer_res, "");
    ASSERT_FALSE(counter_res.has_value());
    
    // Assert counter didn't count
    ASSERT_EQ(counter.trade_count, 1);
}
