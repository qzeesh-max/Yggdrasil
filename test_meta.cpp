#include <iostream>
#include <meta>
#include <vector>

template <typename T>
consteval auto generate_proxy() {
    struct GeneratedProxy;
    consteval {
        std::vector<std::meta::info> mems;
        mems.push_back(std::meta::data_member_spec(^^int, {.name="x"}));
        std::meta::define_aggregate(^^GeneratedProxy, mems);
    }
    return GeneratedProxy{};
}

template <typename T>
using proxy_type = decltype(generate_proxy<T>());

int main() {
    proxy_type<int> p;
    p.x = 42;
    std::cout << p.x << std::endl;
}
