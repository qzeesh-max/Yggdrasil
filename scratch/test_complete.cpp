#include <meta>
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>

struct Incomplete;

consteval auto test_complete() {
    std::vector<std::meta::info> mems;
    mems.push_back(std::meta::data_member_spec(^^int, {.name = "x"}));
    std::meta::define_aggregate(^^Incomplete, mems);
    
    // Now try to instantiate unordered_map
    auto map_type = std::meta::substitute(^^std::unordered_map, {^^std::string, ^^Incomplete});
    return map_type;
}

using T = typename [: test_complete() :];

int main() {
    return 0;
}
