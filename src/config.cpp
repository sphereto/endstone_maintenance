#include "maintenance/config.h"

#include <algorithm>
#include <cctype>
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

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

Config::Config(std::filesystem::path file) : path_(std::move(file)) {}

bool Config::saveDefault(std::string_view content)
{
    std::error_code ec;
    if (!path_.parent_path().empty()) {
        std::filesystem::create_directories(path_.parent_path(), ec);
    }
    if (!std::filesystem::exists(path_, ec)) {
        std::ofstream out(path_, std::ios::binary);
        if (!out) {
            return false;
        }
        out << content;
    }
    return reload();
}

bool Config::reload()
{
    lines_.clear();
    values_.clear();

    std::ifstream in(path_, std::ios::binary);
    if (!in) {
        return false;
    }

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
                    values_[key] = unquote(trim(std::string_view(trimmed).substr(delimiter + 1)));
                    line.key = std::move(key);
                }
            }
        }
        lines_.push_back(std::move(line));
    }
    return true;
}

bool Config::save() const
{
    std::error_code ec;
    if (!path_.parent_path().empty()) {
        std::filesystem::create_directories(path_.parent_path(), ec);
    }

    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    for (const auto &line : lines_) {
        out << line.raw << '\n';
    }
    return static_cast<bool>(out);
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
    try {
        std::size_t consumed = 0;
        const int result = std::stoi(it->second, &consumed);
        return consumed == it->second.size() ? result : def;
    }
    catch (const std::exception &) {
        return def;
    }
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
