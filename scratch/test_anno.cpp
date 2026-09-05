#include <meta>
#include <iostream>

struct event_key {};

struct Foo {
    void method([[=event_key{}]] int x) {}
};

int main() {
    constexpr auto type = []() consteval {
        auto mems = std::meta::members_of(^^Foo, std::meta::access_context::current());
        auto method = mems[0];
        auto params = std::meta::parameters_of(method);
        auto p = params[0];
        auto annos = std::meta::annotations_of(p);
        return std::meta::remove_cvref(std::meta::type_of(annos[0])) == ^^event_key;
    }();
    std::cout << type << std::endl;
    return 0;
}
