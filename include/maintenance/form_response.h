#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace maintenance {

/**
 * Parses the top-level elements of a JSON array as returned by a modal form.
 *
 * String elements are returned decoded (escape sequences resolved); every other
 * element type (null, number, bool, object, array) yields std::nullopt so the
 * element index still lines up with the form's control index.
 *
 * Returns an empty vector if `json` is not a well-formed array.
 */
std::vector<std::optional<std::string>> parseFormResponse(std::string_view json);

/** Returns the string at `index`, or the last string element if `index` is out of range. */
std::optional<std::string> formTextAt(std::string_view json, std::size_t index);

/** Compares two strings without short-circuiting on the first differing byte. */
bool constantTimeEquals(std::string_view a, std::string_view b);

} // namespace maintenance
