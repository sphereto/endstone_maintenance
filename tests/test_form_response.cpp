#include "maintenance/form_response.h"
#include "test_framework.h"

using maintenance::constantTimeEquals;
using maintenance::formTextAt;
using maintenance::parseFormResponse;

TEST(form_parses_modal_form_response)
{
    const auto parsed = parseFormResponse(R"([null,null,"secret"])");
    CHECK_EQ(parsed.size(), std::size_t{3});
    CHECK(!parsed[0].has_value());
    CHECK(!parsed[1].has_value());
    CHECK_EQ(parsed[2].value_or(""), std::string("secret"));
}

TEST(form_text_requires_exact_match_not_substring)
{
    // The old implementation used json.find(password), which accepted any response that merely contained it.
    const auto entered = formTextAt(R"([null,null,"not-the-password-at-all"])", 2);
    CHECK(entered.has_value());
    CHECK(!constantTimeEquals(*entered, "password"));
    CHECK(constantTimeEquals(*entered, "not-the-password-at-all"));
}

TEST(form_decodes_escapes)
{
    CHECK_EQ(formTextAt(R"(["a\"b\\c\/d\n"])", 0).value_or(""), std::string("a\"b\\c/d\n"));
    CHECK_EQ(formTextAt(R"(["é"])", 0).value_or(""), std::string("\xC3\xA9"));
    CHECK_EQ(formTextAt(R"(["😀"])", 0).value_or(""), std::string("\xF0\x9F\x98\x80"));
}

TEST(form_skips_non_string_elements)
{
    const auto parsed = parseFormResponse(R"([1, true, 2.5, {"a":[1,"]"]}, [1,2], "x" ])");
    CHECK_EQ(parsed.size(), std::size_t{6});
    CHECK(!parsed[3].has_value());
    CHECK(!parsed[4].has_value());
    CHECK_EQ(parsed[5].value_or(""), std::string("x"));
}

TEST(form_falls_back_to_last_string_when_index_out_of_range)
{
    CHECK_EQ(formTextAt(R"(["only"])", 5).value_or(""), std::string("only"));
    CHECK(!formTextAt(R"([null])", 0).has_value());
}

TEST(form_rejects_malformed_input)
{
    CHECK(parseFormResponse("").empty());
    CHECK(parseFormResponse("not json").empty());
    CHECK(parseFormResponse(R"(["unterminated)").empty());
    CHECK(parseFormResponse(R"(["a" "b"])").empty());
    CHECK(parseFormResponse(R"(["bad \q escape"])").empty());
    CHECK(parseFormResponse("[]").empty());
    CHECK(!formTextAt("[]", 0).has_value());
}

TEST(constant_time_equals_basics)
{
    CHECK(constantTimeEquals("", ""));
    CHECK(constantTimeEquals("abc", "abc"));
    CHECK(!constantTimeEquals("abc", "abd"));
    CHECK(!constantTimeEquals("abc", "abcd"));
    CHECK(!constantTimeEquals("abc", ""));
}
