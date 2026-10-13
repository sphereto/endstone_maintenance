#include "maintenance/plugin.h"

#include "maintenance/form_response.h"
#include "maintenance/movement.h"
#include "maintenance/util.h"

#include <algorithm>
#include <format>
#include <utility>

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

constexpr std::size_t kPasswordControlIndex = 2; // Label, Label, TextInput

} // namespace

void MaintenancePlugin::onEnable()
{
    config_ = std::make_unique<Config>(getDataFolder() / "config.yml");
    if (!config_->saveDefault(defaultConfigText())) {
        getLogger().error("Could not read or create {}; using built-in defaults.", config_->path().string());
    }
    refreshSettings();
    warnAboutPassword();

    registerEvent(&MaintenancePlugin::onPlayerJoin, *this);
    registerEvent(&MaintenancePlugin::onPlayerQuit, *this);
    registerEvent(&MaintenancePlugin::onServerListPing, *this);

    // `ignore_cancelled` makes Endstone skip our handler when an earlier plugin already cancelled the event.
    registerEvent(&MaintenancePlugin::onPlayerMove, *this, endstone::EventPriority::High, true);
    registerLockGuards<endstone::PlayerChatEvent, endstone::PlayerCommandEvent, endstone::PlayerInteractEvent,
                       endstone::PlayerInteractActorEvent, endstone::PlayerDropItemEvent,
                       endstone::PlayerPickupItemEvent, endstone::PlayerItemConsumeEvent, endstone::PlayerEmoteEvent,
                       endstone::BlockBreakEvent, endstone::BlockPlaceEvent>();

    getLogger().info("Maintenance mode is {}.", isEnabled() ? "ON" : "off");
}

void MaintenancePlugin::onDisable()
{
    unlockEveryone();
}

template <typename... EventTypes>
void MaintenancePlugin::registerLockGuards()
{
    (registerEvent(&MaintenancePlugin::cancelIfLocked<EventTypes>, *this, endstone::EventPriority::High, true), ...);
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
    refreshSettings();

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
    sender.sendMessage("Whitelisted players: {}", settings_.whitelist.size());
    sender.sendMessage("Kick delay: {}s", settings_.kick_delay);
}

bool MaintenancePlugin::handleReload(endstone::CommandSender &sender)
{
    if (!config_->reload()) {
        sender.sendErrorMessage("Could not read {}.", config_->path().string());
        return true;
    }
    refreshSettings();
    warnAboutPassword();
    sender.sendMessage(ColorFormat::Green + "Maintenance config reloaded.");
    return true;
}

void MaintenancePlugin::handleList(endstone::CommandSender &sender) const
{
    const auto &names = settings_.whitelist;
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
    auto names = settings_.whitelist;
    const auto lowered = toLower(name);
    const auto it = std::find_if(names.begin(), names.end(), [&](const std::string &existing) {
        return toLower(existing) == lowered;
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
    refreshSettings();
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
    refreshSettings();
    sender.sendMessage(ColorFormat::Green + "Maintenance password updated.");
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
// Events
// --------------------------------------------------------------------------------------------------------------------

void MaintenancePlugin::onPlayerJoin(endstone::PlayerJoinEvent &event)
{
    auto &player = event.getPlayer();

    // A player who left the login screen earlier may still carry our effects (they are saved with the player).
    const bool stale_effects = pending_cleanup_.erase(player.getUniqueId()) > 0;
    const bool exempt = isEnabled() && isExempt(player);
    if (stale_effects && (!isEnabled() || exempt)) {
        clearEffects(player);
    }

    if (!isEnabled()) {
        return;
    }
    if (exempt) {
        player.sendMessage(ColorFormat::Green + settings_.messages.bypass);
        return;
    }
    lockPlayer(player);
}

void MaintenancePlugin::onPlayerQuit(endstone::PlayerQuitEvent &event)
{
    const auto uuid = event.getPlayer().getUniqueId();
    cancelPendingKick(uuid);

    // The player is already disconnecting here, so effects cannot be removed reliably; remember to do it on rejoin.
    if (const auto it = locked_.find(uuid); it != locked_.end()) {
        if (it->second.effects_applied) {
            pending_cleanup_.insert(uuid);
        }
        locked_.erase(it);
    }
}

void MaintenancePlugin::onPlayerMove(endstone::PlayerMoveEvent &event)
{
    if (!isLocked(event.getPlayer())) {
        return;
    }

    // Turning the camera does not move the player. Cancelling such an event would only make Endstone teleport the
    // player back (and snap their view) for every camera turn, so only real position changes are cancelled.
    const auto &from = event.getFrom();
    const auto &to = event.getTo();
    if (isPositionalMove(to.getX() - from.getX(), to.getY() - from.getY(), to.getZ() - from.getZ())) {
        event.setCancelled(true);
    }
}

template <typename EventType>
void MaintenancePlugin::cancelIfLocked(EventType &event)
{
    if (isLocked(event.getPlayer())) {
        event.setCancelled(true);
    }
}

void MaintenancePlugin::onServerListPing(endstone::ServerListPingEvent &event)
{
    if (isEnabled() && !settings_.messages.motd.empty()) {
        event.setMotd(settings_.messages.motd);
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Locking
// --------------------------------------------------------------------------------------------------------------------

bool MaintenancePlugin::isExempt(endstone::Player &player) const
{
    return player.hasPermission("maintenance.bypass") || settings_.isWhitelisted(player.getName());
}

bool MaintenancePlugin::isLocked(const endstone::Player &player) const
{
    return !locked_.empty() && locked_.count(player.getUniqueId()) > 0;
}

void MaintenancePlugin::lockPlayer(endstone::Player &player)
{
    if (!locked_.try_emplace(player.getUniqueId(), LockState{settings_.apply_effects}).second) {
        return;
    }

    if (settings_.apply_effects) {
        applyEffects(player);
    }

    showLoginForm(player);
    player.playSound(player.getLocation(), "random.orb", 1.0F, 1.0F);
}

void MaintenancePlugin::unlockPlayer(endstone::Player &player)
{
    if (!releaseLock(player)) {
        return;
    }
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

bool MaintenancePlugin::releaseLock(endstone::Player &player)
{
    const auto uuid = player.getUniqueId();
    cancelPendingKick(uuid);

    const auto it = locked_.find(uuid);
    if (it == locked_.end()) {
        return false;
    }

    const bool had_effects = it->second.effects_applied;
    locked_.erase(it);
    if (had_effects && !clearEffects(player)) {
        pending_cleanup_.insert(uuid);
    }
    return true;
}

void MaintenancePlugin::kickLocked(endstone::Player &player, const std::string &reason)
{
    // Effects are saved with the player, so they have to be removed before the kick or they would survive the relog.
    releaseLock(player);
    player.kick(reason);
}

void MaintenancePlugin::applyEffects(endstone::Player &player)
{
    const auto target = "effect \"" + player.getName() + "\" ";
    runServerCommand(target + "blindness 1000000 255 true");
    runServerCommand(target + "slowness 1000000 255 true");
    runServerCommand(target + "resistance 1000000 255 true");
}

bool MaintenancePlugin::clearEffects(endstone::Player &player)
{
    return runServerCommand("effect \"" + player.getName() + "\" clear");
}

void MaintenancePlugin::showLoginForm(endstone::Player &player)
{
    const int delay = settings_.effectiveKickDelay();
    const auto uuid = player.getUniqueId();

    cancelPendingKick(uuid);
    if (const auto task = getServer().getScheduler().runTaskLater(
            *this,
            [this, uuid]() {
                pending_kicks_.erase(uuid);
                if (auto *p = getServer().getPlayer(uuid); p != nullptr && isLocked(*p)) {
                    kickLocked(*p, settings_.messages.timeout);
                }
            },
            static_cast<std::uint64_t>(delay) * 20)) {
        pending_kicks_[uuid] = task->getTaskId();
    }

    endstone::ModalForm form;
    form.setTitle(settings_.messages.form_title);
    form.addControl(endstone::Label(ColorFormat::Gold + "Time Left: " + ColorFormat::Green + std::to_string(delay) +
                                    "s" + ColorFormat::Reset));
    form.addControl(endstone::Label(settings_.messages.form_body));
    form.addControl(endstone::TextInput("password_input", settings_.messages.form_input_label, ". . ."));
    form.setSubmitButton(settings_.messages.form_submit);

    form.setOnSubmit([this](endstone::Player *p, const std::string &response) {
        if (p == nullptr || !isLocked(*p)) {
            return;
        }

        const auto &expected = settings_.password;
        const auto entered = formTextAt(response, kPasswordControlIndex);

        if (!expected.empty() && entered.has_value() && constantTimeEquals(*entered, expected)) {
            unlockPlayer(*p);
            p->sendMessage(ColorFormat::Green + settings_.messages.accepted);
            p->sendTitle("Welcome", "Maintenance Mode");
        }
        else {
            kickLocked(*p, settings_.messages.wrong_password);
        }
    });

    form.setOnClose([this](endstone::Player *p) {
        if (p == nullptr || !isLocked(*p)) {
            return;
        }
        kickLocked(*p, settings_.messages.closed);
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

bool MaintenancePlugin::runServerCommand(const std::string &command)
{
    const bool ok = getServer().dispatchCommand(getServer().getCommandSender(), command);
    if (!ok) {
        getLogger().debug("Command failed: {}", command);
    }
    return ok;
}

void MaintenancePlugin::refreshSettings()
{
    settings_ = Settings::fromConfig(*config_);
}

void MaintenancePlugin::warnAboutPassword() const
{
    if (settings_.password.empty()) {
        getLogger().warning("maintenance.password is empty: nobody can log in except players with bypass access.");
    }
    else if (settings_.password == Settings::kDefaultPassword) {
        getLogger().warning(
            "maintenance.password is still the default; change it in the config or with /maintenance password.");
    }
}

} // namespace maintenance
