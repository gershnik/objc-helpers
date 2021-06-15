/*
 Copyright 2020 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_NS_STRING_UTIL_INCLUDED
#define HEADER_NS_STRING_UTIL_INCLUDED

#include "NSObjectUtil.h"

#import <Foundation/Foundation.h>

#include <ostream>

/**
 Comparator of NSString * for std::map, std::set etc.
 */
class NSStringLess
{
public:
    NSStringLess(NSStringCompareOptions options = 0): _options(options) {}
    
    bool operator()(NSString * lhs, NSString * rhs) const noexcept
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
    NSStringLocaleLess(NSLocale * locale, NSStringCompareOptions options = 0):
        _options(options),
        _locale(locale)
    {}
    
    bool operator()(NSString * lhs, NSString * rhs) const noexcept
    {
        if (!lhs) return rhs;
        if (!rhs) return false;
        
        return [lhs compare:rhs options:_options range:{0, lhs.length} locale:_locale] == NSOrderedAscending;
    }
private:
    NSStringCompareOptions _options = 0;
    NSLocale * _locale;
};

/**
 Equality comparator.
 
 This is faster than using NSObjectEqual
 */
struct NSStringEqual
{
    bool operator()(NSString * lhs, NSString * rhs) const
    {
        if (!lhs) return !rhs;
        return [lhs isEqualToString:rhs];
    }
};

/**
 Serialization into ostream
 */
inline std::ostream & operator<<(std::ostream & stream, NSString * str)
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
    auto format(NSString * obj, FormatCtx & ctx)
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
    auto format(NSString * obj, FormatCtx & ctx)
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


/**
 Accessor for NSString characters via STL container interface.
 
 This is a **reference** class (like `std::string_view`). It does not hold a strong reference to the underlying string and is only valid
 as long as the string is valid. It is meant to be used on the stack for STL-compatible access to the string,
 not to store or pass around. As any reference class, it is safe to copy but the copy simply points to the same original.
 */
class NSStringCharAccess
{
public:
    using value_type = UniChar;
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
        
        friend const_iterator operator+(const const_iterator & lhs, int rhs) noexcept
            { return const_iterator(lhs._owner, lhs._idx + rhs); }
        friend const_iterator operator+(int lhs, const const_iterator & rhs) noexcept
            { return const_iterator(rhs._owner, rhs._idx + lhs); }
        
        friend difference_type operator-(const const_iterator & lhs, const const_iterator & rhs) noexcept
            { return lhs._idx - rhs._idx; }
        
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
    private:
        const_iterator(const NSStringCharAccess * owner, CFIndex idx) noexcept:
            _owner(owner),
            _idx(idx)
        {}
    private:
        const NSStringCharAccess * _owner = nullptr;
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
    NSStringCharAccess(NSString * str) noexcept:
        _string((__bridge CFStringRef)str)
    {
        if (_string)
        {
            _size = CFStringGetLength(_string);
            if ( (_buffer.directUniChar = CFStringGetCharactersPtr(_string)) )
                _bufferType = DirectUniChar;
            else if ( (_buffer.directCString = CFStringGetCStringPtr(_string, kCFStringEncodingASCII)) )
                _bufferType = DirectCString;
        }
    }
    
    CFStringRef getCFString() const noexcept
        { return _string; }
    
    NSString * getString() const noexcept
        { return (__bridge NSString *)_string; }


    UniChar operator[](CFIndex idx) const noexcept
    {
        if (idx < 0 || idx >= _size)
            return 0;
        
        if (_bufferType == DirectUniChar)
            return _buffer.directUniChar[idx];
        if (_bufferType == DirectCString)
            return UniChar(_buffer.directCString[idx]);
            
        if (idx >= _end || idx < _start)
            fill(idx);
        return _buffer.indirect[idx - _start];
    }
    
    CFIndex size() const noexcept
        { return _size; }
    
    bool empty() const noexcept
        { return _size; }
    
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
        _end = _start + std::size(_buffer.indirect);
        if (_end > _size)
            _end = _size;
        CFStringGetCharacters(_string, CFRangeMake(_start, _end - _start), _buffer.indirect);
    }
private:
    CFStringRef _string = nil;
    union
    {
        mutable UniChar indirect[64];
        const UniChar * directUniChar;
        const char * directCString;
    } _buffer;
    BufferType _bufferType = Indirect;
    mutable CFIndex _start = 0;
    mutable CFIndex _end = 0;
    CFIndex _size = 0;
};


#endif
