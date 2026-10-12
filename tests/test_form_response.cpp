#include "maintenance/form_response.h"
#include "test_framework.h"

#include <string>

using maintenance::constantTimeEquals;
using maintenance::formTextAt;
using maintenance::parseFormResponse;

// All JSON inputs are declared outside the CHECK macros on purpose (see test_framework.h).

TEST(form_parses_modal_form_response)
{
    const std::string json = R"([null,null,"secret"])";
    const auto parsed = parseFormResponse(json);
    CHECK_EQ(parsed.size(), std::size_t{3});
    CHECK(!parsed[0].has_value());
    CHECK(!parsed[1].has_value());
    CHECK_EQ(parsed[2].value_or(""), std::string("secret"));
}

TEST(form_text_requires_exact_match_not_substring)
{
    // The old implementation used json.find(password), which accepted any response that merely contained it.
    const std::string json = R"([null,null,"not-the-password-at-all"])";
    const auto entered = formTextAt(json, 2);
    CHECK(entered.has_value());
    CHECK(!constantTimeEquals(*entered, "password"));
    CHECK(constantTimeEquals(*entered, "not-the-password-at-all"));
}

TEST(form_decodes_escapes)
{
    const std::string quoted = R"(["a\"b\\c\/d\n"])";
    const std::string expected_quoted = "a\"b\\c/d\n";
    CHECK_EQ(formTextAt(quoted, 0).value_or(""), expected_quoted);

    const std::string accent = R"(["\u00e9"])";
    const std::string expected_accent = "\xC3\xA9";
    CHECK_EQ(formTextAt(accent, 0).value_or(""), expected_accent);

    // A surrogate pair must be combined into a single 4-byte UTF-8 sequence.
    const std::string emoji = R"(["\ud83d\ude00"])";
    const std::string expected_emoji = "\xF0\x9F\x98\x80";
    CHECK_EQ(formTextAt(emoji, 0).value_or(""), expected_emoji);

    // Raw UTF-8 in the payload passes through unchanged.
    const std::string raw_utf8 = "[\"\xC3\xA9\"]";
    CHECK_EQ(formTextAt(raw_utf8, 0).value_or(""), expected_accent);
}

TEST(form_skips_non_string_elements)
{
    const std::string json = R"([1, true, 2.5, {"a":[1,"]"]}, [1,2], "x" ])";
    const auto parsed = parseFormResponse(json);
    CHECK_EQ(parsed.size(), std::size_t{6});
    CHECK(!parsed[3].has_value());
    CHECK(!parsed[4].has_value());
    CHECK_EQ(parsed[5].value_or(""), std::string("x"));
}

TEST(form_falls_back_to_last_string_when_index_out_of_range)
{
    const std::string only = R"(["only"])";
    const std::string none = R"([null])";
    CHECK_EQ(formTextAt(only, 5).value_or(""), std::string("only"));
    CHECK(!formTextAt(none, 0).has_value());
}

TEST(form_rejects_malformed_input)
{
    const std::string unterminated = R"(["unterminated)";
    const std::string missing_comma = R"(["a" "b"])";
    const std::string bad_escape = R"(["bad \q escape"])";
    const std::string lone_surrogate = R"(["\ud83d"])";
    const std::string empty_array = "[]";

    CHECK(parseFormResponse("").empty());
    CHECK(parseFormResponse("not json").empty());
    CHECK(parseFormResponse(unterminated).empty());
    CHECK(parseFormResponse(missing_comma).empty());
    CHECK(parseFormResponse(bad_escape).empty());
    CHECK(parseFormResponse(lone_surrogate).empty());
    CHECK(parseFormResponse(empty_array).empty());
    CHECK(!formTextAt(empty_array, 0).has_value());
}

TEST(constant_time_equals_basics)
{
    CHECK(constantTimeEquals("", ""));
    CHECK(constantTimeEquals("abc", "abc"));
    CHECK(!constantTimeEquals("abc", "abd"));
    CHECK(!constantTimeEquals("abc", "abcd"));
    CHECK(!constantTimeEquals("abc", ""));
}
