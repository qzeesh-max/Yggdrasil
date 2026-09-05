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

#include <iostream>
#include <string_view>
#include <array>

template <size_t N>
struct static_string {
    char buf[N + 1]{};
    constexpr std::string_view view() const { return {buf, N}; }
};

consteval size_t get_plural_len(std::string_view s) {
    if (s.ends_with("s") || s.ends_with("x") || s.ends_with("z") || s.ends_with("ch") || s.ends_with("sh")) return s.size() + 2;
    if (s.ends_with("y") && s.size() > 1) {
        char c = s[s.size() - 2];
        if (c != 'a' && c != 'e' && c != 'i' && c != 'o' && c != 'u') return s.size() + 2;
    }
    return s.size() + 1;
}

template <size_t N>
consteval static_string<N> pluralize_impl(std::string_view s) {
    static_string<N> res{};
    size_t i = 0;
    if (s.ends_with("s") || s.ends_with("x") || s.ends_with("z") || s.ends_with("ch") || s.ends_with("sh")) {
        for (; i < s.size(); ++i) res.buf[i] = s[i];
        res.buf[i++] = 'e';
        res.buf[i++] = 's';
    } else if (s.ends_with("y") && s.size() > 1) {
        char c = s[s.size() - 2];
        if (c != 'a' && c != 'e' && c != 'i' && c != 'o' && c != 'u') {
            for (; i < s.size() - 1; ++i) res.buf[i] = s[i];
            res.buf[i++] = 'i';
            res.buf[i++] = 'e';
            res.buf[i++] = 's';
        } else {
            for (; i < s.size(); ++i) res.buf[i] = s[i];
            res.buf[i++] = 's';
        }
    } else {
        for (; i < s.size(); ++i) res.buf[i] = s[i];
        res.buf[i++] = 's';
    }
    return res;
}

#define PLURALIZE(s) []{ \
    constexpr auto res = pluralize_impl<get_plural_len(s)>(s); \
    return res; \
}()

int main() {
    constexpr auto p1 = PLURALIZE("trade");
    constexpr auto p2 = PLURALIZE("bus");
    constexpr auto p3 = PLURALIZE("party");
    constexpr auto p4 = PLURALIZE("boy");
    
    std::cout << p1.view() << "\n";
    std::cout << p2.view() << "\n";
    std::cout << p3.view() << "\n";
    std::cout << p4.view() << "\n";
}
