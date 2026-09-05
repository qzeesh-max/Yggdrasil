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

#include <meta>
#include <iostream>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <type_traits>

struct event_key {};
template <typename T> struct map_to {};

consteval size_t get_plural_len(std::string_view s) {
    if (s.ends_with("s") || s.ends_with("x") || s.ends_with("z") || s.ends_with("ch") || s.ends_with("sh")) return s.size() + 2;
    if (s.ends_with("y") && s.size() > 1) {
        char c = s[s.size() - 2];
        if (c != 'a' && c != 'e' && c != 'i' && c != 'o' && c != 'u') return s.size() + 2;
    }
    return s.size() + 1;
}
template <size_t N> struct static_string {
    char buf[N + 1]{};
    constexpr std::string_view view() const { return {buf, N}; }
};
template <size_t N> consteval static_string<N> pluralize_impl(std::string_view s) {
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

template <typename FSM, std::meta::info method>
struct EventPayload;

struct Rules {
    void trade([[=event_key{}]] std::string_view execID, [[=map_to<int>{}]] double price, int qty) {}
};

template <typename T>
consteval auto generate_proxy() {
    struct GeneratedProxy;
    
    consteval {
        std::vector<std::meta::info> proxy_members;
        
        auto rules_members = std::define_static_array(std::meta::members_of(^^T, std::meta::access_context::current()));
        template for (constexpr auto m : rules_members) {
            if constexpr (std::meta::is_function(m) && std::meta::has_identifier(m) && std::meta::identifier_of(m) == "trade") {
                auto params = std::define_static_array(std::meta::parameters_of(m));
                bool has_key = false;
                std::meta::info key_type = ^^void;
                
                std::vector<std::meta::info> payload_members;
                
                template for (constexpr auto p : params) {
                    bool is_key = false;
                    auto annos = std::define_static_array(std::meta::annotations_of(p));
                    template for (constexpr auto a : annos) {
                        if constexpr (std::meta::type_of(a) == ^^event_key) is_key = true;
                    }
                    if (is_key) {
                        has_key = true;
                        key_type = std::meta::type_of(p);
                        if (key_type == ^^std::string_view) key_type = ^^std::string;
                    } else {
                        auto param_type = std::meta::type_of(p);
                        template for (constexpr auto a : annos) {
                            if constexpr (std::meta::has_template_arguments(std::meta::type_of(a)) && std::meta::template_of(std::meta::type_of(a)) == ^^map_to) {
                                param_type = std::meta::template_arguments_of(std::meta::type_of(a))[0];
                            }
                        }
                        if (param_type == ^^std::string_view) param_type = ^^std::string;
                        
                        payload_members.push_back(std::meta::data_member_spec(param_type, {.name = std::meta::identifier_of(p)}));
                    }
                }
                
                if (has_key) {
                    // Define the aggregate
                    std::meta::define_aggregate(^^EventPayload<GeneratedProxy, m>, payload_members);
                    
                    // Create the map field in proxy
                    constexpr auto plural_str = pluralize_impl<get_plural_len("trade")>("trade");
                    
                    std::meta::info map_tmpl = ^^std::unordered_map;
                    std::meta::info map_args[] = {key_type, ^^EventPayload<GeneratedProxy, m>};
                    std::meta::info map_type = std::meta::substitute(map_tmpl, map_args);
                    
                    proxy_members.push_back(std::meta::data_member_spec(map_type, {.name = plural_str.view()}));
                }
            }
        }
        std::meta::define_aggregate(^^GeneratedProxy, proxy_members);
    }
    return GeneratedProxy{};
}

int main() {
    auto proxy = generate_proxy<Rules>();
    // EventPayload<decltype(proxy), (something)> p; // can't easily name it here, but it exists!
    proxy.trades["test"].qty = 10;
    proxy.trades["test"].price = 15; // Should be int because of map_to
    std::cout << proxy.trades["test"].qty << " " << proxy.trades["test"].price << "\n";
}
