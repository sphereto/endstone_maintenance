#include "maintenance/plugin.h"

#include "maintenance/form_response.h"

#include <algorithm>
#include <cctype>

#ifndef MAINTENANCE_VERSION
#define MAINTENANCE_VERSION "0.0.0-dev"
#endif

using endstone::ColorFormat;

ENDSTONE_PLUGIN("maintenance", MAINTENANCE_VERSION, maintenance::MaintenancePlugin)
{
    description = "Password-protected maintenance mode for Endstone servers.";
    website = "https://github.com/sphereto/endstone_maintenance";
    authors = {"zcvmix", "sphereto"};

    command("maintenance")
        .description("Manage maintenance mode.")
        .usages("/maintenance (on|off|status|reload|list)<action: MaintenanceAction>",
                "/maintenance (add|remove)<action: MaintenanceListAction> <player: string>",
                "/maintenance (password)<action: MaintenancePasswordAction> <password: string>")
        .aliases("mt")
        .permissions("maintenance.command");

    permission("maintenance.command")
        .description("Allow use of the /maintenance command.")
        .default_(endstone::PermissionDefault::Operator);

    permission("maintenance.bypass")
        .description("Bypass maintenance mode.")
        .default_(endstone::PermissionDefault::Operator);
}

namespace maintenance {

namespace {

constexpr const char *kDefaultPassword = "passwordhere";
constexpr std::size_t kPasswordControlIndex = 2; // Label, Label, TextInput

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

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

void MaintenancePlugin::onEnable()
{
    config_ = std::make_unique<Config>(getDataFolder() / "config.yml");
    if (!config_->saveDefault(kDefaultConfig)) {
        getLogger().error("Could not read or create {}; using built-in defaults.", config_->path().string());
    }
    warnAboutPassword();

    registerEvent(&MaintenancePlugin::onPlayerJoin, *this);
    registerEvent(&MaintenancePlugin::onPlayerQuit, *this);
    registerEvent(&MaintenancePlugin::onPlayerMove, *this, endstone::EventPriority::High);
    registerEvent(&MaintenancePlugin::onPlayerChat, *this, endstone::EventPriority::High);
    registerEvent(&MaintenancePlugin::onPlayerCommand, *this, endstone::EventPriority::High);
    registerEvent(&MaintenancePlugin::onPlayerInteract, *this, endstone::EventPriority::High);
    registerEvent(&MaintenancePlugin::onPlayerDropItem, *this, endstone::EventPriority::High);
    registerEvent(&MaintenancePlugin::onBlockBreak, *this, endstone::EventPriority::High);
    registerEvent(&MaintenancePlugin::onBlockPlace, *this, endstone::EventPriority::High);
    registerEvent(&MaintenancePlugin::onServerListPing, *this);

    getLogger().info("Maintenance mode is {}.", isEnabled() ? "ON" : "off");
}

void MaintenancePlugin::onDisable()
{
    unlockEveryone();
}

// --------------------------------------------------------------------------------------------------------------------
// Commands
// --------------------------------------------------------------------------------------------------------------------

bool MaintenancePlugin::onCommand(endstone::CommandSender &sender, const endstone::Command &command,
                                  const std::vector<std::string> &args)
{
    if (command.getName() != "maintenance" || args.empty()) {
        return false;
    }

    const std::string action = toLower(args[0]);
    if (action == "on" || action == "off") {
        return handleToggle(sender, action == "on");
    }
    if (action == "status") {
        handleStatus(sender);
        return true;
    }
    if (action == "reload") {
        return handleReload(sender);
    }
    if (action == "list") {
        handleList(sender);
        return true;
    }
    if ((action == "add" || action == "remove") && args.size() >= 2) {
        return handleWhitelist(sender, action == "add", args[1]);
    }
    if (action == "password" && args.size() >= 2) {
        return handlePassword(sender, args[1]);
    }
    return false;
}

bool MaintenancePlugin::handleToggle(endstone::CommandSender &sender, bool enable)
{
    if (isEnabled() == enable) {
        sender.sendErrorMessage("Maintenance mode is already {}.", enable ? "on" : "off");
        return true;
    }

    config_->setBool("maintenance.enabled", enable);
    if (!config_->save()) {
        sender.sendErrorMessage("Maintenance mode changed, but the config file could not be saved.");
    }

    if (enable) {
        for (auto *player : getServer().getOnlinePlayers()) {
            if (!isExempt(*player)) {
                lockPlayer(*player);
            }
        }
        getServer().broadcastMessage(ColorFormat::Gold + "Maintenance mode has been enabled.");
    }
    else {
        unlockEveryone();
        getServer().broadcastMessage(ColorFormat::Green + "Maintenance mode has been disabled.");
    }
    sender.sendMessage(ColorFormat::Green + std::format("Maintenance mode is now {}.", enable ? "ON" : "OFF"));
    return true;
}

void MaintenancePlugin::handleStatus(endstone::CommandSender &sender) const
{
    sender.sendMessage("Maintenance mode: {}", isEnabled() ? ColorFormat::Red + "ON" : ColorFormat::Green + "OFF");
    sender.sendMessage("Players on the login screen: {}", locked_.size());
    sender.sendMessage("Whitelisted players: {}", config_->getList("maintenance.whitelist").size());
    sender.sendMessage("Kick delay: {}s", config_->getInt("maintenance.kick_delay", 15));
}

bool MaintenancePlugin::handleReload(endstone::CommandSender &sender)
{
    if (!config_->reload()) {
        sender.sendErrorMessage("Could not read {}.", config_->path().string());
        return true;
    }
    warnAboutPassword();
    sender.sendMessage(ColorFormat::Green + "Maintenance config reloaded.");
    return true;
}

void MaintenancePlugin::handleList(endstone::CommandSender &sender) const
{
    const auto names = config_->getList("maintenance.whitelist");
    if (names.empty()) {
        sender.sendMessage("The maintenance whitelist is empty.");
        return;
    }
    std::string joined;
    for (const auto &name : names) {
        joined += (joined.empty() ? "" : ", ") + name;
    }
    sender.sendMessage("Whitelisted ({}): {}", names.size(), joined);
}

bool MaintenancePlugin::handleWhitelist(endstone::CommandSender &sender, bool add, const std::string &name)
{
    auto names = config_->getList("maintenance.whitelist");
    const auto it = std::find_if(names.begin(), names.end(), [&](const std::string &existing) {
        return toLower(existing) == toLower(name);
    });

    if (add) {
        if (it != names.end()) {
            sender.sendErrorMessage("{} is already whitelisted.", name);
            return true;
        }
        names.push_back(name);
    }
    else {
        if (it == names.end()) {
            sender.sendErrorMessage("{} is not whitelisted.", name);
            return true;
        }
        names.erase(it);
    }

    config_->setList("maintenance.whitelist", names);
    if (!config_->save()) {
        sender.sendErrorMessage("Whitelist changed, but the config file could not be saved.");
    }
    sender.sendMessage(ColorFormat::Green +
                       std::format("{} {} the maintenance whitelist.", name, add ? "added to" : "removed from"));
    return true;
}

bool MaintenancePlugin::handlePassword(endstone::CommandSender &sender, const std::string &password)
{
    config_->set("maintenance.password", password);
    if (!config_->save()) {
        sender.sendErrorMessage("Password changed, but the config file could not be saved.");
    }
    sender.sendMessage(ColorFormat::Green + "Maintenance password updated.");
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// Events
// --------------------------------------------------------------------------------------------------------------------

void MaintenancePlugin::onPlayerJoin(endstone::PlayerJoinEvent &event)
{
    if (!isEnabled()) {
        return;
    }

    auto &player = event.getPlayer();
    if (isExempt(player)) {
        player.sendMessage(ColorFormat::Green +
                           message("messages.bypass", "Maintenance is active, but you have bypass permission."));
        return;
    }
    lockPlayer(player);
}

void MaintenancePlugin::onPlayerQuit(endstone::PlayerQuitEvent &event)
{
    const auto uuid = event.getPlayer().getUniqueId();
    cancelPendingKick(uuid);
    locked_.erase(uuid);
}

void MaintenancePlugin::onPlayerMove(endstone::PlayerMoveEvent &event)
{
    if (isLocked(event.getPlayer())) {
        event.setCancelled(true);
    }
}

void MaintenancePlugin::onPlayerChat(endstone::PlayerChatEvent &event)
{
    if (isLocked(event.getPlayer())) {
        event.setCancelled(true);
    }
}

void MaintenancePlugin::onPlayerCommand(endstone::PlayerCommandEvent &event)
{
    if (isLocked(event.getPlayer())) {
        event.setCancelled(true);
    }
}

void MaintenancePlugin::onPlayerInteract(endstone::PlayerInteractEvent &event)
{
    if (isLocked(event.getPlayer())) {
        event.setCancelled(true);
    }
}

void MaintenancePlugin::onPlayerDropItem(endstone::PlayerDropItemEvent &event)
{
    if (isLocked(event.getPlayer())) {
        event.setCancelled(true);
    }
}

void MaintenancePlugin::onBlockBreak(endstone::BlockBreakEvent &event)
{
    if (isLocked(event.getPlayer())) {
        event.setCancelled(true);
    }
}

void MaintenancePlugin::onBlockPlace(endstone::BlockPlaceEvent &event)
{
    if (isLocked(event.getPlayer())) {
        event.setCancelled(true);
    }
}

void MaintenancePlugin::onServerListPing(endstone::ServerListPingEvent &event)
{
    if (!isEnabled()) {
        return;
    }
    const auto motd = message("messages.motd", "");
    if (!motd.empty()) {
        event.setMotd(motd);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Locking
// --------------------------------------------------------------------------------------------------------------------

bool MaintenancePlugin::isEnabled() const
{
    return config_ && config_->getBool("maintenance.enabled", true);
}

bool MaintenancePlugin::isExempt(endstone::Player &player) const
{
    if (player.hasPermission("maintenance.bypass")) {
        return true;
    }
    const auto name = toLower(player.getName());
    const auto names = config_->getList("maintenance.whitelist");
    return std::any_of(names.begin(), names.end(), [&](const std::string &entry) {
        return toLower(entry) == name;
    });
}

bool MaintenancePlugin::isLocked(const endstone::Player &player) const
{
    return !locked_.empty() && locked_.count(player.getUniqueId()) > 0;
}

void MaintenancePlugin::lockPlayer(endstone::Player &player)
{
    if (!locked_.insert(player.getUniqueId()).second) {
        return;
    }

    if (config_->getBool("maintenance.apply_effects", true)) {
        const auto target = "effect \"" + player.getName() + "\" ";
        runServerCommand(target + "blindness 1000000 255 true");
        runServerCommand(target + "slowness 1000000 255 true");
        runServerCommand(target + "resistance 1000000 255 true");
    }

    showLoginForm(player);
    player.playSound(player.getLocation(), "random.orb", 1.0F, 1.0F);
}

void MaintenancePlugin::unlockPlayer(endstone::Player &player)
{
    const auto uuid = player.getUniqueId();
    cancelPendingKick(uuid);
    if (locked_.erase(uuid) == 0) {
        return;
    }

    runServerCommand("effect \"" + player.getName() + "\" clear");
    player.playSound(player.getLocation(), "random.levelup", 1.0F, 1.0F);
}

void MaintenancePlugin::unlockEveryone()
{
    for (auto *player : getServer().getOnlinePlayers()) {
        if (isLocked(*player)) {
            unlockPlayer(*player);
            player->closeForm();
        }
    }
    locked_.clear();
    pending_kicks_.clear();
}

void MaintenancePlugin::showLoginForm(endstone::Player &player)
{
    const int delay = std::max(1, config_->getInt("maintenance.kick_delay", 15));
    const auto uuid = player.getUniqueId();

    cancelPendingKick(uuid);
    if (const auto task = getServer().getScheduler().runTaskLater(
            *this,
            [this, uuid]() {
                pending_kicks_.erase(uuid);
                if (auto *p = getServer().getPlayer(uuid); p != nullptr && isLocked(*p)) {
                    p->kick(message("messages.timeout", "You took too long to login during maintenance."));
                }
            },
            static_cast<std::uint64_t>(delay) * 20)) {
        pending_kicks_[uuid] = task->getTaskId();
    }

    endstone::ModalForm form;
    form.setTitle(message("messages.form_title", "Server Maintenance"));
    form.addControl(endstone::Label(ColorFormat::Gold + "Time Left: " + ColorFormat::Green + std::to_string(delay) +
                                    "s" + ColorFormat::Reset));
    form.addControl(endstone::Label(message("messages.form_body", "The server is currently in maintenance mode.")));
    form.addControl(endstone::TextInput("password_input",
                                        message("messages.form_input_label", "Password To Enter The Server"), ". . ."));
    form.setSubmitButton(message("messages.form_submit", "Login"));

    form.setOnSubmit([this, uuid](endstone::Player *p, const std::string &response) {
        if (p == nullptr || !isLocked(*p)) {
            return;
        }

        const auto expected = config_->getString("maintenance.password", kDefaultPassword);
        const auto entered = formTextAt(response, kPasswordControlIndex);

        if (!expected.empty() && entered.has_value() && constantTimeEquals(*entered, expected)) {
            unlockPlayer(*p);
            p->sendMessage(ColorFormat::Green + message("messages.accepted", "Password accepted. Welcome!"));
            p->sendTitle("Welcome", "Maintenance Mode");
        }
        else {
            cancelPendingKick(uuid);
            locked_.erase(uuid);
            p->kick(message("messages.wrong_password", "Wrong Password!"));
        }
    });

    form.setOnClose([this, uuid](endstone::Player *p) {
        if (p == nullptr || !isLocked(*p)) {
            return;
        }
        cancelPendingKick(uuid);
        locked_.erase(uuid);
        p->kick(message("messages.closed", "Come Check Back Soon!"));
    });

    player.sendForm(form);
}

void MaintenancePlugin::cancelPendingKick(const endstone::UUID &uuid)
{
    if (const auto it = pending_kicks_.find(uuid); it != pending_kicks_.end()) {
        getServer().getScheduler().cancelTask(it->second);
        pending_kicks_.erase(it);
    }
}

void MaintenancePlugin::runServerCommand(const std::string &command)
{
    if (!getServer().dispatchCommand(getServer().getCommandSender(), command)) {
        getLogger().debug("Command failed: {}", command);
    }
}

void MaintenancePlugin::warnAboutPassword() const
{
    const auto password = config_->getString("maintenance.password", kDefaultPassword);
    if (password.empty()) {
        getLogger().warning("maintenance.password is empty: nobody can log in except players with bypass access.");
    }
    else if (password == kDefaultPassword) {
        getLogger().warning(
            "maintenance.password is still the default; change it in the config or with /maintenance password.");
    }
}

std::string MaintenancePlugin::message(const std::string &key, const std::string &def) const
{
    return config_ ? config_->getString(key, def) : def;
}

} // namespace maintenance
