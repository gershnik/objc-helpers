#include <objc-helpers/NSObjectUtil.h>

#include "doctest.h"

#include <sstream>
#include <format>

@interface Foo : NSObject
@end

@implementation Foo

-(NSUInteger) hash {
    return 42;
}

- (NSString *) description {
    return @"Foo!";
}

@end

@interface FooLoc : NSObject
@end

@implementation FooLoc

- (NSString *) descriptionWithLocale:(NSLocale *)loc {
    return @"FooLoc!";
}

- (NSString *) description {
    return @"ha";
}

@end

TEST_CASE("comparison") {
    
    CHECK(NSObjectEqual()(@(1), @(1)));
    CHECK(NSObjectEqual()(@"abc", [NSString stringWithCString:"abc" encoding:NSUTF8StringEncoding]));
    CHECK(!NSObjectEqual()(@(1), @(2)));
    CHECK(!NSObjectEqual()(nullptr, @(0)));
    CHECK(!NSObjectEqual()(@(0), nullptr));
    CHECK(NSObjectEqual()(nullptr, nullptr));
}

TEST_CASE("hash") {
 
    CHECK(NSObjectHash()([Foo new]) == 42);
}

TEST_CASE("printing") {
    
    {
        std::ostringstream str;
        str << [Foo new];
        CHECK(str.str() == "Foo!");
    }
    
    {
        std::ostringstream str;
        str << [FooLoc new];
        CHECK(str.str() == "FooLoc!");
    }
    
    {
        std::ostringstream str;
        str << (NSString *)nullptr;
        CHECK(str.str() == "nullptr");
    }
}
