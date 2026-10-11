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
