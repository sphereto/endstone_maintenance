#pragma once

#include "maintenance/config.h"
#include "maintenance/settings.h"

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
    /** What we know about a player on the login screen. */
    struct LockState {
        bool effects_applied = false; // blindness / slowness / resistance must be removed again when the lock ends
    };

    // Events
    void onPlayerJoin(endstone::PlayerJoinEvent &event);
    void onPlayerQuit(endstone::PlayerQuitEvent &event);
    void onPlayerMove(endstone::PlayerMoveEvent &event);
    void onServerListPing(endstone::ServerListPingEvent &event);

    /** Cancels any event caused by a player who is still on the login screen. */
    template <typename EventType>
    void cancelIfLocked(EventType &event);

    /** Registers cancelIfLocked for every listed event type. */
    template <typename... EventTypes>
    void registerLockGuards();

    // Commands
    bool handleToggle(endstone::CommandSender &sender, bool enable);
    void handleStatus(endstone::CommandSender &sender) const;
    bool handleReload(endstone::CommandSender &sender);
    void handleList(endstone::CommandSender &sender) const;
    bool handleWhitelist(endstone::CommandSender &sender, bool add, const std::string &name);
    bool handlePassword(endstone::CommandSender &sender, const std::string &password);

    // Locking
    [[nodiscard]] bool isEnabled() const
    {
        return settings_.enabled;
    }
    [[nodiscard]] bool isExempt(endstone::Player &player) const;
    [[nodiscard]] bool isLocked(const endstone::Player &player) const;
    void lockPlayer(endstone::Player &player);
    void unlockPlayer(endstone::Player &player);
    void unlockEveryone();
    /** Ends the lock and removes the effects we applied. Returns false if the player was not locked. */
    bool releaseLock(endstone::Player &player);
    /** Ends the lock (and its effects) and then kicks the player. */
    void kickLocked(endstone::Player &player, const std::string &reason);
    void applyEffects(endstone::Player &player);
    bool clearEffects(endstone::Player &player);
    void showLoginForm(endstone::Player &player);
    void cancelPendingKick(const endstone::UUID &uuid);
    bool runServerCommand(const std::string &command);
    void refreshSettings();
    void warnAboutPassword() const;

    std::unique_ptr<Config> config_;
    Settings settings_;
    std::unordered_map<endstone::UUID, LockState> locked_;
    std::unordered_map<endstone::UUID, endstone::TaskId> pending_kicks_;
    // Players who left the login screen with our effects still on them and whose effect removal did not (or could not)
    // happen; they are cleaned up the next time they join without being locked again.
    std::unordered_set<endstone::UUID> pending_cleanup_;
};

} // namespace maintenance
