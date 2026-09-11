#include <string>
#include <string_view>
#include <iostream>
#include <unordered_map>
#include <sstream>

#include <yggdrasil/state_machine.hpp>
#include <yggdrasil/contextualize.hpp>
#include <yggdrasil/json.hpp>

using namespace yggdrasil;

struct FixField {
    int tag;
};

struct FixFieldValue {
    char val;
};

template <typename U>
char get_fix_enum_val(U value) {
    static constexpr auto enumerators = std::define_static_array(std::meta::enumerators_of(^^U));
    char res = '\0';
    template for (constexpr auto e : enumerators) {
        if (std::meta::extract<U>(e) == value) {
            static constexpr auto anns = std::define_static_array(std::meta::annotations_of(e));
            template for (constexpr auto ann : anns) {
                if constexpr (std::meta::remove_cv(std::meta::type_of(ann)) == ^^FixFieldValue) {
                    res = std::meta::extract<FixFieldValue>(ann).val;
                }
            }
        }
    }
    return res;
}

struct fix_generator {
    template <typename T>
    std::string to_fix(const T& obj, char execType) const {
        std::ostringstream ss;
        ss << "35=8|"; // ExecReport
        ss << "150=" << execType << "|"; // ExecType
        
        yggdrasil::for_each(obj, [&](const auto& /*obj*/,
                                     std::string_view name,
                                     const auto& value,
                                     const auto&... annotations)
        {
            int tag = 0;
            auto get_tag = [&](auto ann) {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(ann)>, FixField>) {
                    tag = ann.tag;
                }
            };
            (get_tag(annotations), ...);
            
            if (tag > 0) {
                using U = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_enum_v<U>) {
                    char fix_val = get_fix_enum_val(value);
                    if (fix_val != '\0') {
                        ss << tag << "=" << fix_val << "|";
                    }
                } else if constexpr (std::is_arithmetic_v<U>) {
                    ss << tag << "=" << value << "|";
                } else if constexpr (std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view>) {
                    if (!value.empty()) {
                        ss << tag << "=" << value << "|";
                    }
                }
            }
        });
        
        return ss.str();
    }
};

struct fix_order_state : state_machine
{
    enum class order_state_enum
    {
        uninited,
        order_received [[=FixFieldValue{'0'}]],
        open [[=FixFieldValue{'0'}]],
        partially_filled [[=FixFieldValue{'1'}]],
        filled [[=FixFieldValue{'2'}]],
        order_rejected [[=FixFieldValue{'8'}]],
        order_canceled [[=FixFieldValue{'4'}]],
    };
    
    enum class event
    {
        new_order, replaced, trade, bust_trade, cancel, rejected, replaced_rejected, cancel_rejected, cancel_sent, replace_sent,
    };
    
    enum type_t
    {
        market [[=FixFieldValue{'1'}]],
        limit [[=FixFieldValue{'2'}]],
        regular_peg [[=FixFieldValue{'R'}]],
        mid_peg [[=FixFieldValue{'M'}]],
        market_peg [[=FixFieldValue{'P'}]],
    };
    
    [[=FixField{39}]]
    order_state_enum fix_order_state {};
    
    [[=FixField{55}]]
    std::string symbol;
    
    [[=FixField{38}]]
    uint32_t orderSize{};
    
    [[=FixField{44}]]
    double orderPrice{};
    
    [[=FixField{40}]]
    type_t type{};
    
    [[=FixField{211}]]
    double pegOffset{};
    
    [[=init_from<^^orderSize>{}]]
    [[=FixField{151}]]
    uint32_t leavesQty{};
    
    [[=init_val<0>{}]]
    [[=FixField{14}]]
    uint32_t cumQty{};
    
    [[=init_val<0.0>{}]]
    [[=FixField{6}]]
    double avgPx{};
    
    [[=FixField{58}]]
    std::string rejectText;
    
    [[=initial{}]]
    auto to_order_received(std::string_view symbol, uint32_t orderSize, double orderPrice,
                           type_t type, double pegOffset) -> any_of<order_state_enum::open, order_state_enum::order_rejected>;

    [[=on_error("Order already open")]]
    auto to_open() -> any_of<order_state_enum::order_canceled, order_state_enum::partially_filled, order_state_enum::filled>;
    
    auto to_order_canceled() -> final;
    auto to_partially_filled() -> any_of<order_state_enum::order_canceled, order_state_enum::partially_filled, order_state_enum::filled>;
    
    [[=on_error("Order already filled")]]
    auto to_filled() -> final;
    
    auto to_order_rejected() -> final;
    
    [[=transition(order_state_enum::open)]]
    on new_order(double price)
    {
        orderPrice = price;
        return accepted;
    }
    
    [[=transition(order_state_enum::order_rejected)]]
    on rejected(std::string_view text)
    {
        rejectText = text;
        return accepted;
    }
    
    [[=transition(order_state_enum::order_canceled)]]
    on cancel()
    {
        return accepted;
    }

    struct trade_data_t {
        uint32_t fillQty {};
        double fillPx {};
    };

    using trades_t = std::unordered_map<std::string, trade_data_t>;

    trades_t trades;
    
    [[=transition(any_of<order_state_enum::partially_filled, order_state_enum::filled>{})]]
    [[=mapping<^^trades>{}]]
    [[=on_error("Duplicate Trade ID")]]
    on trade([[=storage_key{}]]std::string_view tradeId, uint32_t fillQty, double fillPx)
    {
        avgPx = (avgPx * cumQty + fillQty * fillPx) / (fillQty + cumQty);
        leavesQty -= fillQty;
        cumQty += fillQty;
        return leavesQty ? to(order_state_enum::partially_filled) : to(order_state_enum::filled);
    }
};

void check(const auto& res) {
    if (!res.has_value()) {
        std::cout << "[Error] Transition rejected: " << res.error() << "\n";
    }
}

int main() {
    auto fsm = build_state_machine_type<fix_order_state>{};
    fix_generator generator;
    json_serializer serializer;
    
    std::cout << "--- Initialize Order ---\n";
    fsm.initialize("AAPL", 100u, 150.0, fix_order_state::limit, 0.0);
    
    // We can chain to JSON then to FIX
    std::cout << "--- New Order Event ---\n";
    auto [new_order_res, json1, fix1] = contextualize(fsm, serializer, generator)
        .new_order(150.0)
        .to_json()
        .to_fix('0'); // ExecType = 0 (New)
        
    check(new_order_res);
    if (new_order_res.has_value()) {
        std::cout << "JSON: " << json1 << "\n";
        std::cout << "FIX:  " << fix1 << "\n";
        
        // Validation
        if (fix1.find("39=0|") == std::string::npos || fix1.find("55=AAPL|") == std::string::npos) {
            std::cerr << "Validation Failed: FIX output for new_order is incorrect.\n";
            return 1;
        }
    }
    
    std::cout << "\n--- Partial Trade Event ---\n";
    auto [trade1_res, fix2, json2] = contextualize(fsm, generator, serializer)
        .trade("T001", 40u, 150.5)
        .to_fix('1') // ExecType = 1 (Partial Fill)
        .to_json();
        
    check(trade1_res);
    if (trade1_res.has_value()) {
        std::cout << "FIX:  " << fix2 << "\n";
        std::cout << "JSON: " << json2 << "\n";
        
        // Validation
        if (fix2.find("39=1|") == std::string::npos || fix2.find("14=40|") == std::string::npos || fix2.find("151=60|") == std::string::npos) {
            std::cerr << "Validation Failed: FIX output for trade1 is incorrect.\n";
            return 1;
        }
    }
    
    std::cout << "\n--- Complete Trade Event ---\n";
    auto [trade2_res, fix3] = contextualize(fsm, generator)
        .trade("T002", 60u, 151.0)
        .to_fix('2'); // ExecType = 2 (Fill)
        
    check(trade2_res);
    if (trade2_res.has_value()) {
        std::cout << "FIX:  " << fix3 << "\n";
        
        // Validation
        if (fix3.find("39=2|") == std::string::npos || fix3.find("14=100|") == std::string::npos || fix3.find("151=0|") == std::string::npos) {
            std::cerr << "Validation Failed: FIX output for trade2 is incorrect.\n";
            return 1;
        }
    }
    
    std::cout << "\nFIX Order example complete and validated!\n";
    return 0;
}
