/*
 Copyright 2020 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_NS_OBJECT_UTIL_INCLUDED
#define HEADER_NS_OBJECT_UTIL_INCLUDED

#ifdef __OBJC__

#import <Foundation/Foundation.h>

#include <ostream>

#if (__cpp_lib_format >= 201907L || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 170000)) && __has_include(<format>)
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
    bool operator()(id<NSObject> __nullable lhs, id<NSObject> __nullable rhs) const
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
    size_t operator()(id<NSObject> __nullable obj) const
    {
        return obj.hash;
    }
};

/**
 Serialization into ostream
 */
inline std::ostream & operator<<(std::ostream & stream, id<NSObject> __nullable obj)
{
    if (!obj)
        return stream << nullptr;
    
    if ([obj respondsToSelector:@selector(descriptionWithLocale:)])
        return stream << [(id)obj descriptionWithLocale:NSLocale.currentLocale].UTF8String;
    return stream << obj.description.UTF8String;
}

#if (__cpp_lib_format >= 201907L || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 170000)) && __has_include(<format>)

/**
 Serialization into std::format
 */
template<class T>
struct std::formatter<T, std::enable_if_t<std::is_convertible_v<T, id<NSObject>>, char>> : std::formatter<std::string_view>
{
    template<class FormatCtx>
    auto format(id<NSObject> __nullable obj, FormatCtx & ctx)
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

namespace fmt
{
    template<class T>
    struct nsptr_holder
    {
        T * __nullable ptr;
    };
    
    template <typename T>
    std::enable_if_t<std::is_convertible_v<T, id<NSObject>>,
    nsptr_holder<std::remove_pointer_t<T>>>
    nsptr(T p) { return {p}; }
}



/**
 Serialization into older fmt::format
 */
template<class T>
struct fmt::formatter<T, std::enable_if_t<std::is_convertible_v<T, id<NSObject>>, char>> : fmt::formatter<std::string_view>
{
    template<class FormatCtx>
    auto format(id<NSObject> __nullable obj, FormatCtx & ctx) const
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

/**
 Serialization into newer fmt::format
 */
template<class T>
struct fmt::formatter<fmt::nsptr_holder<T>, std::enable_if_t<std::is_convertible_v<T *, id<NSObject>>, char>> : fmt::formatter<std::string_view>
{
    template<class FormatCtx>
    auto format(fmt::nsptr_holder<T> holder, FormatCtx & ctx) const
    {
        const char * str;
        
        if (!holder.ptr)
            str = "<null>";
        else if ([holder.ptr respondsToSelector:@selector(descriptionWithLocale:)])
            str = [(id)holder.ptr descriptionWithLocale:NSLocale.currentLocale].UTF8String;
        else
            str = holder.ptr.description.UTF8String;
        
        return fmt::formatter<std::string_view>::format(str, ctx);
    }
};

#endif

#endif //__OBJC__

#endif
