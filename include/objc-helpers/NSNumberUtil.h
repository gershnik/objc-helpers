/*
 Copyright 2020 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_NS_NUMBER_UTIL_INCLUDED
#define HEADER_NS_NUMBER_UTIL_INCLUDED

#include "NSObjectUtil.h"

#ifdef __OBJC__

#import <Foundation/Foundation.h>

/**
 Comparator of NSNumber * for std::map, std::set etc.
 */
struct NSNumberLess
{
public:
    
    bool operator()(NSNumber * lhs, NSNumber * rhs) const noexcept
    {
        if (!lhs) return rhs;
        if (!rhs) return false;
        
        return [lhs compare:rhs] == NSOrderedAscending;
    }
};


/**
 Equality comparator.
 
 This is faster than using NSObjectEqual
 */
struct NSNumberEqual
{
    bool operator()(NSNumber * lhs, NSNumber * rhs) const
    {
        if (!lhs) return !rhs;
        if (!rhs) return false;
        return [lhs isEqualToNumber:rhs];
    }
};

#endif  //__OBJC__

#endif
