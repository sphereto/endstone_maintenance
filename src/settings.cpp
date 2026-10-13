#include "maintenance/settings.h"

#include "maintenance/config.h"
#include "maintenance/util.h"

#include <utility>

namespace maintenance {

namespace {

constexpr const char *kDefaultConfig =
    "# Maintenance plugin configuration. Run /maintenance reload after editing this file.\n"
    "\n"
    "# Master switch. Can also be toggled in-game with /maintenance on|off.\n"
    "maintenance.enabled: true\n"
    "\n"
    "# Password players must enter to join while maintenance is on.\n"
    "maintenance.password: passwordhere\n"
    "\n"
    "# Seconds a player has to enter the password before being kicked.\n"
    "maintenance.kick_delay: 15\n"
    "\n"
    "# Apply blindness / slowness / resistance while a player is on the login screen.\n"
    "maintenance.apply_effects: true\n"
    "\n"
    "# Comma-separated player names that skip the login screen (case-insensitive).\n"
    "maintenance.whitelist:\n"
    "\n"
    "# Text shown in the server list while maintenance is on. Leave empty to keep the normal MOTD.\n"
    "messages.motd: §cServer under maintenance\n"
    "\n"
    "# Messages shown to players.\n"
    "messages.form_title: Server Maintenance\n"
    "messages.form_body: The server is currently in maintenance mode.\n"
    "messages.form_input_label: Password To Enter The Server\n"
    "messages.form_submit: Login\n"
    "messages.timeout: You took too long to login during maintenance.\n"
    "messages.wrong_password: Wrong Password!\n"
    "messages.closed: Come Check Back Soon!\n"
    "messages.accepted: Password accepted. Welcome!\n"
    "messages.bypass: Maintenance is active, but you have bypass permission.\n";

} // namespace

const char *defaultConfigText()
{
    return kDefaultConfig;
}

Settings Settings::fromConfig(const Config &config)
{
    Settings s; // every getter below falls back to the value already in `s`, i.e. the defaults declared in the header

    s.enabled = config.getBool("maintenance.enabled", s.enabled);
    s.password = config.getString("maintenance.password", s.password);
    s.kick_delay = config.getInt("maintenance.kick_delay", s.kick_delay);
    s.apply_effects = config.getBool("maintenance.apply_effects", s.apply_effects);
    s.setWhitelist(config.getList("maintenance.whitelist"));

    auto &m = s.messages;
    m.motd = config.getString("messages.motd", m.motd);
    m.form_title = config.getString("messages.form_title", m.form_title);
    m.form_body = config.getString("messages.form_body", m.form_body);
    m.form_input_label = config.getString("messages.form_input_label", m.form_input_label);
    m.form_submit = config.getString("messages.form_submit", m.form_submit);
    m.timeout = config.getString("messages.timeout", m.timeout);
    m.wrong_password = config.getString("messages.wrong_password", m.wrong_password);
    m.closed = config.getString("messages.closed", m.closed);
    m.accepted = config.getString("messages.accepted", m.accepted);
    m.bypass = config.getString("messages.bypass", m.bypass);
    return s;
}

bool Settings::isWhitelisted(const std::string &name) const
{
    return !whitelist_lookup_.empty() && whitelist_lookup_.count(toLower(name)) > 0;
}

void Settings::setWhitelist(std::vector<std::string> names)
{
    whitelist = std::move(names);
    whitelist_lookup_.clear();
    whitelist_lookup_.reserve(whitelist.size());
    for (const auto &name : whitelist) {
        whitelist_lookup_.insert(toLower(name));
    }
}

} // namespace maintenance
