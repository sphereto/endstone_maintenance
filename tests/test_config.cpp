#include "maintenance/config.h"
#include "test_framework.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

std::filesystem::path tempDir(const std::string &name)
{
    auto dir = std::filesystem::temp_directory_path() / ("maintenance_tests_" + name);
    std::filesystem::remove_all(dir);
    return dir;
}

std::string slurp(const std::filesystem::path &p)
{
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST(config_writes_default_only_when_missing)
{
    const auto dir = tempDir("default");
    maintenance::Config cfg(dir / "config.yml");
    CHECK(cfg.saveDefault("a: 1\n"));
    CHECK_EQ(cfg.getInt("a", 0), 1);

    maintenance::Config again(dir / "config.yml");
    CHECK(again.saveDefault("a: 2\n")); // must not overwrite
    CHECK_EQ(again.getInt("a", 0), 1);
}

TEST(config_parses_types_comments_and_quotes)
{
    const auto dir = tempDir("parse");
    maintenance::Config cfg(dir / "config.yml");
    CHECK(cfg.saveDefault("# comment\n"
                          "\n"
                          "flag: yes\n"
                          "off_flag: off\n"
                          "num: 42\n"
                          "bad_num: 12abc\n"
                          "quoted: \"  spaced  \"\n"
                          "single: 'x: y'\n"
                          "url: http://example.com:8080\n"));
    CHECK(cfg.getBool("flag", false));
    CHECK(!cfg.getBool("off_flag", true));
    CHECK(cfg.getBool("missing", true));
    CHECK_EQ(cfg.getInt("num", 0), 42);
    CHECK_EQ(cfg.getInt("bad_num", -1), -1);
    CHECK_EQ(cfg.getString("quoted"), std::string("  spaced  "));
    CHECK_EQ(cfg.getString("single"), std::string("x: y"));
    CHECK_EQ(cfg.getString("url"), std::string("http://example.com:8080"));
    CHECK(!cfg.has("# comment"));
}

TEST(config_handles_windows_line_endings)
{
    const auto dir = tempDir("crlf");
    maintenance::Config cfg(dir / "config.yml");
    CHECK(cfg.saveDefault("a: hello\r\nb: true\r\n"));
    CHECK_EQ(cfg.getString("a"), std::string("hello"));
    CHECK(cfg.getBool("b", false));
}

TEST(config_set_preserves_comments_and_round_trips)
{
    const auto dir = tempDir("set");
    maintenance::Config cfg(dir / "config.yml");
    CHECK(cfg.saveDefault("# keep me\nenabled: true\n"));

    cfg.setBool("enabled", false);
    cfg.set("added", "value");
    CHECK(cfg.save());

    const auto text = slurp(dir / "config.yml");
    CHECK(text.find("# keep me") != std::string::npos);
    CHECK(text.find("enabled: false") != std::string::npos);
    CHECK(text.find("added: value") != std::string::npos);

    maintenance::Config reloaded(dir / "config.yml");
    CHECK(reloaded.reload());
    CHECK(!reloaded.getBool("enabled", true));
    CHECK_EQ(reloaded.getString("added"), std::string("value"));
}

TEST(config_lists)
{
    CHECK_EQ(maintenance::Config::splitList(" a, b ,,c ").size(), std::size_t{3});
    CHECK(maintenance::Config::splitList("").empty());
    CHECK(maintenance::Config::splitList(" , ").empty());

    const auto dir = tempDir("list");
    maintenance::Config cfg(dir / "config.yml");
    CHECK(cfg.saveDefault("names:\n"));
    CHECK(cfg.getList("names").empty());

    cfg.setList("names", {"Steve", "Alex"});
    CHECK(cfg.save());
    maintenance::Config reloaded(dir / "config.yml");
    CHECK(reloaded.reload());
    const auto names = reloaded.getList("names");
    CHECK_EQ(names.size(), std::size_t{2});
    CHECK_EQ(names[1], std::string("Alex"));
}

TEST(config_get_int_follows_stoi_rules)
{
    const auto dir = tempDir("getint");
    maintenance::Config cfg(dir / "config.yml");
    CHECK(cfg.saveDefault("plus: +5\n"
                          "minus: -3\n"
                          "padded: \"  7\"\n"
                          "trailing: 12abc\n"
                          "huge: 99999999999\n"
                          "empty:\n"
                          "sign_only: +\n"
                          "double_sign: +-5\n"
                          "hex: 0x10\n"
                          "zero: 0\n"));
    CHECK_EQ(cfg.getInt("plus", -1), 5);
    CHECK_EQ(cfg.getInt("minus", -1), -3);
    CHECK_EQ(cfg.getInt("padded", -1), 7);
    CHECK_EQ(cfg.getInt("zero", -1), 0);
    CHECK_EQ(cfg.getInt("trailing", -1), -1);
    CHECK_EQ(cfg.getInt("huge", -1), -1);
    CHECK_EQ(cfg.getInt("empty", -1), -1);
    CHECK_EQ(cfg.getInt("sign_only", -1), -1);
    CHECK_EQ(cfg.getInt("double_sign", -1), -1);
    CHECK_EQ(cfg.getInt("hex", -1), -1);
    CHECK_EQ(cfg.getInt("missing", -1), -1);
}

TEST(config_failed_reload_keeps_previous_state)
{
    const auto dir = tempDir("failed_reload");
    maintenance::Config cfg(dir / "config.yml");
    CHECK(cfg.saveDefault("# keep\nenabled: false\nname: Steve\n"));

    std::filesystem::remove(dir / "config.yml");
    CHECK(!cfg.reload());

    // The values must survive, and saving afterwards must still write the complete file (comment included).
    CHECK(!cfg.getBool("enabled", true));
    CHECK_EQ(cfg.getString("name"), std::string("Steve"));
    CHECK(cfg.save());
    const auto text = slurp(dir / "config.yml");
    CHECK(text.find("# keep") != std::string::npos);
    CHECK(text.find("name: Steve") != std::string::npos);
}

TEST(config_save_replaces_file_and_leaves_no_temp_file)
{
    const auto dir = tempDir("atomic_ok");
    maintenance::Config cfg(dir / "config.yml");
    CHECK(cfg.saveDefault("a: 1\n"));
    cfg.set("a", "2");
    CHECK(cfg.save());
    CHECK_EQ(slurp(dir / "config.yml"), std::string("a: 2\n"));
    CHECK(!std::filesystem::exists(dir / "config.yml.tmp"));
}

TEST(config_failed_save_leaves_original_untouched)
{
    const auto dir = tempDir("atomic_fail");
    maintenance::Config cfg(dir / "config.yml");
    CHECK(cfg.saveDefault("a: 1\n"));

    // A directory squatting on the temp file name makes the temp write fail, which simulates any write error.
    std::filesystem::create_directory(dir / "config.yml.tmp");
    cfg.set("a", "2");
    CHECK(!cfg.save());
    CHECK_EQ(slurp(dir / "config.yml"), std::string("a: 1\n"));
}
