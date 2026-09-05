![Yggdrasil Logo](assets/yggdrasil_logo.png)
# 🌳 Yggdrasil

> **The declarative, reflection-powered Finite State Machine framework for modern C++26.**

Yggdrasil is a highly declarative, attribute-driven Finite State Machine (FSM) framework that makes writing complex state logic as easy as reading a configuration file. By leveraging bleeding-edge **C++26 reflection** (`<meta>`), Yggdrasil unifies your state topology, data, and event handlers into a single, cohesive, zero-boilerplate structure.

## 🌟 Why Developers Love Yggdrasil

* **Declarative & Self-Documenting:** Say goodbye to convoluted transition tables and massive `switch` statements. Yggdrasil uses C++ attributes like `[[=transition(state::open)]]` directly on your event handlers. The code *is* the documentation.
* **Zero-Boilerplate Data Management:** Variables are automatically tracked and initialized. Need to initialize `leavesQty` from `orderSize`? Just write `[[=init_from<"orderSize">{}]]`. Accessors and getters are generated automatically.
* **Intelligent Data Mapping:** Track historical data (like trades on an order) natively. With `[[=mapping<^^trades>{}]]` and `[[=storage_key{}]]`, Yggdrasil automatically hashes, stores, and maps incoming event data without manual map-insertion logic.
* **Safe by Default (and Revertible):** Yggdrasil forces you to define valid endpoints and automatically rejects invalid events with rich errors like `[[=on_error("Order already filled")]]`. It even supports reverting from final states via `[[=can_revert_final{}]]`.
* **Seamless Testability:** Generated FSM event handlers return `std::expected<void, std::string>`, making unit testing incredibly straightforward.

## 📖 The Etymology
In Norse mythology, **Yggdrasil** is the immense, sacred World Tree whose branches and roots connect the Nine Worlds of the cosmos. 

The metaphor for a finite state machine is perfect:
* **The Branches and Roots:** The complex, branching paths of your state transitions.
* **The Worlds:** The distinct states (e.g., `open`, `filled`) your application exists in.
* **The Tree Itself:** The framework. Yggdrasil is the robust, centralized structure that connects isolated states and guides data safely between them.

## ⚡ Quick Start

Yggdrasil allows you to define your states, events, and data in a single `struct`. Here is an example of an Order Execution state machine:

```cpp
#include <yggdrasil/state_machine.hpp>
#include <string>
#include <unordered_map>

using namespace yggdrasil;

struct order_state : state_machine {
    // 1. Define States and Events
    enum class state { uninited, order_received, open, partially_filled, filled, order_rejected, order_canceled };
    enum class event { new_order, trade, bust_trade, cancel };
    
    state order_state {};
    std::string symbol;
    uint32_t orderSize{};
    
    // 2. Zero-Boilerplate Initialization (using C++26 reflection variables)
    [[=init_from<^^orderSize>{}]]
    uint32_t leavesQty{};
    
    [[=init_val<0>{}]]
    uint32_t cumQty{};
    
    // 3. Define Transitions and Error Handling
    [[=initial{}]]
    auto to_order_received(std::string_view symbol, uint32_t orderSize) -> any_of<state::open, state::order_rejected>;

    [[=on_error("Order already open")]]
    auto to_open() -> any_of<state::order_canceled, state::partially_filled, state::filled>;
    
    auto to_partially_filled() -> any_of<state::order_canceled, state::partially_filled, state::filled>;

    [[=on_error("Order already filled")]]
    auto to_filled() -> final;
    
    // 4. Bind Events to Handlers
    [[=transition(state::open)]]
    on new_order(double price) {
        return accepted;
    }
    
    // Define a map to automatically store trade data
    struct trade_data_t {
        uint32_t fillQty {};
        double fillPx {};
    };
    std::unordered_map<std::string, trade_data_t> trades;

    // Yggdrasil automatically inserts into `trades`, extracting `tradeId` as the map key
    [[=transition(any_of<state::partially_filled, state::filled>{})]]
    [[=mapping<^^trades>{}]]
    [[=on_error("Duplicate Trade ID")]]
    on trade([[=storage_key{}]]std::string_view tradeId, uint32_t fillQty, double fillPx) {
        leavesQty -= fillQty;
        cumQty += fillQty;
        return leavesQty ? to(state::partially_filled) : to(state::filled);
    }

    // You can even revert from a final state (like filled) using can_revert_final!
    [[=can_revert_final{}]]
    [[=transition(any_of<state::partially_filled, state::open, state::order_canceled>{})]]
    [[=on_error("Trade to bust not found")]]
    on bust_trade(std::string_view tradeId) {
        if (auto it = trades.find(std::string(tradeId)); it != trades.end()) {
            cumQty -= it->second.fillQty;
            leavesQty += it->second.fillQty;
            trades.erase(it);
            
            if (order_state == state::filled) return to(state::order_canceled);
            if (cumQty > 0) return to(state::partially_filled);
            return to(state::open);
        }
        return rejected;
    }
};
```

### Using the State Machine

Yggdrasil uses C++26 metaprogramming to generate a fully-featured proxy object wrapped around your definition:

```cpp
// Generate the FSM
auto fsm = build_state_machine_type<order_state>{};

// Initialize (moves to 'order_received')
fsm.initialize("AAPL", 100);

// Fire events! Returns std::expected<void, std::string>
auto result = fsm.new_order(155.0);
if (result.has_value()) {
    // Accessors are automatically generated!
    std::cout << "State: " << (int)fsm.order_state() << "\n";
    std::cout << "Symbol: " << fsm.symbol() << "\n";
}

// Data mapping automatically tracks your data
fsm.trade("T001", 40, 154.5);
std::cout << "Trades recorded: " << fsm.trades().size() << "\n";

// Invalid transitions safely return unexpected errors
auto bad_transition = fsm.trade("T002", 100, 155.0); 
// bad_transition.error() == "Order already open" (if called in wrong state, e.g. uninited)
```

## 🛠️ Requirements

Yggdrasil relies heavily on the upcoming C++26 Reflection TS (`std::meta`). 

* A compiler supporting C++26 and `<meta>` (e.g., bleeding-edge Clang/GCC forks implementing P2996 or related reflection proposals).
* C++26 standard library support (specifically `std::expected` and `std::string_view`).

## 📄 License

This project is licensed under the GNU AFFERO GENERAL PUBLIC LICENSE.