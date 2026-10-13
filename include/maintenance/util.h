#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace maintenance {

/** ASCII lower-casing (locale independent), used for every case-insensitive name comparison. */
inline std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace maintenance
