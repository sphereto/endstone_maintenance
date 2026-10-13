#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace maintenance {

/**
 * Minimal flat "key: value" configuration file (YAML-compatible subset).
 *
 * - Lines starting with '#' and blank lines are preserved on save.
 * - Values may be wrapped in single or double quotes.
 * - Lists are stored as comma-separated values.
 *
 * This class has no Endstone dependency so it can be unit tested on its own.
 */
class Config {
public:
    explicit Config(std::filesystem::path file);

    /** Writes `content` if the file does not exist yet, then loads the file. */
    bool saveDefault(std::string_view content);

    /** (Re)loads the file from disk. Returns false (and keeps the current state) if it could not be read. */
    bool reload();

    /** Writes the current state back to disk atomically (temp file + rename), keeping comments and ordering. */
    bool save() const;

    [[nodiscard]] bool has(const std::string &key) const;
    [[nodiscard]] std::string getString(const std::string &key, const std::string &def = "") const;
    [[nodiscard]] int getInt(const std::string &key, int def) const;
    [[nodiscard]] bool getBool(const std::string &key, bool def) const;
    [[nodiscard]] std::vector<std::string> getList(const std::string &key) const;

    void set(const std::string &key, const std::string &value);
    void setBool(const std::string &key, bool value);
    void setList(const std::string &key, const std::vector<std::string> &values);

    [[nodiscard]] const std::filesystem::path &path() const
    {
        return path_;
    }

    /** Splits "a, b,,c" into {"a", "b", "c"}. */
    static std::vector<std::string> splitList(std::string_view text);

private:
    struct Line {
        std::string raw;
        std::string key; // empty for comments / blank lines
    };

    std::filesystem::path path_;
    std::vector<Line> lines_;
    std::unordered_map<std::string, std::string> values_;
};

} // namespace maintenance
