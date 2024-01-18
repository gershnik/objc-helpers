#include <objc-helpers/NSStringUtil.h>

#include "doctest.h"

#include <sstream>
#include <format>
#include <ranges>
#include <algorithm>

using namespace std::literals;

doctest::String toString(NSString * str) {
    return str.UTF8String;
}

TEST_SUITE_BEGIN( "NSStringUtilTests" );

static_assert(sizeof(NSStringCharAccess) == 21 * sizeof(void *));
static_assert(alignof(NSStringCharAccess) == alignof(void *));

static_assert(std::is_same_v<std::ranges::iterator_t<NSStringCharAccess>, NSStringCharAccess::const_iterator>);
//static_assert(std::is_same_v<std::ranges::const_iterator_t<NSStringCharAccess>, NSStringCharAccess::const_iterator>);
static_assert(std::is_same_v<std::ranges::sentinel_t<NSStringCharAccess>, NSStringCharAccess::const_iterator>);
//static_assert(std::is_same_v<std::ranges::const_sentinel_t<NSStringCharAccess>, NSStringCharAccess::const_iterator>);
static_assert(std::is_same_v<std::ranges::range_size_t<NSStringCharAccess>, CFIndex>);
static_assert(std::is_same_v<std::ranges::range_difference_t<NSStringCharAccess>, CFIndex>);
static_assert(std::is_same_v<std::ranges::range_value_t<NSStringCharAccess>, char16_t>);
static_assert(std::is_same_v<std::ranges::range_reference_t<NSStringCharAccess>, char16_t>);
//static_assert(std::is_same_v<std::ranges::range_const_reference_t<NSStringCharAccess>, char16_t>);
static_assert(std::is_same_v<std::ranges::range_rvalue_reference_t<NSStringCharAccess>, char16_t>);
//static_assert(std::is_same_v<std::ranges::range_common_reference_t<NSStringCharAccess>, char16_t>);

static_assert(std::ranges::range<NSStringCharAccess>);
static_assert(!std::ranges::borrowed_range<NSStringCharAccess>);
static_assert(std::ranges::sized_range<NSStringCharAccess>);
static_assert(!std::ranges::view<NSStringCharAccess>);
static_assert(std::ranges::input_range<NSStringCharAccess>);
static_assert(!std::ranges::output_range<NSStringCharAccess, char16_t>);
static_assert(std::ranges::forward_range<NSStringCharAccess>);
static_assert(std::ranges::bidirectional_range<NSStringCharAccess>);
static_assert(std::ranges::random_access_range<NSStringCharAccess>);
static_assert(!std::ranges::contiguous_range<NSStringCharAccess>);
static_assert(std::ranges::common_range<NSStringCharAccess>);
static_assert(std::ranges::viewable_range<NSStringCharAccess>);
//static_assert(std::ranges::constant_range<NSStringCharAccess>);


TEST_CASE("comparison") {
    
    CHECK(NSStringEqual()(@"abc", @"abc"));
    CHECK(!NSStringEqual()(@"abc", @"abd"));
    CHECK(!NSStringEqual()(nullptr, @""));
    CHECK(!NSStringEqual()(@"", nullptr));
    CHECK(NSStringEqual()(nullptr, nullptr));
    
    CHECK(NSStringLess()(@"ab", @"abc"));
    CHECK(!NSStringLess()(@"abc", @"abc"));
    CHECK(NSStringLess()(nullptr, @"a"));
    CHECK(NSStringLess()(nullptr, @""));
    CHECK(!NSStringLess()(@"", nullptr));
    CHECK(!NSStringLess()(nullptr, nullptr));
    
    CHECK(NSStringLess()(@"AB", @"ab"));
    CHECK(!NSStringLess(NSCaseInsensitiveSearch)(@"AB", @"ab"));
    
    auto en = [NSLocale localeWithLocaleIdentifier:@"en"];
    CHECK(NSStringLocaleLess(en)(@"å", @"z"));
    auto da = [NSLocale localeWithLocaleIdentifier:@"da"];
    CHECK(NSStringLocaleLess(da)(@"z", @"å"));
    
    CHECK(NSStringLocaleLess(en)(nullptr, @""));
    CHECK(!NSStringLocaleLess(en)(@"", nullptr));
    CHECK(!NSStringLocaleLess(en)(nullptr, nullptr));
}

TEST_CASE("format") {
    
    {
        std::stringstream str;
        str << @"abc";
        CHECK(str.str() == "abc");
    }
#if __cpp_lib_format
    {
        auto str = std::format("{}", @"abc");
        CHECK(str == "abc");
    }
#endif
}

TEST_CASE("char access") {
    using namespace std::literals;
    namespace r = std::ranges;
    namespace v = std::ranges::views;
    
    //test different NSString buffer types
    {
        auto * str = @"abc";
        CHECK(r::equal(NSStringCharAccess(str), u"abc"sv));
        CHECK(r::equal(NSStringCharAccess(str) | v::reverse, u"cba"sv));
        CHECK(NSStringCharAccess(str).getString() == str);
    }
    {
        auto * str = @"яж";
        CHECK(r::equal(NSStringCharAccess(str), u"яж"sv));
        CHECK(r::equal(NSStringCharAccess(str) | v::reverse, u"жя"sv));
    }
    {
        auto * str = [NSString stringWithFormat:@"%@", @(75)];
        CHECK(r::equal(NSStringCharAccess(str), u"75"sv));
        CHECK(r::equal(NSStringCharAccess(str) | v::reverse, u"57"sv));
    }
    
    CHECK(NSStringCharAccess(nullptr).getString() == nullptr);
    CHECK(NSStringCharAccess::iterator().getString() == nullptr);
    
    auto access = NSStringCharAccess(@"abcd");
    auto it1 = access.begin() + 1;
    auto it2 = it1 + 2;
    auto substr = [access.getString() substringWithRange:{NSUInteger(it1.index()), NSUInteger(it2.index() - it1.index())}];
    CHECK(NSStringEqual()(substr, @"bc"));
    CHECK(it1.getString() == it2.getString());
    CHECK(it1 < it2);
    CHECK(it1 <= it2);
    CHECK(it2 > it1);
    CHECK(it2 >= it1);
    CHECK(access.begin() == access.cbegin());
    CHECK(access.rbegin() == access.crbegin());
    CHECK(access.end() == access.cend());
    CHECK(access.rend() == access.crend());
    CHECK(r::equal(r::subrange(access.rbegin(), access.rend()), u"dcba"sv));
    
    it1 = access.begin();
    it2 = it1++;
    CHECK(it2 == access.begin());
    CHECK(it1 - it2 == 1);
    CHECK(1 + it1 == it1 + 1);
    CHECK(it2[1] == *it1);
    it1 -= 1;
    CHECK(it1 == it2);
    it1 += 1;
    CHECK(it1 == it2 + 1);
    it1 = access.end();
    it2 = it1--;
    CHECK(it2 == access.end());
    CHECK(it1 == it2 - 1);
    
    CHECK(access.end() != NSStringCharAccess::iterator());
    
    CHECK(access[-1] == 0);
    CHECK(access[100] == 0);
    
    CHECK(!NSStringCharAccess(@"abcd").empty());
    CHECK(NSStringCharAccess(nullptr).empty());
    CHECK(NSStringCharAccess(@"").empty());
    
    CHECK(CFStringCompare(NSStringCharAccess(@"abcd").getCFString(), CFSTR("abcd"), 0) == 0);
    CHECK(NSStringCharAccess(nullptr).getCFString() == nullptr);
}

TEST_CASE("makeNSString") {
    auto eq = NSStringEqual();
    
    {
        CHECK(eq(makeNSString(u"abc"), @"abc"));
        CHECK(eq(makeNSString(u"abc"s), @"abc"));
        CHECK(eq(makeNSString(u"abc"sv), @"abc"));
        
        const char16_t * str = nullptr;
        CHECK(makeNSString(str) == nullptr);
        str = u"abc";
        CHECK(eq(makeNSString(str), @"abc"));
        
        CHECK(eq(makeNSString({u'a', u'b', u'c'}), @"abc"));
        
        std::vector<char16_t> vec = {u'a', u'b', u'c'};
        CHECK(eq(makeNSString(vec), @"abc"));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(makeNSString(span), @"abc"));
        
        auto malformed = makeNSString(u"\xD800");
        CHECK(malformed.length == 1);
        CHECK([malformed characterAtIndex:0] == u'\xD800');
    }
    
    {
        CHECK(eq(makeNSString("abc"), @"abc"));
        
        const char * str = nullptr;
        CHECK(makeNSString(str) == nullptr);
        str = "abc";
        CHECK(eq(makeNSString(str), @"abc"));
        
        CHECK(eq(makeNSString({'a', 'b', 'c'}), @"abc"));
        
        std::vector<char> vec = {'a', 'b', 'c'};
        CHECK(eq(makeNSString(vec), @"abc"));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(makeNSString(span), @"abc"));
        
        CHECK(makeNSString("\x80") == nullptr);
    }
    
    {
        CHECK(eq(makeNSString(u8"abc"), @"abc"));
        
        const char8_t * str = nullptr;
        CHECK(makeNSString(str) == nullptr);
        str = u8"abc";
        CHECK(eq(makeNSString(str), @"abc"));
        
        CHECK(eq(makeNSString({u8'a', u8'b', u8'c'}), @"abc"));
        
        std::vector<char> vec = {u8'a', u8'b', u8'c'};
        CHECK(eq(makeNSString(vec), @"abc"));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(makeNSString(span), @"abc"));
        
        CHECK(makeNSString(u8"\x80") == nullptr);
    }
    
    {
        CHECK(eq(makeNSString(u8"abc"), @"abc"));
        
        const char8_t * str = nullptr;
        CHECK((__bridge void *)makeNSString(str) == nullptr);
        str = u8"abc";
        CHECK(eq(makeNSString(str), @"abc"));
        
        CHECK(eq(makeNSString({u8'a', u8'b', u8'c'}), @"abc"));
        
        std::vector<char8_t> vec = {u8'a', u8'b', u8'c'};
        CHECK(eq(makeNSString(vec), @"abc"));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(makeNSString(span), @"abc"));
        
        CHECK(makeNSString(u8"\x80") == nullptr);
    }
    
    {
        CHECK(eq(makeNSString(U"abc"), @"abc"));
        
        const char32_t * str = nullptr;
        CHECK((__bridge void *)makeNSString(str) == nullptr);
        str = U"abc";
        CHECK(eq(makeNSString(str), @"abc"));
        
        CHECK(eq(makeNSString({U'a', U'b', U'c'}), @"abc"));
        
        std::vector<char8_t> vec = {U'a', U'b', U'c'};
        CHECK(eq(makeNSString(vec), @"abc"));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(makeNSString(span), @"abc"));
        
        CHECK(makeNSString({char32_t(0x110000)}) == nullptr);
    }
    
    {
        CHECK(eq(makeNSString(L"abc"), @"abc"));
        
        const wchar_t * str = nullptr;
        CHECK((__bridge void *)makeNSString(str) == nullptr);
        str = L"abc";
        CHECK(eq(makeNSString(str), @"abc"));
        
        CHECK(eq(makeNSString({L'a', L'b', L'c'}), @"abc"));
        
        std::vector<char8_t> vec = {L'a', L'b', L'c'};
        CHECK(eq(makeNSString(vec), @"abc"));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(makeNSString(span), @"abc"));
        
        CHECK(makeNSString({wchar_t(0x110000)}) == nullptr);
    }
    
}

TEST_CASE("makeStdString") {
    
    NSStringCharAccess access(@"abc");
    using std::views::take;
    
    auto malformed = makeNSString(u"\xD800");
    
    {
        CHECK(makeStdString<char16_t>((NSString *)nullptr) == u"");
        CHECK(makeStdString<char16_t>(@"abc") == u"abc");
        CHECK(makeStdString<char16_t>(@"abc", 1) == u"bc");
        CHECK(makeStdString<char16_t>(@"abc", 1, 1) == u"b");
        CHECK(makeStdString<char16_t>(NSStringCharAccess(@"abc") | take(2)) == u"ab");
        CHECK(makeStdString<char16_t>(access.begin(), access.end()) == u"abc");
        CHECK(makeStdString<char16_t>(malformed) == u"\xD800");
    }
    {
        CHECK(makeStdString<char>((NSString *)nullptr) == "");
        CHECK(makeStdString<char>(@"abc") == "abc");
        CHECK(makeStdString<char>(@"abc", 1) == "bc");
        CHECK(makeStdString<char>(@"abc", 1, 1) == "b");
        CHECK(makeStdString<char>(NSStringCharAccess(@"abc") | take(2)) == "ab");
        CHECK(makeStdString<char>(access.begin(), access.end()) == "abc");
        CHECK(makeStdString<char>(malformed) == "");
    }
    {
        CHECK(makeStdString<char8_t>((NSString *)nullptr) == u8"");
        CHECK(makeStdString<char8_t>(@"abc") == u8"abc");
        CHECK(makeStdString<char8_t>(@"abc", 1) == u8"bc");
        CHECK(makeStdString<char8_t>(@"abc", 1, 1) == u8"b");
        CHECK(makeStdString<char8_t>(NSStringCharAccess(@"abc") | take(2)) == u8"ab");
        CHECK(makeStdString<char8_t>(access.begin(), access.end()) == u8"abc");
        CHECK(makeStdString<char8_t>(malformed) == u8"");
    }
    {
        CHECK(makeStdString<char32_t>((NSString *)nullptr) == U"");
        CHECK(makeStdString<char32_t>(@"abc") == U"abc");
        CHECK(makeStdString<char32_t>(@"abc", 1) == U"bc");
        CHECK(makeStdString<char32_t>(@"abc", 1, 1) == U"b");
        CHECK(makeStdString<char32_t>(NSStringCharAccess(@"abc") | take(2)) == U"ab");
        CHECK(makeStdString<char32_t>(access.begin(), access.end()) == U"abc");
        CHECK(makeStdString<char32_t>(malformed) == U"");
    }
    {
        CHECK(makeStdString<wchar_t>((NSString *)nullptr) == L"");
        CHECK(makeStdString<wchar_t>(@"abc") == L"abc");
        CHECK(makeStdString<wchar_t>(@"abc", 1) == L"bc");
        CHECK(makeStdString<wchar_t>(@"abc", 1, 1) == L"b");
        CHECK(makeStdString<wchar_t>(NSStringCharAccess(@"abc") | take(2)) == L"ab");
        CHECK(makeStdString<wchar_t>(access.begin(), access.end()) == L"abc");
        CHECK(makeStdString<wchar_t>(malformed) == L"");
    }
}

TEST_SUITE_END();
