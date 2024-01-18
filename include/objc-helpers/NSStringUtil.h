/*
 Copyright 2020 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_NS_STRING_UTIL_INCLUDED
#define HEADER_NS_STRING_UTIL_INCLUDED

#if __cplusplus < 202002L
    #error This header requires C++20 mode or above
#endif

#include <version>

#if __cpp_lib_ranges < 201911L
    #error This header requires C++20 mode or above with ranges support
#endif

#include "NSObjectUtil.h"

#ifdef __OBJC__
    #import <Foundation/Foundation.h>
#else
    #include <CoreFoundation/CoreFoundation.h>
#endif

#include <ranges>
#include <string_view>
#include <ostream>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-extension"


#ifdef __OBJC__

/**
 Comparator of NSString * for std::map, std::set etc.
 */
class NSStringLess
{
public:
    NSStringLess(NSStringCompareOptions options = 0): _options(options) {}
    
    bool operator()(NSString * __nullable lhs, NSString * __nullable rhs) const noexcept
    {
        if (!lhs) return rhs;
        if (!rhs) return false;
        
        return [lhs compare:rhs options:_options range:{0, lhs.length}] == NSOrderedAscending;
    }
private:
    NSStringCompareOptions _options = 0;
};

/**
 Comparator of NSString * for std::map, std::set etc.
 */
class NSStringLocaleLess
{
public:
    NSStringLocaleLess(NSLocale * __nullable locale, NSStringCompareOptions options = 0):
        _options(options),
        _locale(locale)
    {}
    
    bool operator()(NSString * __nullable lhs, NSString * __nullable rhs) const noexcept
    {
        if (!lhs) return rhs;
        if (!rhs) return false;
        
        return [lhs compare:rhs options:_options range:{0, lhs.length} locale:_locale] == NSOrderedAscending;
    }
private:
    NSStringCompareOptions _options = 0;
    NSLocale * __nullable _locale;
};

/**
 Equality comparator.
 
 This is faster than using NSObjectEqual
 */
struct NSStringEqual
{
    bool operator()(NSString * __nullable lhs, NSString * __nullable rhs) const
    {
        if (!lhs) return !rhs;
        return [lhs isEqualToString:rhs];
    }
};

/**
 Serialization into ostream
 */
inline std::ostream & operator<<(std::ostream & stream, NSString * __nullable str)
{
    return stream << str.UTF8String;
}

#if __cpp_lib_format > 201907

/**
 Serialization into std::format
 */
template<>
struct std::formatter<NSString *> : std::formatter<std::string_view>
{
    template<class FormatCtx>
    auto format(NSString * __nullable obj, FormatCtx & ctx)
    {
        const char * str;
        
        if (!obj)
            str = "<null>";
        else
            str = obj.UTF8String;
        
        return std::formatter<std::string_view>::format(str, ctx);
    }
};

#endif

#ifdef NS_OBJECT_UTIL_USE_FMT

/**
 Serialization into fmt::format
 */
template<>
struct fmt::formatter<NSString *> : fmt::formatter<std::string_view>
{
    template<class FormatCtx>
    auto format(NSString * __nullable obj, FormatCtx & ctx)
    {
        const char * str;
        
        if (!obj)
            str = "null";
        else
            str = obj.UTF8String;
        
        return fmt::formatter<std::string_view>::format(str, ctx);
    }
};

#endif

#endif //__OBJC__


template<class Char>
concept CharTypeConvertibleWithNSString =
    std::is_same_v<Char, char> ||
    std::is_same_v<Char, char8_t> ||
    std::is_same_v<Char, char16_t> ||
    std::is_same_v<Char, char32_t> ||
    std::is_same_v<Char, wchar_t>
;

template<class Char> struct kCFStringEncodingForImpl;
template<> struct kCFStringEncodingForImpl<char>     { static constexpr auto value = kCFStringEncodingUTF8; };
template<> struct kCFStringEncodingForImpl<char8_t>  { static constexpr auto value = kCFStringEncodingUTF8; };
template<> struct kCFStringEncodingForImpl<char32_t> { static constexpr auto value = kCFStringEncodingUTF32LE; };
template<> struct kCFStringEncodingForImpl<wchar_t>  { static constexpr auto value = kCFStringEncodingUTF32LE; };

template<class Char>
constexpr CFStringBuiltInEncodings kCFStringEncodingFor = kCFStringEncodingForImpl<Char>::value;


/**
 Accessor for NSString characters via STL container interface.
 
 This is a **reference** class (like `std::string_view`). It does not hold a strong reference to the underlying string and is only valid
 as long as the string is valid. It is meant to be used on the stack for STL-compatible access to the string,
 not to store or pass around. As any reference class, it is safe to copy but the copy simply points to the same original.
 */
class NSStringCharAccess
{
public:
    using value_type = char16_t;
    using size_type = CFIndex;
    using difference_type = decltype(CFIndex(0) - CFIndex(0));
    using reference = value_type;
    using pointer = void;
    
    class const_iterator
    {
    friend class NSStringCharAccess;
    public:
        using value_type = NSStringCharAccess::value_type;
        using size_type = NSStringCharAccess::size_type;
        using difference_type = NSStringCharAccess::difference_type;
        using reference = NSStringCharAccess::reference;
        using pointer = NSStringCharAccess::pointer;
        using iterator_category = std::random_access_iterator_tag;
    public:
        const_iterator() noexcept
        {}
        
        value_type operator*() const noexcept
            { return (*_owner)[_idx]; }
        value_type operator[](difference_type i) const noexcept
            { return (*_owner)[_idx + i]; }
        
        const_iterator & operator++() noexcept
            { ++_idx; return *this; }
        const_iterator operator++(int) noexcept
            { return const_iterator(_owner, _idx++);  }
        const_iterator & operator+=(difference_type val) noexcept
            { _idx += val; return *this; }
        const_iterator & operator--() noexcept
            { --_idx; return *this; }
        const_iterator operator--(int) noexcept
            { return const_iterator(_owner, _idx--);  }
        const_iterator & operator-=(difference_type val) noexcept
            { _idx -= val; return *this; }
        
        friend const_iterator operator+(const const_iterator & lhs, difference_type rhs) noexcept
            { return const_iterator(lhs._owner, lhs._idx + rhs); }
        friend const_iterator operator+(difference_type lhs, const const_iterator & rhs) noexcept
            { return const_iterator(rhs._owner, rhs._idx + lhs); }
        
        friend difference_type operator-(const const_iterator & lhs, const const_iterator & rhs) noexcept
            { return lhs._idx - rhs._idx; }
        friend const_iterator operator-(const const_iterator & lhs, int rhs) noexcept
            { return const_iterator(lhs._owner, lhs._idx - rhs); }
        
        friend bool operator==(const const_iterator & lhs, const const_iterator & rhs) noexcept
            { return lhs._idx == rhs._idx; }
        friend bool operator!=(const const_iterator & lhs, const const_iterator & rhs) noexcept
            { return lhs._idx != rhs._idx; }
        friend bool operator<(const const_iterator & lhs, const const_iterator & rhs) noexcept
            { return lhs._idx < rhs._idx; }
        friend bool operator<=(const const_iterator & lhs, const const_iterator & rhs) noexcept
            { return lhs._idx <= rhs._idx; }
        friend bool operator>(const const_iterator & lhs, const const_iterator & rhs) noexcept
            { return lhs._idx > rhs._idx; }
        friend bool operator>=(const const_iterator & lhs, const const_iterator & rhs) noexcept
            { return lhs._idx >= rhs._idx; }
        
        CFIndex index() const noexcept
            { return _idx; }
        
        CFStringRef __nullable getCFString() const noexcept
            { return _owner ? _owner->_string : nullptr; }
#ifdef __OBJC__
        NSString * __nullable getString() const noexcept
            { return (__bridge NSString *)getCFString(); }
#endif
        
    private:
        const_iterator(const NSStringCharAccess * __nonnull owner, CFIndex idx) noexcept:
            _owner(owner),
            _idx(idx)
        {}
    private:
        const NSStringCharAccess * __nullable _owner = nullptr;
        CFIndex _idx = -1;
    };
    
    using iterator=const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = std::reverse_iterator<iterator>;
private:
    enum BufferType
    {
        DirectUniChar,
        DirectCString,
        Indirect
    };
public:
    NSStringCharAccess(std::nullptr_t) noexcept:
        _string(nullptr)
    {}
    
#ifdef __OBJC__
    NSStringCharAccess(NSString * __nullable str) noexcept:
        NSStringCharAccess((__bridge CFStringRef)str)
    {}
#endif
    
    NSStringCharAccess(CFStringRef __nullable str) noexcept:
        _string(str)
    {
        if (_string)
        {
            _size = CFStringGetLength(_string);
            if ( (_buffer.directUniChar = (char16_t *)CFStringGetCharactersPtr(_string)) )
                _bufferType = DirectUniChar;
            else if ( (_buffer.directCString = CFStringGetCStringPtr(_string, kCFStringEncodingASCII)) )
                _bufferType = DirectCString;
        }
    }
    
    CFStringRef __nullable getCFString() const noexcept
        { return _string; }
    
#ifdef __OBJC__
    NSString * __nullable getString() const noexcept
        { return (__bridge NSString *)_string; }
#endif


    value_type operator[](CFIndex idx) const noexcept
    {
        if (idx < 0 || idx >= _size)
            return 0;
        
        if (_bufferType == DirectUniChar)
            return _buffer.directUniChar[idx];
        if (_bufferType == DirectCString)
            return value_type(_buffer.directCString[idx]);
            
        if (idx >= _end || idx < _start)
            fill(idx);
        return _buffer.indirect[idx - _start];
    }
    
    CFIndex size() const noexcept
        { return _size; }
    
    bool empty() const noexcept
        { return _size == 0; }
    
    const_iterator begin() const noexcept
        { return const_iterator(this, 0); }
    const_iterator cbegin() const noexcept
        { return const_iterator(this, 0); }
    const_iterator end() const noexcept
        { return const_iterator(this, size()); }
    const_iterator cend() const noexcept
        { return const_iterator(this, size()); }
    
    const_reverse_iterator rbegin() const noexcept
        { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept
        { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept
        { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept
        { return const_reverse_iterator(begin()); }
    
private:
    void fill(CFIndex idx) const
    {
        if ((_start = idx - 4) < 0)
            _start = 0;
        _end = _start + CFIndex(std::size(_buffer.indirect));
        if (_end > _size)
            _end = _size;
        CFStringGetCharacters(_string, CFRangeMake(_start, _end - _start), (UniChar *)_buffer.indirect);
    }
private:
    CFStringRef __nullable _string = nil;
    union
    {
        mutable char16_t indirect[64];
        const char16_t * __nonnull directUniChar;
        const char * __nonnull directCString;
    } _buffer;
    BufferType _bufferType = Indirect;
    mutable CFIndex _start = 0;
    mutable CFIndex _end = 0;
    CFIndex _size = 0;
};

/**
 Converts CFStringRef to `std::basic_string<Char>`
 
 @tparam Char character type of the resulatant std::basic_string
 @param str string to convert. If nullptr an empty string is returned
 @param start start index. If less than 0 assumed to be 0. If greater than length of the string an empty string is returned.
 @param length number of source characters to convert. If less than 0 or start + length exceeds the length of the string assumed "to the end of string".
 
 Convertions to char16_t are exact and never fail. Conversions to other character types are transcodings and can fail if source string contains invalid UTF-16
 sequences. In such cases an empty string is returned. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<CharTypeConvertibleWithNSString Char>
auto makeStdString(CFStringRef __nullable str, CFIndex start = 0, CFIndex length = -1) {
    using RetType = std::basic_string<Char>;
    
    if (!str)
        return RetType();
    auto strLength = CFStringGetLength(str);
    if (start >= strLength)
        return RetType();
    if (start < 0)
        start = 0;
    if (length < 0)
        length = strLength - start;
    else
        length = std::min(length, strLength - start);

    if constexpr (std::is_same_v<Char, char16_t>) {
        RetType ret(size_t(length), '\0');
        CFStringGetCharacters(str, {start, length}, (UniChar *)ret.data());
        return ret;
    } else {
        constexpr auto enc = kCFStringEncodingFor<Char>;
        CFIndex bufLen = 0;
        auto res = CFStringGetBytes(str, {start, length}, enc, 0, false, nullptr, 0, &bufLen);
        if (res != length)
            return RetType();
        RetType ret(size_t(bufLen) / sizeof(Char), '\0');
        CFStringGetBytes(str, {start, length}, enc, 0, false, (UInt8 *)ret.data(), bufLen, nullptr);
        return ret;
    }
}

#ifdef __OBJC__

/**
 Converts NSString to `std::basic_string<Char>`
 
 @tparam Char character type of the resulatant std::basic_string
 @param str string to convert. If nil an empty string is returned
 @param location start index. If greater than length of the string an empty string is returned.
 @param length number of source characters to convert. If start + length exceeds the length of the string assumed "to the end of string".
 
 Convertions to char16_t are exact and never fail. Conversions to other character types are transcodings and can fail if source string contains invalid UTF-16
 sequences. In such cases an empty string is returned. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<CharTypeConvertibleWithNSString Char>
inline auto makeStdString(NSString * __nullable str, NSUInteger location = 0, NSUInteger length = NSUInteger(-1)) {
    return makeStdString<Char>((__bridge CFStringRef)str, CFIndex(location), CFIndex(length));
}

#endif

/**
 Converts a range denoted by a pair of  NSStringCharAccess::const_iterator to `std::basic_string<Char>`
 
 @tparam Char character type of the resulatant std::basic_string
 
 If iterators do not form a valid substring of some NSString the behavior is undefined. If first and alst are both uninitialized iterators the result is an empty string.
 
 Convertions to char16_t are exact and never fail. Conversions to other character types are transcodings and can fail if source string contains invalid UTF-16
 sequences. In such cases an empty string is returned. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<CharTypeConvertibleWithNSString Char>
inline auto makeStdString(NSStringCharAccess::const_iterator first, NSStringCharAccess::const_iterator last) {
    return makeStdString<Char>(first.getCFString(), first.index(), last.index() - first.index());
}

/**
 Converts NSStringCharAccess or a view derived from it to `std::basic_string<Char>`
 
 @tparam Char character type of the resulatant std::basic_string
 
 A dervied view must produce the original view iterators. Thus, for example, `std::views::take(N) would work while
 `std::views::reverse` will not (and won't compile) since the latter's iterators are not the same as NSStringCharAccess ones.
 
 If iterators do not form a valid substring of some NSString the behavior is undefined. If first and alst are both uninitialized iterators the result is an empty string.
 
 Convertions to char16_t are exact and never fail. Conversions to other character types are transcodings and can fail if source string contains invalid UTF-16
 sequences. In such cases an empty string is returned. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<CharTypeConvertibleWithNSString Char, std::ranges::common_range Range>
requires(std::is_same_v<std::ranges::iterator_t<std::remove_cvref_t<Range>>, NSStringCharAccess::const_iterator>)
inline auto makeStdString(Range && range) {
    return makeStdString<Char>(std::ranges::begin(range), std::ranges::end(range));
}

/**
 Converts any contiguous range of characters to CFString
 
 @returns nullptr on failure
 
 The type of range's characters can be any of: char, char16_t, char32_t, char8_t, wchar_t.
 
 Convertions from char16_t are exact and can only fail if out of memory. Conversions from other character types are
 transcodings and can fail if source string contains invalid UTF sequences. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<std::ranges::contiguous_range CharRange>
requires(CharTypeConvertibleWithNSString<std::ranges::range_value_t<CharRange>>)
inline auto makeCFString(const CharRange & range)  -> CFStringRef __nullable {
    if (range.empty())
        return CFSTR("");
    using Char = std::ranges::range_value_t<CharRange>;
    if constexpr (std::is_same_v<Char, char16_t>) {
        return CFStringCreateWithCharacters(nullptr, (const UniChar *)range.data(), CFIndex(range.size()));
    } else {
        return CFStringCreateWithBytes(nullptr, (const UInt8 *)range.data(), CFIndex(range.size() * sizeof(Char)),
                                       kCFStringEncodingFor<Char>, false);
    }
}

/**
 Converts a null terminated character string to CFString
 
 @returns nullptr on failure
 
 The type of range's characters can be any of: char, char16_t, char32_t, char8_t, wchar_t.
 
 Convertions from char16_t are exact and can only fail if out of memory. Conversions from other character types are
 transcodings and can fail if source string contains invalid UTF sequences. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<CharTypeConvertibleWithNSString Char>
inline auto makeCFString(const Char * __nullable str) -> CFStringRef __nullable {
    if (!str)
        return nullptr;
    return makeCFString(std::basic_string_view<Char>(str));
}

/**
 Converts an initializer list of characters to CFString
 
 @returns nullptr on failure
 
 The type of range's characters can be any of: char, char16_t, char32_t, char8_t, wchar_t.
 
 Convertions from char16_t are exact and can only fail if out of memory. Conversions from other character types are
 transcodings and can fail if source string contains invalid UTF sequences. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<CharTypeConvertibleWithNSString Char>
inline auto makeCFString(const std::initializer_list<Char> & str) {
    return makeCFString(std::basic_string_view<Char>(str.begin(), str.size()));
}


#ifdef __OBJC__

/**
 Converts any contiguous range of characters to NSString
 
 @returns nil on failure
 
 The type of range's characters can be any of: char, char16_t, char32_t, char8_t, wchar_t.
 
 Convertions from char16_t are exact and can only fail if out of memory. Conversions from other character types are
 transcodings and can fail if source string contains invalid UTF sequences. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<std::ranges::contiguous_range CharRange>
requires(CharTypeConvertibleWithNSString<std::ranges::range_value_t<CharRange>>)
inline auto makeNSString(const CharRange & range) {
    return (__bridge_transfer NSString *)makeCFString(range);
}

/**
 Converts a null terminated character string to NSString
 
 @returns nil on failure
 
 The type of range's characters can be any of: char, char16_t, char32_t, char8_t, wchar_t.
 
 Convertions from char16_t are exact and can only fail if out of memory. Conversions from other character types are
 transcodings and can fail if source string contains invalid UTF sequences. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<CharTypeConvertibleWithNSString Char>
inline auto makeNSString(const Char * __nullable str) {
    return (__bridge_transfer NSString *)makeCFString(str);
}

/**
 Converts an initializer list of characters to NSString
 
 @returns nil on failure
 
 The type of range's characters can be any of: char, char16_t, char32_t, char8_t, wchar_t.
 
 Convertions from char16_t are exact and can only fail if out of memory. Conversions from other character types are
 transcodings and can fail if source string contains invalid UTF sequences. Encodings of char and wchar_t are UTF-8 and UTF-32 respectively.
 */
template<CharTypeConvertibleWithNSString Char>
inline auto makeNSString(const std::initializer_list<Char> & str) {
    return (__bridge_transfer NSString *)makeCFString(str);
}

#endif

#pragma clang diagnostic pop

#endif

