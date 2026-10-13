#include "maintenance/config.h"

#include "maintenance/util.h"

#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>

namespace maintenance {

namespace {

std::string trim(std::string_view s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(first, last - first + 1));
}

std::string unquote(std::string value)
{
    if (value.size() >= 2) {
        const char front = value.front();
        if ((front == '"' || front == '\'') && value.back() == front) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

std::string quoteIfNeeded(const std::string &value)
{
    const bool needs_quotes = !value.empty() && (std::isspace(static_cast<unsigned char>(value.front())) != 0 ||
                                                 std::isspace(static_cast<unsigned char>(value.back())) != 0 ||
                                                 value.front() == '"' || value.front() == '\'' || value.front() == '#');
    return needs_quotes ? "\"" + value + "\"" : value;
}

/**
 * Writes `content` next to `target` and renames it over the original, so a crash or a full disk while writing can never
 * leave a truncated config behind: readers see either the complete old file or the complete new one.
 */
bool writeAtomically(const std::filesystem::path &target, const std::string &content)
{
    std::error_code ec;
    auto temporary = target;
    temporary += ".tmp";

    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(temporary, ec);
            return false;
        }
    }

    std::filesystem::rename(temporary, target, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

} // namespace

Config::Config(std::filesystem::path file) : path_(std::move(file)) {}

bool Config::saveDefault(std::string_view content)
{
    std::error_code ec;
    if (!path_.parent_path().empty()) {
        std::filesystem::create_directories(path_.parent_path(), ec);
    }
    if (!std::filesystem::exists(path_, ec) && !writeAtomically(path_, std::string(content))) {
        return false;
    }
    return reload();
}

bool Config::reload()
{
    std::ifstream in(path_, std::ios::binary);
    if (!in) {
        return false; // keep the state we already have rather than silently falling back to defaults
    }

    // Parse into temporaries and swap at the end: a failed reload never leaves a half-loaded or empty config behind.
    std::vector<Line> lines;
    std::unordered_map<std::string, std::string> values;

    std::string raw;
    while (std::getline(in, raw)) {
        if (!raw.empty() && raw.back() == '\r') {
            raw.pop_back();
        }

        Line line{raw, {}};
        const std::string trimmed = trim(raw);
        if (!trimmed.empty() && trimmed.front() != '#') {
            const auto delimiter = trimmed.find(':');
            if (delimiter != std::string::npos) {
                std::string key = trim(std::string_view(trimmed).substr(0, delimiter));
                if (!key.empty()) {
                    values[key] = unquote(trim(std::string_view(trimmed).substr(delimiter + 1)));
                    line.key = std::move(key);
                }
            }
        }
        lines.push_back(std::move(line));
    }

    lines_ = std::move(lines);
    values_ = std::move(values);
    return true;
}

bool Config::save() const
{
    std::error_code ec;
    if (!path_.parent_path().empty()) {
        std::filesystem::create_directories(path_.parent_path(), ec);
    }

    std::string content;
    for (const auto &line : lines_) {
        content += line.raw;
        content += '\n';
    }
    return writeAtomically(path_, content);
}

bool Config::has(const std::string &key) const
{
    return values_.find(key) != values_.end();
}

std::string Config::getString(const std::string &key, const std::string &def) const
{
    const auto it = values_.find(key);
    return it != values_.end() ? it->second : def;
}

int Config::getInt(const std::string &key, int def) const
{
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return def;
    }

    // Same accepted syntax as std::stoi (optional leading whitespace and '+'), but without exceptions or allocation.
    const char *first = it->second.data();
    const char *const last = first + it->second.size();
    while (first != last && std::isspace(static_cast<unsigned char>(*first)) != 0) {
        ++first;
    }
    if (first != last && *first == '+') {
        ++first;
        if (first == last || std::isdigit(static_cast<unsigned char>(*first)) == 0) {
            return def;
        }
    }

    int result = 0;
    const auto [end, ec] = std::from_chars(first, last, result);
    return (ec == std::errc() && end == last) ? result : def;
}

bool Config::getBool(const std::string &key, bool def) const
{
    const auto it = values_.find(key);
    if (it == values_.end()) {
        return def;
    }
    const std::string v = toLower(it->second);
    if (v == "true" || v == "1" || v == "yes" || v == "on") {
        return true;
    }
    if (v == "false" || v == "0" || v == "no" || v == "off") {
        return false;
    }
    return def;
}

std::vector<std::string> Config::getList(const std::string &key) const
{
    return splitList(getString(key));
}

void Config::set(const std::string &key, const std::string &value)
{
    values_[key] = value;
    const std::string rendered = key + ": " + quoteIfNeeded(value);

    for (auto &line : lines_) {
        if (line.key == key) {
            line.raw = rendered;
            return;
        }
    }
    lines_.push_back({rendered, key});
}

void Config::setBool(const std::string &key, bool value)
{
    set(key, value ? "true" : "false");
}

void Config::setList(const std::string &key, const std::vector<std::string> &values)
{
    std::ostringstream joined;
    for (std::size_t i = 0; i < values.size(); ++i) {
        joined << (i == 0 ? "" : ", ") << values[i];
    }
    set(key, joined.str());
}

std::vector<std::string> Config::splitList(std::string_view text)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto comma = text.find(',', start);
        const auto end = comma == std::string_view::npos ? text.size() : comma;
        std::string item = trim(text.substr(start, end - start));
        if (!item.empty()) {
            result.push_back(std::move(item));
        }
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    return result;
}

} // namespace maintenance
