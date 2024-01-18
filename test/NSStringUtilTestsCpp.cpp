#include <objc-helpers/NSStringUtil.h>

#include "doctest.h"

using namespace std::literals;

TEST_SUITE_BEGIN( "NSStringUtilTestsCpp" );

namespace {
    struct hold {
        CFStringRef str;
        
        explicit hold(CFStringRef s): str(s)
        {}
        ~hold() {
            if (str)
                CFRelease(str);
        }
        hold(hold &&) = default;
        
        operator CFStringRef() const { return str; }
    };
}

TEST_CASE("makeCFString") {
    auto eq = [](CFStringRef lhs, CFStringRef rhs) -> bool {
        if (!lhs) return rhs;
        if (!rhs) return false;
        
        return CFStringCompare(lhs, rhs, kCFCompareForcedOrdering) == kCFCompareEqualTo;
    };
    
    
    {
        CHECK(eq(hold(makeCFString(u"")), CFSTR("")));
        CHECK(eq(hold(makeCFString(u"abc")), CFSTR("abc")));
        CHECK(eq(hold(makeCFString(u"abc"s)), CFSTR("abc")));
        CHECK(eq(hold(makeCFString(u"abc"sv)), CFSTR("abc")));
        
        const char16_t * str = nullptr;
        CHECK(hold(makeCFString(str)) == nullptr);
        str = u"abc";
        CHECK(eq(hold(makeCFString(str)), CFSTR("abc")));
        
        CHECK(eq(hold(makeCFString({u'a', u'b', u'c'})), CFSTR("abc")));
        
        std::vector<char16_t> vec = {u'a', u'b', u'c'};
        CHECK(eq(hold(makeCFString(vec)), CFSTR("abc")));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(hold(makeCFString(span)), CFSTR("abc")));
        
        auto malformed = hold(makeCFString(u"\xD800"));
        CHECK(CFStringGetLength(malformed) == 1);
        CHECK(CFStringGetCharacterAtIndex(malformed, 0) == u'\xD800');
    }
    
    {
        CHECK(eq(hold(makeCFString("")), CFSTR("")));
        CHECK(eq(hold(makeCFString("abc")), CFSTR("abc")));
        
        const char * str = nullptr;
        CHECK(hold(makeCFString(str)) == nullptr);
        str = "abc";
        CHECK(eq(hold(makeCFString(str)), CFSTR("abc")));
        
        CHECK(eq(hold(makeCFString({'a', 'b', 'c'})), CFSTR("abc")));
        
        std::vector<char> vec = {'a', 'b', 'c'};
        CHECK(eq(hold(makeCFString(vec)), CFSTR("abc")));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(hold(makeCFString(span)), CFSTR("abc")));
        
        CHECK(hold(makeCFString("\x80")) == nullptr);
    }
    
    {
        CHECK(eq(hold(makeCFString(u8"")), CFSTR("")));
        CHECK(eq(hold(makeCFString(u8"abc")), CFSTR("abc")));
        
        const char8_t * str = nullptr;
        CHECK(hold(makeCFString(str)) == nullptr);
        str = u8"abc";
        CHECK(eq(hold(makeCFString(str)), CFSTR("abc")));
        
        CHECK(eq(hold(makeCFString({u8'a', u8'b', u8'c'})), CFSTR("abc")));
        
        std::vector<char> vec = {u8'a', u8'b', u8'c'};
        CHECK(eq(hold(makeCFString(vec)), CFSTR("abc")));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(hold(makeCFString(span)), CFSTR("abc")));
        
        CHECK(hold(makeCFString(u8"\x80")) == nullptr);
    }
    
    {
        CHECK(eq(hold(makeCFString(u8"")), CFSTR("")));
        CHECK(eq(hold(makeCFString(u8"abc")), CFSTR("abc")));
        
        const char8_t * str = nullptr;
        CHECK(hold(makeCFString(str)) == nullptr);
        str = u8"abc";
        CHECK(eq(hold(makeCFString(str)), CFSTR("abc")));
        
        CHECK(eq(hold(makeCFString({u8'a', u8'b', u8'c'})), CFSTR("abc")));
        
        std::vector<char8_t> vec = {u8'a', u8'b', u8'c'};
        CHECK(eq(hold(makeCFString(vec)), CFSTR("abc")));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(hold(makeCFString(span)), CFSTR("abc")));
        
        CHECK(hold(makeCFString(u8"\x80")) == nullptr);
    }
    
    {
        CHECK(eq(hold(makeCFString(U"")), CFSTR("")));
        CHECK(eq(hold(makeCFString(U"abc")), CFSTR("abc")));
        
        const char32_t * str = nullptr;
        CHECK(hold(makeCFString(str)) == nullptr);
        str = U"abc";
        CHECK(eq(hold(makeCFString(str)), CFSTR("abc")));
        
        CHECK(eq(hold(makeCFString({U'a', U'b', U'c'})), CFSTR("abc")));
        
        std::vector<char8_t> vec = {U'a', U'b', U'c'};
        CHECK(eq(hold(makeCFString(vec)), CFSTR("abc")));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(hold(makeCFString(span)), CFSTR("abc")));
        
        CHECK(hold(makeCFString({char32_t(0x110000)})) == nullptr);
    }
    
    {
        CHECK(eq(hold(makeCFString(L"")), CFSTR("")));
        CHECK(eq(hold(makeCFString(L"abc")), CFSTR("abc")));
        
        const wchar_t * str = nullptr;
        CHECK(hold(makeCFString(str)) == nullptr);
        str = L"abc";
        CHECK(eq(hold(makeCFString(str)), CFSTR("abc")));
        
        CHECK(eq(hold(makeCFString({L'a', L'b', L'c'})), CFSTR("abc")));
        
        std::vector<char8_t> vec = {L'a', L'b', L'c'};
        CHECK(eq(hold(makeCFString(vec)), CFSTR("abc")));
        
        auto span = std::span(vec.begin(), vec.end());
        CHECK(eq(hold(makeCFString(span)), CFSTR("abc")));
        
        CHECK(hold(makeCFString({wchar_t(0x110000)})) == nullptr);
    }
    
}

TEST_CASE("makeStdString") {
    
    NSStringCharAccess access(CFSTR("abc"));
    using std::views::take;
    
    auto malformed = hold(makeCFString(u"\xD800"));
    
    {
        CHECK(makeStdString<char16_t>((CFStringRef)nullptr) == u"");
        CHECK(makeStdString<char16_t>(CFSTR("abc")) == u"abc");
        CHECK(makeStdString<char16_t>(CFSTR("abc"), 1) == u"bc");
        CHECK(makeStdString<char16_t>(CFSTR("abc"), 1, 1) == u"b");
        CHECK(makeStdString<char16_t>(CFSTR("abc"), -1, 1) == u"a");
        CHECK(makeStdString<char16_t>(CFSTR("abc"), 1, 7) == u"bc");
        CHECK(makeStdString<char16_t>(CFSTR("abc"), 3, 5) == u"");
        CHECK(makeStdString<char16_t>(CFSTR("abc"), 1, 0) == u"");
        CHECK(makeStdString<char16_t>(NSStringCharAccess(CFSTR("abc")) | take(2)) == u"ab");
        CHECK(makeStdString<char16_t>(access.begin(), access.end()) == u"abc");
        CHECK(makeStdString<char16_t>(malformed) == u"\xD800");
    }
    {
        CHECK(makeStdString<char>((CFStringRef)nullptr) == "");
        CHECK(makeStdString<char>(CFSTR("abc")) == "abc");
        CHECK(makeStdString<char>(CFSTR("abc"), 1) == "bc");
        CHECK(makeStdString<char>(CFSTR("abc"), 1, 1) == "b");
        CHECK(makeStdString<char>(CFSTR("abc"), -1, 1) == "a");
        CHECK(makeStdString<char>(NSStringCharAccess(CFSTR("abc")) | take(2)) == "ab");
        CHECK(makeStdString<char>(access.begin(), access.end()) == "abc");
        CHECK(makeStdString<char>(malformed) == "");
    }
    {
        CHECK(makeStdString<char8_t>((CFStringRef)nullptr) == u8"");
        CHECK(makeStdString<char8_t>(CFSTR("abc")) == u8"abc");
        CHECK(makeStdString<char8_t>(CFSTR("abc"), 1) == u8"bc");
        CHECK(makeStdString<char8_t>(CFSTR("abc"), 1, 1) == u8"b");
        CHECK(makeStdString<char8_t>(CFSTR("abc"), -1, 1) == u8"a");
        CHECK(makeStdString<char8_t>(NSStringCharAccess(CFSTR("abc")) | take(2)) == u8"ab");
        CHECK(makeStdString<char8_t>(access.begin(), access.end()) == u8"abc");
        CHECK(makeStdString<char8_t>(malformed) == u8"");
    }
    {
        CHECK(makeStdString<char32_t>((CFStringRef)nullptr) == U"");
        CHECK(makeStdString<char32_t>(CFSTR("abc")) == U"abc");
        CHECK(makeStdString<char32_t>(CFSTR("abc"), 1) == U"bc");
        CHECK(makeStdString<char32_t>(CFSTR("abc"), 1, 1) == U"b");
        CHECK(makeStdString<char32_t>(CFSTR("abc"), -1, 1) == U"a");
        CHECK(makeStdString<char32_t>(NSStringCharAccess(CFSTR("abc")) | take(2)) == U"ab");
        CHECK(makeStdString<char32_t>(access.begin(), access.end()) == U"abc");
        CHECK(makeStdString<char32_t>(malformed) == U"");
    }
    {
        CHECK(makeStdString<wchar_t>((CFStringRef)nullptr) == L"");
        CHECK(makeStdString<wchar_t>(CFSTR("abc")) == L"abc");
        CHECK(makeStdString<wchar_t>(CFSTR("abc"), 1) == L"bc");
        CHECK(makeStdString<wchar_t>(CFSTR("abc"), 1, 1) == L"b");
        CHECK(makeStdString<wchar_t>(CFSTR("abc"), -1, 1) == L"a");
        CHECK(makeStdString<wchar_t>(NSStringCharAccess(CFSTR("abc")) | take(2)) == L"ab");
        CHECK(makeStdString<wchar_t>(access.begin(), access.end()) == L"abc");
        CHECK(makeStdString<wchar_t>(malformed) == L"");
    }
}

TEST_SUITE_END();
