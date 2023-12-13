#include <objc-helpers/NSStringUtil.h>

#include "doctest.h"

#include <sstream>
#include <format>
#include <ranges>
#include <algorithm>

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
    
    auto access = NSStringCharAccess(@"abcd");
    auto it1 = access.begin() + 1;
    auto it2 = it1 + 2;
    auto substr = [access.getString() substringWithRange:{NSUInteger(it1.index()), NSUInteger(it2.index() - it1.index())}];
    CHECK(NSStringEqual()(substr, @"bc"));
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
}
