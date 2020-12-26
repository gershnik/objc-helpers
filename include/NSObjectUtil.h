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



#endif
