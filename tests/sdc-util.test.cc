#include "sdc.hh"

#include <gtest/gtest.h>

//
// Wildcard matching

TEST(StrUtilTest_wildcard, wildcardMatches) {
    using sdc::aux::matches_wildcard;
    EXPECT_TRUE(matches_wildcard( "foo", "foo" ));
    EXPECT_TRUE(matches_wildcard( "b*r", "bar" ));
    EXPECT_TRUE(matches_wildcard( "space*", "spacebar" ));
    EXPECT_TRUE(matches_wildcard( "*bar", "spacebar" ));
    EXPECT_TRUE(matches_wildcard( "*", "*" ));
    EXPECT_TRUE(matches_wildcard( "*", "" ));
}

TEST(StrUtilTest_wildcard, wildcardDoesntMatch) {
    using sdc::aux::matches_wildcard;
    EXPECT_FALSE(matches_wildcard( "foo", "bar" ));
    EXPECT_FALSE(matches_wildcard( "f*o", "bar" ));
    EXPECT_FALSE(matches_wildcard( "bar*", "spacebar" ));
}

//
// String trimming and tokenization

TEST(StrUtilTest_trim, doesTrimSpaces) {
    using sdc::aux::trim;
    EXPECT_EQ( trim("foo"), "foo" );
    EXPECT_EQ( trim("\t \t\nfoo bar\n"), "foo bar" );
    // ... quotes escaping?
}

TEST(StrUtilTest_tokenize, tokenizesAndTrimsByComma) {
    using sdc::aux::tokenize;
    const auto strexpr = " one, two three\n ,four,\nfive\n\tsix";
    const char expected[][32] = {"one", "two three", "four", "five\n\tsix", ""};
    auto toks = tokenize(strexpr, ',');
    int n = 0;
    for( auto token : toks ) {
        EXPECT_EQ(token, expected[n++]);
        ASSERT_LE( n, sizeof(expected)/sizeof(*expected) );
    }
}

TEST(StrUtilTest_tokenize, tokenizesAndTrimsBySpace) {
    using sdc::aux::tokenize;
    const auto strexpr = " one, two three\n ,four,\nfive\n\tsix";
    const char expected[][32] = {"one,", "two", "three", ",four,", "five", "six", ""};
    auto toks = tokenize(strexpr);
    int n = 0;
    for( auto token : toks ) {
        EXPECT_EQ(token, expected[n++]);
        ASSERT_LE( n, sizeof(expected)/sizeof(*expected) );
    }
}

//
// Check for numeric literal

TEST(StrUtilTest_is_numeric, matchesNumerical) {
    using sdc::aux::is_numeric_literal;
    EXPECT_TRUE( is_numeric_literal("0") );
    EXPECT_TRUE( is_numeric_literal("1") );
    EXPECT_TRUE( is_numeric_literal("42") );
    EXPECT_TRUE( is_numeric_literal("-0") );
    EXPECT_TRUE( is_numeric_literal("-1") );
    EXPECT_TRUE( is_numeric_literal("-.033e-64") );
    // specials
    EXPECT_TRUE( is_numeric_literal("nan") );
    EXPECT_TRUE( is_numeric_literal("NaN") );
    //EXPECT_TRUE( is_numeric_literal("+inf") );  // todo?
    //EXPECT_TRUE( is_numeric_literal("-INF") );
}

TEST(StrUtilTest_is_numeric, doesNotMatchNonNumerical) {
    using sdc::aux::is_numeric_literal;
    EXPECT_FALSE( is_numeric_literal("") );
    EXPECT_FALSE( is_numeric_literal("a") );
    EXPECT_FALSE( is_numeric_literal("abc") );
    EXPECT_FALSE( is_numeric_literal("e") );
    EXPECT_FALSE( is_numeric_literal("-e") );
    EXPECT_FALSE( is_numeric_literal("--0") );
    EXPECT_FALSE( is_numeric_literal("--1") );
    EXPECT_FALSE( is_numeric_literal("2+3") );
}

//
// Line reading

TEST(CustomGetlineTest, returnsTrimmedContent) {
    // A testing expression
    std::istringstream iss(R"ML(one
 two
three	
bar foo
Kilroy was
	 here	
)ML" );

    const struct {
        size_t lineNo;
        const char content[128];
    } expected[] = {
        {  1, "one"},
        {  2, "two"},
        {  3, "three"},
        {  4, "bar foo"},
        {  5, "Kilroy was"},
        {  6, "here"},
    };

    size_t lineNo = 0;  // must be set!
    bool rc = false;
    auto comment_f = [](const std::string & l) {  // no comments for this test
            return std::pair<size_t, size_t>(std::string::npos, std::string::npos);
        };
    std::string line;
    for( size_t i = 0; i < sizeof(expected)/sizeof(*expected); ++i ) {
        rc = sdc::aux::getline( iss, line, lineNo, comment_f );
        EXPECT_TRUE( rc );
        EXPECT_EQ( expected[i].lineNo, lineNo );
        EXPECT_EQ( expected[i].content, line );
    }
    EXPECT_FALSE( sdc::aux::getline( iss, line, lineNo, comment_f ) );
}

TEST(CustomGetlineTest, returnsValidLinesForSimpleGrammar) {
    // A testing expression
    std::istringstream iss(R"ML(# This is a comment line, be ignored, 1
foo=bar  # this must not, 2
bar = foo
  one  # some starting with space, 4

   # blah  # blank line, 6
. # blah # blah # more interestign one, 7
123 45e12, 123

blah 123	blah 456	# comment will go; preserve the tabs, 10
)ML" );

    const struct {
        size_t lineNo;
        const char content[128];
    } expected[] = {
        {  2, "foo=bar"},
        {  3, "bar = foo"},
        {  4, "one"},
        {  7, "."},
        {  8, "123 45e12, 123"},
        { 10, "blah 123	blah 456"},
    };

    size_t lineNo = 0;  // must be set!
    bool rc = false;
    auto comment_f = [](const std::string & l) {
                    return std::pair<size_t, size_t>(l.find('#'), std::string::npos);
                };
    std::string line;
    for( size_t i = 0; i < sizeof(expected)/sizeof(*expected); ++i ) {
        rc = sdc::aux::getline( iss, line, lineNo, comment_f );
        EXPECT_TRUE( rc );
        EXPECT_EQ( expected[i].lineNo, lineNo );
        EXPECT_EQ( expected[i].content, line );
    }
    EXPECT_FALSE( sdc::aux::getline( iss, line, lineNo, comment_f ) );
}

#if 0
TEST(CustomGetlineTest, treatsComplexComments) {
    // A testing expression
    std::istringstream iss(R"ML(This content must (* be ignored *) pass.
Support for (* Kilroy
was here *) multiline comments
(* bam! *)
exists.
)ML" );

    const struct {
        size_t lineNo;
        const char content[128];
    } expected[] = {
        {  1, "This content must  pass."},
        {  2, "Support for"},
        {  3, "multiline comments"},
        {  5, "exists."},
    };

    size_t lineNo = 0;  // must be set!
    bool rc = false;
    auto comment_f = [](const std::string & l) {
                    return std::pair<size_t, size_t>(l.find('#'), std::string::npos);
                };
    std::string line;
    for( size_t i = 0; i < sizeof(expected)/sizeof(*expected); ++i ) {
        rc = sdc::aux::getline( iss, line, lineNo, comment_f );
        EXPECT_TRUE( rc );
        EXPECT_EQ( expected[i].lineNo, lineNo );
        EXPECT_EQ( expected[i].content, line );
    }
    EXPECT_FALSE( sdc::aux::getline( iss, line, lineNo, comment_f ) );
}
#endif
