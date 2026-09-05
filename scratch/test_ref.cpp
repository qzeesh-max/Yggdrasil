#include <meta>
#include <iostream>

int main() {
    constexpr auto type = []() consteval {
        auto t = ^^int&;
        if (std::meta::type_is_reference(t)) {
            return std::meta::type_remove_reference(t);
        }
        return t;
    }();
    std::cout << type << std::endl;
    return 0;
}
