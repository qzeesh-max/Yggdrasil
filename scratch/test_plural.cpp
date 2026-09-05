#include <meta>
#include <iostream>
#include <string_view>
#include <vector>
#include <tuple>

consteval size_t get_plural_len(std::string_view name) {
    if (name.ends_with("y")) return name.size() + 2;
    if (name.ends_with("o")) return name.size() + 2;
    return name.size() + 1;
}

template<size_t N>
consteval auto pluralize_impl(std::string_view name) {
    struct str_t {
        char chars[N + 1] = {};
        constexpr std::string_view view() const { return {chars, N}; }
    };
    str_t res;
    size_t i = 0;
    if (name.ends_with("y")) {
        for (; i < name.size() - 1; ++i) res.chars[i] = name[i];
        res.chars[i++] = 'i';
        res.chars[i++] = 'e';
        res.chars[i++] = 's';
    } else if (name.ends_with("o")) {
        for (; i < name.size(); ++i) res.chars[i] = name[i];
        res.chars[i++] = 'e';
        res.chars[i++] = 's';
    } else {
        for (; i < name.size(); ++i) res.chars[i] = name[i];
        res.chars[i++] = 's';
    }
    return res;
}

struct Foo {
    void trade();
};

template <typename T>
consteval auto define_my_agg() {
    std::vector<std::meta::info> mems;
    
    static constexpr std::meta::info methods[] = {
        std::meta::members_of(^^T, std::meta::access_context::current())[0]
    };
    
    template for (constexpr auto mem : methods) {
        if constexpr (std::meta::has_identifier(mem)) {
            constexpr auto name = std::meta::identifier_of(mem);
            static constexpr auto plural_str = pluralize_impl<get_plural_len(name)>(name);
            bool some_cond = true;
            if (some_cond) {
                mems.push_back(std::meta::data_member_spec(^^int, {.name = plural_str.chars}));
            }
        }
    }
    return mems[0];
}

int main() {
    constexpr auto type = define_my_agg<Foo>();
    return 0;
}
