#include "maintenance/config.h"
#include "maintenance/movement.h"
#include "maintenance/settings.h"
#include "test_framework.h"

#include <filesystem>
#include <string>

namespace {

std::filesystem::path tempDir(const std::string &name)
{
    auto dir = std::filesystem::temp_directory_path() / ("maintenance_tests_settings_" + name);
    std::filesystem::remove_all(dir);
    return dir;
}

maintenance::Settings load(const std::string &name, const std::string &yaml)
{
    maintenance::Config cfg(tempDir(name) / "config.yml");
    CHECK(cfg.saveDefault(yaml));
    return maintenance::Settings::fromConfig(cfg);
}

} // namespace

TEST(settings_empty_config_gives_defaults)
{
    const auto s = load("empty", "");
    const maintenance::Settings defaults;
    CHECK(s.enabled);
    CHECK_EQ(s.password, std::string(maintenance::Settings::kDefaultPassword));
    CHECK_EQ(s.kick_delay, 15);
    CHECK(s.apply_effects);
    CHECK(s.whitelist.empty());
    CHECK_EQ(s.messages.form_title, defaults.messages.form_title);
    CHECK(s.messages.motd.empty()); // a missing motd key must leave the server's own MOTD alone
}

// Guards against the shipped config.yml and the built-in defaults drifting apart (they used to be written twice).
TEST(settings_shipped_config_matches_builtin_defaults)
{
    const auto s = load("shipped", maintenance::defaultConfigText());
    const maintenance::Settings d;
    CHECK_EQ(s.enabled, d.enabled);
    CHECK_EQ(s.password, d.password);
    CHECK_EQ(s.kick_delay, d.kick_delay);
    CHECK_EQ(s.apply_effects, d.apply_effects);
    CHECK(s.whitelist.empty());
    CHECK_EQ(s.messages.form_title, d.messages.form_title);
    CHECK_EQ(s.messages.form_body, d.messages.form_body);
    CHECK_EQ(s.messages.form_input_label, d.messages.form_input_label);
    CHECK_EQ(s.messages.form_submit, d.messages.form_submit);
    CHECK_EQ(s.messages.timeout, d.messages.timeout);
    CHECK_EQ(s.messages.wrong_password, d.messages.wrong_password);
    CHECK_EQ(s.messages.closed, d.messages.closed);
    CHECK_EQ(s.messages.accepted, d.messages.accepted);
    CHECK_EQ(s.messages.bypass, d.messages.bypass);
    // The only intentional difference: the shipped file sets a MOTD, the code default keeps the server's own.
    CHECK(!s.messages.motd.empty());
    CHECK(d.messages.motd.empty());
}

TEST(settings_reads_values_and_ignores_malformed_ones)
{
    const auto s = load("values", "maintenance.enabled: off\n"
                                  "maintenance.password: s3cret\n"
                                  "maintenance.kick_delay: abc\n"
                                  "maintenance.apply_effects: maybe\n"
                                  "messages.form_title: Custom Title\n");
    CHECK(!s.enabled);
    CHECK_EQ(s.password, std::string("s3cret"));
    CHECK_EQ(s.kick_delay, 15); // not a number: default
    CHECK(s.apply_effects);     // not a boolean: default
    CHECK_EQ(s.messages.form_title, std::string("Custom Title"));
}

TEST(settings_effective_kick_delay_is_at_least_one_second)
{
    maintenance::Settings s;
    s.kick_delay = 0;
    CHECK_EQ(s.effectiveKickDelay(), 1);
    s.kick_delay = -5;
    CHECK_EQ(s.effectiveKickDelay(), 1);
    s.kick_delay = 20;
    CHECK_EQ(s.effectiveKickDelay(), 20);
    CHECK_EQ(s.kick_delay, 20); // the configured value itself is left as written
}

TEST(settings_whitelist_is_case_insensitive_and_keeps_original_names)
{
    const auto s = load("whitelist", "maintenance.whitelist: Steve, ALEX ,, bob\n");
    CHECK_EQ(s.whitelist.size(), std::size_t{3});
    CHECK_EQ(s.whitelist[1], std::string("ALEX"));
    CHECK(s.isWhitelisted("steve"));
    CHECK(s.isWhitelisted("STEVE"));
    CHECK(s.isWhitelisted("alex"));
    CHECK(s.isWhitelisted("Bob"));
    CHECK(!s.isWhitelisted("Herobrine"));
    CHECK(!s.isWhitelisted(""));
}

TEST(settings_set_whitelist_replaces_previous_lookup)
{
    maintenance::Settings s;
    CHECK(!s.isWhitelisted("steve"));
    s.setWhitelist({"Steve"});
    CHECK(s.isWhitelisted("STEVE"));
    s.setWhitelist({"Alex"});
    CHECK(!s.isWhitelisted("steve"));
    CHECK(s.isWhitelisted("alex"));
    s.setWhitelist({});
    CHECK(!s.isWhitelisted("alex"));
    CHECK(s.whitelist.empty());
}

TEST(movement_threshold_matches_endstone_definition)
{
    using maintenance::isPositionalMove;
    CHECK(!isPositionalMove(0.0F, 0.0F, 0.0F));    // pure camera turn
    CHECK(!isPositionalMove(0.05F, 0.0F, 0.0F));   // jitter below 1/16 block
    CHECK(!isPositionalMove(0.0625F, 0.0F, 0.0F)); // exactly the threshold: Endstone does not fire either
    CHECK(isPositionalMove(0.07F, 0.0F, 0.0F));
    CHECK(isPositionalMove(0.0F, -0.1F, 0.0F));   // falling
    CHECK(isPositionalMove(0.0F, 0.0F, 1.0F));    // walking
    CHECK(isPositionalMove(0.04F, 0.04F, 0.04F)); // small but combined over axes
}
