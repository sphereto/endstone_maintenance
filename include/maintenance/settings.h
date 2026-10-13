#pragma once

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

namespace maintenance {

class Config;

/** Player-facing texts. The member initializers are the single source of truth for the defaults. */
struct Messages {
    std::string motd; // empty: leave the server's own MOTD untouched (the shipped config sets a text)
    std::string form_title = "Server Maintenance";
    std::string form_body = "The server is currently in maintenance mode.";
    std::string form_input_label = "Password To Enter The Server";
    std::string form_submit = "Login";
    std::string timeout = "You took too long to login during maintenance.";
    std::string wrong_password = "Wrong Password!";
    std::string closed = "Come Check Back Soon!";
    std::string accepted = "Password accepted. Welcome!";
    std::string bypass = "Maintenance is active, but you have bypass permission.";
};

/**
 * Typed, parsed view of the configuration.
 *
 * It is built once (on load, reload and after every change) so hot paths such as joins and server-list pings read plain
 * members instead of doing string-keyed lookups, string copies and list parsing. It has no Endstone dependency.
 */
struct Settings {
    static constexpr const char *kDefaultPassword = "passwordhere";

    bool enabled = true;
    std::string password = kDefaultPassword;
    int kick_delay = 15;
    bool apply_effects = true;
    std::vector<std::string> whitelist; // as written in the config (keeps the original case and order)
    Messages messages;

    /** Builds the snapshot; every value that is missing or malformed in `config` keeps the default above. */
    static Settings fromConfig(const Config &config);

    /** The delay actually used for the kick timer: never below one second. */
    [[nodiscard]] int effectiveKickDelay() const
    {
        return std::max(1, kick_delay);
    }

    /** Case-insensitive whitelist lookup in O(1) without allocating a list. */
    [[nodiscard]] bool isWhitelisted(const std::string &name) const;

    /** Replaces the whitelist and rebuilds the lookup set. */
    void setWhitelist(std::vector<std::string> names);

private:
    std::unordered_set<std::string> whitelist_lookup_; // lower-cased copy of `whitelist`
};

/** The config.yml written on first start. */
const char *defaultConfigText();

} // namespace maintenance
