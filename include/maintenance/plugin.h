#pragma once

#include "maintenance/config.h"

#include <endstone/endstone.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace maintenance {

class MaintenancePlugin : public endstone::Plugin {
public:
    void onEnable() override;
    void onDisable() override;
    bool onCommand(endstone::CommandSender &sender, const endstone::Command &command,
                   const std::vector<std::string> &args) override;

private:
    // Events
    void onPlayerJoin(endstone::PlayerJoinEvent &event);
    void onPlayerQuit(endstone::PlayerQuitEvent &event);
    void onPlayerMove(endstone::PlayerMoveEvent &event);
    void onPlayerChat(endstone::PlayerChatEvent &event);
    void onPlayerCommand(endstone::PlayerCommandEvent &event);
    void onPlayerInteract(endstone::PlayerInteractEvent &event);
    void onPlayerDropItem(endstone::PlayerDropItemEvent &event);
    void onBlockBreak(endstone::BlockBreakEvent &event);
    void onBlockPlace(endstone::BlockPlaceEvent &event);
    void onServerListPing(endstone::ServerListPingEvent &event);

    // Commands
    bool handleToggle(endstone::CommandSender &sender, bool enable);
    void handleStatus(endstone::CommandSender &sender) const;
    bool handleReload(endstone::CommandSender &sender);
    void handleList(endstone::CommandSender &sender) const;
    bool handleWhitelist(endstone::CommandSender &sender, bool add, const std::string &name);
    bool handlePassword(endstone::CommandSender &sender, const std::string &password);

    // Locking
    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] bool isExempt(endstone::Player &player) const;
    [[nodiscard]] bool isLocked(const endstone::Player &player) const;
    void lockPlayer(endstone::Player &player);
    void unlockPlayer(endstone::Player &player);
    void unlockEveryone();
    void showLoginForm(endstone::Player &player);
    void cancelPendingKick(const endstone::UUID &uuid);
    void runServerCommand(const std::string &command);
    void warnAboutPassword() const;
    [[nodiscard]] std::string message(const std::string &key, const std::string &def) const;

    std::unique_ptr<Config> config_;
    std::unordered_set<endstone::UUID> locked_;
    std::unordered_map<endstone::UUID, endstone::TaskId> pending_kicks_;
};

} // namespace maintenance
