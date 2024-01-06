#include <objc-helpers/BoxUtil.h>

#include "doctest.h"


TEST_SUITE_BEGIN( "BoxUtilTests" );

TEST_CASE( "integer" ) {
    auto obj = box(42);
    static_assert(std::is_same_v<decltype(obj), NSObject<BoxedValue, NSCopying> *>);
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
    static_assert(std::is_same_v<decltype(obj), NSObject<BoxedValue, NSCopying> *>);
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
    static_assert(std::is_same_v<decltype(obj), NSObject<BoxedValue> *>);
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



TEST_SUITE_END();
