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
#include "for_each.hpp"

namespace yggdrasil {

// ─── to_json_value ───────────────────────────────────────────────────────────

/// Serialize a scalar value to its JSON representation.
/// - arithmetic types  → std::to_string
/// - std::string       → quoted string
/// - char arrays / pointers → quoted string
/// - other class types → recursive object (uses template for reflection)
/// - anything else     → "<unknown>"
template <typename T>
std::string to_json_value(const T& val) {
    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_arithmetic_v<U>) {
        return std::to_string(val);
    } else if constexpr (std::is_same_v<U, std::string>) {
        return "\"" + val + "\"";
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        return "\"" + std::string(val) + "\"";
    } else if constexpr (std::is_array_v<std::remove_reference_t<T>> ||
                         std::is_pointer_v<U>) {
        return "\"" + std::string(val) + "\"";
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
                                     const auto&... /*annotations*/)
        {
            if (!first) ss << ", ";
            first = false;
            ss << "\"" << name << "\": " << to_json_value(value);
        });

        ss << "}";
        return ss.str();
    }
};

} // namespace yggdrasil

#endif // !defined(YGGDRASIL_JSON_HPP)
