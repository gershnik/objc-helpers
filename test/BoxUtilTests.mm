#include <objc-helpers/BoxUtil.h>

#include "doctest.h"


TEST_SUITE_BEGIN( "BoxUtilTests" );

TEST_CASE( "integer" ) {
    auto obj = box(42);
    static_assert(std::is_same_v<decltype(obj), NSObject<BoxedValue, BoxedComparable, NSCopying> *>);
    CHECK(boxedValue<int>(obj) == 42);
    CHECK([obj.description isEqualToString:@"42"]);
    CHECK(obj.hash == std::hash<int>()(42));
    
    @try {
        boxedValue<long>(obj);
        FAIL("able to unbox wrong type");
    } @catch (NSException * exc) {
        CHECK([exc.name isEqualToString:NSInvalidArgumentException]);
    }
    
    auto objc = (decltype(obj))[obj copy];
    CHECK(boxedValue<int>(objc) == 42);
    
    CHECK([objc isEqual:obj]);
    CHECK([objc isEqualTo:obj]);
    
    CHECK(![objc isEqual:nullptr]);
    CHECK([objc isEqual:objc]);
    CHECK(![objc isEqual:@(42)]);
    
    CHECK([box(5) compare:box(6)] == NSOrderedAscending);
    CHECK([box(6) compare:box(5)] == NSOrderedDescending);
    CHECK([box(6) compare:box(6)] == NSOrderedSame);
    
    auto b = box(27);
    CHECK([b compare:b] == NSOrderedSame);
    
    @try {
        NSObject<BoxedComparable> * n;
        [box(5) compare:n];
        FAIL("able to compare with nil");
    } @catch (NSException * exc) {
        CHECK([exc.name isEqualToString:NSInvalidArgumentException]);
    }
    
    @try {
        [box(5) compare:box(5l)];
        FAIL("able to compare with different class");
    } @catch (NSException * exc) {
        CHECK([exc.name isEqualToString:NSInvalidArgumentException]);
    }
    
    @try {
        [[maybe_unused]] auto obj1 = (NSObject *)[[obj.class alloc] init];
        FAIL("able to call init");
    } @catch (NSException * exc) {
        CHECK([exc.name isEqualToString:NSInvalidArgumentException]);
    }
    
    @try {
        [obj.class new];
        FAIL("able to call new");
    } @catch (NSException * exc) {
        CHECK([exc.name isEqualToString:NSInvalidArgumentException]);
    }
}

TEST_CASE( "string" ) {
    std::string str("abc");
    
    auto obj = box(std::string("abc"));
    static_assert(std::is_same_v<decltype(obj), NSObject<BoxedValue, BoxedComparable, NSCopying> *>);
    CHECK(boxedValue<std::string>(obj) == str);
    CHECK([obj.description isEqualToString:@"abc"]);
    CHECK(obj.hash == std::hash<std::string>()(str));
    
    auto objc = (decltype(obj))[obj copy];
    CHECK(boxedValue<std::string>(objc) == str);
    
    CHECK([objc isEqual:obj]);
    CHECK([objc isEqualTo:obj]);
    
    auto objm = box(std::move(str));
    CHECK(boxedValue<std::string>(objm) == "abc");
    CHECK(str.empty());
    
    auto obje = box<std::string>(size_t(5), 'a');
    CHECK(boxedValue<std::string>(obje) == "aaaaa");
}

TEST_CASE( "unique_ptr" ) {
    
    auto obj = box(std::make_unique<int>(5));
    static_assert(std::is_same_v<decltype(obj), NSObject<BoxedValue, BoxedComparable> *>);
    CHECK(*boxedValue<std::unique_ptr<int>>(obj) == 5);
    
    @try {
        [[maybe_unused]] auto objc = (decltype(obj))[obj copy];
        FAIL("exception not thrown");
    } @catch (NSException * exc) {
        CHECK([exc.name isEqualToString:NSInvalidArgumentException]);
    }
    
    auto ptr = std::make_unique<int>(6);
    auto hash = std::hash<std::unique_ptr<int>>()(ptr);
    void * val = ptr.get();
    auto objm = box(std::move(ptr));
    CHECK(!ptr);
    CHECK(*boxedValue<std::unique_ptr<int>>(objm) == 6);
    CHECK(boxedValue<std::unique_ptr<int>>(objm).get() == val);
    CHECK(objm.hash == hash);
    CHECK([objm.description isEqualToString:[NSString stringWithFormat:@"%p", val]]);
}

TEST_CASE( "struct" ) {
    
    struct foo {
        int i;
        char c;
    };
    
    auto obj1 = box(foo{4, 'b'});
    auto obj2 = box(foo{4, 'b'});
    
    CHECK(boxedValue<foo>(obj1).i == 4);
    CHECK(boxedValue<foo>(obj2).c == 'b');
    CHECK(![obj1 isEqual:obj2]);
    CHECK(obj1.hash != obj2.hash);
}

TEST_CASE( "equal-no-hash" ) {
    
    struct foo {
        int i;
        char c;
        
        bool operator==(const foo & rhs) const = default;
    };
    
    auto obj1 = box(foo{4, 'b'});
    auto obj2 = box(foo{4, 'b'});
    
    static_assert(std::is_same_v<decltype(obj1), NSObject<BoxedValue, NSCopying> *>);
    
    CHECK(boxedValue<foo>(obj1).i == 4);
    CHECK(boxedValue<foo>(obj2).c == 'b');
    CHECK([obj1 isEqual:obj2]);
    @try {
        [[maybe_unused]] auto hash = obj1.hash;
        FAIL("exception not thrown");
    } @catch (NSException * exc) {
        CHECK([exc.name isEqualToString:NSInvalidArgumentException]);
    }
}

struct qwerty {};

TEST_CASE( "no-desc" ) {
    auto obj = box<qwerty>();
    CHECK([obj.description isEqualToString:@"Boxed object of type \"qwerty\""]);
}

struct no_spaceship {
    int i;
    
    bool operator==(const no_spaceship &) const = default;
    bool operator!=(const no_spaceship &) const = default;
    bool operator<(const no_spaceship & rhs) const { return i < rhs.i; }
    bool operator<=(const no_spaceship & rhs) const { return i <= rhs.i; }
    bool operator>(const no_spaceship & rhs) const { return i > rhs.i; }
    bool operator>=(const no_spaceship & rhs) const { return i >= rhs.i; }
};

TEST_CASE( "no-spaceship" ) {
    auto obj1 = box(no_spaceship{3});
    auto obj2 = box(no_spaceship{4});
    CHECK([obj1 compare:obj2] == NSOrderedAscending);
}

struct spaceship_only {
    int i;
    
    std::strong_ordering operator<=>(const spaceship_only &) const = default;
};

TEST_CASE( "no-spaceship" ) {
    auto obj1 = box(spaceship_only{3});
    auto obj2 = box(spaceship_only{4});
    CHECK([obj1 compare:obj2] == NSOrderedAscending);
    
    auto obj3 = box(spaceship_only{3});
    CHECK([obj3 isEqual:obj1]);
}


TEST_SUITE_END();
