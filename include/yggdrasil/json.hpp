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

#ifndef YGGDRASIL_JSON_HPP
#define YGGDRASIL_JSON_HPP

#include <string>
#include <string_view>
#include <sstream>
#include <type_traits>
#include <iomanip>
#include "for_each.hpp"

namespace yggdrasil {

// ─── Annotations ─────────────────────────────────────────────────────────────

struct json_dumpable {};

// ─── escape_json_string ──────────────────────────────────────────────────────

/// Escapes special characters in a string for JSON representation.
inline std::string escape_json_string(std::string_view s) {
    std::ostringstream ss;
    for (char c : s) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

// ─── to_json_value ───────────────────────────────────────────────────────────

/// Serialize a scalar value to its JSON representation.
/// - arithmetic types  → std::to_string
/// - enums             → "name(int)" if single enumerator matches, otherwise int
/// - std::string       → escaped quoted string
/// - char arrays / pointers → escaped quoted string
/// - other class types → recursive object (uses template for reflection)
/// - anything else     → "<unknown>"
template <typename T>
std::string to_json_value(const T& val) {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_arithmetic_v<U>) {
        return std::to_string(val);
    } else if constexpr (std::is_enum_v<U>) {
        using Underlying = std::underlying_type_t<U>;
        auto int_val = static_cast<Underlying>(val);
        static constexpr auto enumerators = std::define_static_array(std::meta::enumerators_of(^^U));
        std::string name;
        template for (constexpr auto e : enumerators) {
            if (std::meta::extract<U>(e) == val) {
                name = std::meta::identifier_of(e);
                break;
            }
        }
        if (!name.empty()) {
            return "\"" + name + "(" + std::to_string(int_val) + ")\"";
        }
        return std::to_string(int_val);
    } else if constexpr (std::is_same_v<U, std::string>) {
        return "\"" + escape_json_string(val) + "\"";
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        return "\"" + escape_json_string(val) + "\"";
    } else if constexpr (std::is_array_v<std::remove_reference_t<T>> ||
                         std::is_pointer_v<U>) {
        return "\"" + escape_json_string(val) + "\"";
    } else if constexpr (std::is_class_v<U>) {
        // Recurse into class fields using compile-time reflection.
        std::ostringstream ss;
        ss << "{";
        bool first = true;
        static constexpr auto members = std::define_static_array(
            std::meta::nonstatic_data_members_of(^^U, std::meta::access_context::current()));
        template for (constexpr auto member : members) {
            if (!first) ss << ", ";
            first = false;
            ss << "\"" << std::meta::identifier_of(member) << "\": "
               << to_json_value(val.[:member:]);
        }
        ss << "}";
        return ss.str();
    } else {
        return "\"<unknown>\"";
    }
}

// ─── json_serializer ─────────────────────────────────────────────────────────

/// A simple JSON serializer that can participate in a yggdrasil::contextualize()
/// chain.  It exposes a single method:
///
///   std::string to_json(const T& aggregate) const
///
/// The method is implemented via yggdrasil::for_each, so it automatically
/// handles all fields that for_each visits (including annotations-driven
/// flattening of nested state_root members).
///
/// All type dispatch is delegated to to_json_value(), which handles:
///   - arithmetic        → std::to_string
///   - std::string/view  → quoted string
///   - char*/array       → quoted string
///   - class types       → recursive { } object via reflection
///   - everything else   → "<unknown>"
struct json_serializer {
    template <typename T>
    [[nodiscard]] std::string to_json(const T& obj) const {
        std::ostringstream ss;
        ss << "{";
        bool first = true;

        yggdrasil::for_each(obj, [&](const auto& /*obj*/,
                                     std::string_view name,
                                     const auto& value,
                                     const auto&... annotations)
        {
            if (!first) ss << ", ";
            first = false;
            ss << "\"" << name << "\": ";

            constexpr bool has_dumpable = (std::is_same_v<std::remove_cvref_t<decltype(annotations)>, json_dumpable> || ...);
            if constexpr (has_dumpable) {
                ss << "[";
                bool first_elem = true;
                for (const auto& elem : value) {
                    if (!first_elem) ss << ", ";
                    first_elem = false;
                    ss << to_json_value(elem);
                }
                ss << "]";
            } else {
                ss << to_json_value(value);
            }
        });

        ss << "}";
        return ss.str();
    }
};

} // namespace yggdrasil

#endif // !defined(YGGDRASIL_JSON_HPP)
