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
    NSStringLess(NSStringCompareOptions options = 0): m_options(options) {}
    
    bool operator()(NSString * lhs, NSString * rhs) const noexcept
    {
        if (!lhs) return rhs;
        if (!rhs) return false;
        
        return [lhs compare:rhs options:m_options range:{0, lhs.length}] == NSOrderedAscending;
    }
private:
    NSStringCompareOptions m_options = 0;
};

/**
 Comparator of NSString * for std::map, std::set etc.
 */
class NSStringLocaleLess
{
public:
    NSStringLocaleLess(NSLocale * locale, NSStringCompareOptions options = 0):
        m_options(options),
        m_locale(locale)
    {}
    
    bool operator()(NSString * lhs, NSString * rhs) const noexcept
    {
        if (!lhs) return rhs;
        if (!rhs) return false;
        
        return [lhs compare:rhs options:m_options range:{0, lhs.length} locale:m_locale] == NSOrderedAscending;
    }
private:
    NSStringCompareOptions m_options = 0;
    NSLocale * m_locale;
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


inline std::ostream & operator<<(std::ostream & stream, NSString * str)
{
    return stream << str.UTF8String;
}


#endif
