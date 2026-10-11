#include "maintenance/form_response.h"

#include <cstdint>

namespace maintenance {

namespace {

void appendUtf8(std::string &out, std::uint32_t cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    }
    else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

bool parseHex4(std::string_view s, std::size_t pos, std::uint32_t &value)
{
    if (pos + 4 > s.size()) {
        return false;
    }
    value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        const char c = s[pos + i];
        value <<= 4;
        if (c >= '0' && c <= '9') {
            value |= static_cast<std::uint32_t>(c - '0');
        }
        else if (c >= 'a' && c <= 'f') {
            value |= static_cast<std::uint32_t>(c - 'a' + 10);
        }
        else if (c >= 'A' && c <= 'F') {
            value |= static_cast<std::uint32_t>(c - 'A' + 10);
        }
        else {
            return false;
        }
    }
    return true;
}

// Parses a JSON string starting at the opening quote. On success `pos` is left after the closing quote.
bool parseString(std::string_view s, std::size_t &pos, std::string &out)
{
    if (pos >= s.size() || s[pos] != '"') {
        return false;
    }
    ++pos;
    out.clear();

    while (pos < s.size()) {
        const char c = s[pos++];
        if (c == '"') {
            return true;
        }
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (pos >= s.size()) {
            return false;
        }
        const char esc = s[pos++];
        switch (esc) {
        case '"':
        case '\\':
        case '/':
            out.push_back(esc);
            break;
        case 'b':
            out.push_back('\b');
            break;
        case 'f':
            out.push_back('\f');
            break;
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 't':
            out.push_back('\t');
            break;
        case 'u': {
            std::uint32_t cp = 0;
            if (!parseHex4(s, pos, cp)) {
                return false;
            }
            pos += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF) {
                std::uint32_t low = 0;
                if (pos + 6 <= s.size() && s[pos] == '\\' && s[pos + 1] == 'u' && parseHex4(s, pos + 2, low) &&
                    low >= 0xDC00 && low <= 0xDFFF) {
                    pos += 6;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                else {
                    return false;
                }
            }
            appendUtf8(out, cp);
            break;
        }
        default:
            return false;
        }
    }
    return false; // unterminated string
}

void skipWhitespace(std::string_view s, std::size_t &pos)
{
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) {
        ++pos;
    }
}

// Skips one non-string JSON value (null, number, bool, object or array).
bool skipValue(std::string_view s, std::size_t &pos)
{
    int depth = 0;
    while (pos < s.size()) {
        const char c = s[pos];
        if (c == '"') {
            std::string ignored;
            if (!parseString(s, pos, ignored)) {
                return false;
            }
            continue;
        }
        if (c == '[' || c == '{') {
            ++depth;
        }
        else if (c == ']' || c == '}') {
            if (depth == 0) {
                return true; // end of the enclosing array
            }
            --depth;
        }
        else if (c == ',' && depth == 0) {
            return true;
        }
        ++pos;
    }
    return false;
}

} // namespace

std::vector<std::optional<std::string>> parseFormResponse(std::string_view json)
{
    std::vector<std::optional<std::string>> elements;

    std::size_t pos = 0;
    skipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '[') {
        return {};
    }
    ++pos;

    skipWhitespace(json, pos);
    if (pos < json.size() && json[pos] == ']') {
        return elements; // empty array
    }

    while (pos < json.size()) {
        skipWhitespace(json, pos);
        if (pos >= json.size()) {
            return {};
        }

        if (json[pos] == '"') {
            std::string value;
            if (!parseString(json, pos, value)) {
                return {};
            }
            elements.emplace_back(std::move(value));
        }
        else {
            if (!skipValue(json, pos)) {
                return {};
            }
            elements.emplace_back(std::nullopt);
        }

        skipWhitespace(json, pos);
        if (pos >= json.size()) {
            return {};
        }
        if (json[pos] == ',') {
            ++pos;
            continue;
        }
        if (json[pos] == ']') {
            return elements;
        }
        return {};
    }
    return {};
}

std::optional<std::string> formTextAt(std::string_view json, std::size_t index)
{
    const auto elements = parseFormResponse(json);
    if (index < elements.size() && elements[index].has_value()) {
        return elements[index];
    }
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        if (it->has_value()) {
            return *it;
        }
    }
    return std::nullopt;
}

bool constantTimeEquals(std::string_view a, std::string_view b)
{
    unsigned char diff = a.size() == b.size() ? 0 : 1;
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < n; ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

} // namespace maintenance
