#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <gtest/gtest.h>

#include <yggdrasil/state_machine.hpp>
#include <yggdrasil/json.hpp>
#include <yggdrasil/contextualize.hpp>

using namespace yggdrasil;

struct dumpable_state : state_machine {
    enum class state { uninited, active };
    enum class event { add_item };
    
    state dumpable_state {};
    
    [[=json_dumpable{}]]
    std::vector<int> my_vector;
    
    [[=json_dumpable{}]]
    std::unordered_map<std::string, int> my_map;
    
    [[=initial{}]]
    auto to_active() -> any_of<state::active>;
    
    [[=transition(state::active)]]
    on add_item(int val) {
        my_vector.push_back(val);
        my_map["item_" + std::to_string(val)] = val * 10;
        return accepted;
    }
};

TEST(JsonDumpableTest, SerializeCollections) {
    auto fsm = build_state_machine_type<dumpable_state>{};
    json_serializer serializer;
    
    fsm.initialize();
    fsm.add_item(1);
    fsm.add_item(2);
    
    auto json_str = serializer.to_json(fsm);
    
    // Check vector serialization
    // my_vector should be serialized as an array: [1, 2]
    EXPECT_NE(json_str.find(R"("my_vector": [1, 2])"), std::string::npos) << json_str;
    
    // Check map serialization
    // It should be serialized as an array of pairs.
    // e.g. [{"first": "item_1", "second": 10}, {"first": "item_2", "second": 20}]
    // Order is undefined due to unordered_map, so we just check presence of the elements
    EXPECT_NE(json_str.find(R"("first": "item_1")"), std::string::npos);
    EXPECT_NE(json_str.find(R"("second": 10)"), std::string::npos);
    EXPECT_NE(json_str.find(R"("first": "item_2")"), std::string::npos);
    EXPECT_NE(json_str.find(R"("second": 20)"), std::string::npos);
}
