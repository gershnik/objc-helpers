/*
 Copyright 2020 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_NS_OBJECT_UTIL_INCLUDED
#define HEADER_NS_OBJECT_UTIL_INCLUDED

#import <Foundation/Foundation.h>

#include <ostream>

#if __cpp_lib_format > 201907
    #include <format>
#endif


#if __has_include(<fmt/format.h>)
    #define NS_OBJECT_UTIL_USE_FMT
    #include <fmt/format.h>
#elif __has_include("fmt/format.h")
    #define NS_OBJECT_UTIL_USE_FMT
    #include "fmt/format.h"
#endif

/**
 Equality comparison of NSObject * for std::unordered_map, std::unordered_set etc.
 */
struct NSObjectEqual
{
    bool operator()(id<NSObject> lhs, id<NSObject> rhs) const
    {
        if (!lhs) return !rhs;
        return [lhs isEqual:rhs];
    }
};

/**
 Hash code of NSObject * for std::unordered_map, std::unordered_set etc.
 */
struct NSObjectHash
{
    size_t operator()(id<NSObject> obj) const
    {
        return obj.hash;
    }
};

/**
 Serialization into ostream
 */
inline std::ostream & operator<<(std::ostream & stream, id<NSObject> obj)
{
    if (!obj)
        return stream << nullptr;
    
    if ([obj respondsToSelector:@selector(descriptionWithLocale:)])
        return stream << [(id)obj descriptionWithLocale:NSLocale.currentLocale].UTF8String;
    return stream << obj.description.UTF8String;
}

#if __cpp_lib_format > 201907

/**
 Serialization into std::format
 */
template<class T>
struct std::formatter<T, std::enable_if_t<std::is_convertible_v<T, id<NSObject>>, char>> : std::formatter<std::string_view>
{
    template<class FormatCtx>
    auto format(id<NSObject> obj, FormatCtx & ctx)
    {
        const char * str;
        
        if (!obj)
            str = "<null>";
        else if ([obj respondsToSelector:@selector(descriptionWithLocale:)])
            str = [(id)obj descriptionWithLocale:NSLocale.currentLocale].UTF8String;
        else
            str = obj.description.UTF8String;
        
        return std::formatter<std::string_view>::format(str, ctx);
    }
};

#endif

#ifdef NS_OBJECT_UTIL_USE_FMT

/**
 Serialization into fmt::format
 */
template<class T>
struct fmt::formatter<T, std::enable_if_t<std::is_convertible_v<T, id<NSObject>>, char>> : fmt::formatter<std::string_view>
{
    template<class FormatCtx>
    auto format(id<NSObject> obj, FormatCtx & ctx)
    {
        const char * str;
        
        if (!obj)
            str = "<null>";
        else if ([obj respondsToSelector:@selector(descriptionWithLocale:)])
            str = [(id)obj descriptionWithLocale:NSLocale.currentLocale].UTF8String;
        else
            str = obj.description.UTF8String;
        
        return fmt::formatter<std::string_view>::format(str, ctx);
    }
};

#endif

#endif
