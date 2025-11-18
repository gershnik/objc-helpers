#include <objc-helpers/BlockUtil.h>

#include <Foundation/Foundation.h>

#include "doctest.h"

TEST_SUITE_BEGIN( "BlockUtilTests" );

namespace {
    struct fooBase {
        static inline std::string record;
        
        fooBase()
            { record += 'd'; }
        
        fooBase(const fooBase &)
            { record += 'c'; }
        
        fooBase(fooBase &&)
            { record += 'm'; }
        
        ~fooBase()
            { record += '~'; }
        
        int operator()(int i) const {
            record += 'o';
            return i;
        }
    };
    
    using foo = fooBase;
    
    struct fooCopyOnly : fooBase {
        fooCopyOnly() = default;
        fooCopyOnly(const fooCopyOnly &) = default;
        fooCopyOnly(fooCopyOnly &&) = delete;
    };
    
    struct fooMoveOnly : fooBase {
        fooMoveOnly() = default;
        fooMoveOnly(fooMoveOnly &&) = default;
    };

}

static int testBlock(int (^block)()) { return block(); }
static int testBlock(int (^block)(int), int val) { return block(val); }

static int (^g_block)(int);

TEST_CASE( "makeBlock" ) {
    
    @autoreleasepool 
    {
        foo::record.clear();
        g_block = copy(makeBlock(foo{}));
        g_block(5);
        g_block = nil;
    }
    CHECK(foo::record == "dmm~~o~");
    
    {
        foo::record.clear();
        CHECK(testBlock(makeBlock(foo{}), 5) == 5);
    }
    CHECK(foo::record == "dmo~~");
    
    {
        foo::record.clear();
        foo callable;
        CHECK(testBlock(makeBlock(callable), 5) == 5);
    }
    CHECK(foo::record == "dco~~");
    
    {
        foo::record.clear();
        const foo callable;
        CHECK(testBlock(makeBlock(callable), 5) == 5);
    }
    CHECK(foo::record == "dco~~");
    
    {
        std::string str("abcdefghijklmnopqrstuvwxyz");
        std::string (^strblock)() = makeBlock([=](){
            return str;
        });
        CHECK(strblock() == str);
    }
    
    {
        int n = 5;
        CHECK(testBlock(makeMutableBlock([=] () mutable {
            n = 3;
            return 3;
        })) == 3);
    }
    
    {
        foo::record.clear();
        int (^block)(int) = makeBlock(foo{});
        
        CHECK(block(6) == 6);
    }
    CHECK(foo::record == "dmm~~o~");
    
    {
        foo::record.clear();
        int (^block1)(int) = makeBlock(foo{});
        int (^block2)(int) = block1;
        
        CHECK(block2(6) == 6);
    }
    CHECK(foo::record == "dmm~~o~");
    
    {
        foo::record.clear();
        int (^block)(int) = makeBlock(fooCopyOnly{});
        
        CHECK(block(6) == 6);
    }
    CHECK(foo::record == "dcc~~o~");
    
    {
        foo::record.clear();
        int (^block)(int) = makeBlock(fooMoveOnly{});
        
        CHECK(block(6) == 6);
    }
    CHECK(foo::record == "dmm~~o~");
    
    {
        foo::record.clear();
        auto b = makeBlock(foo{});
        decltype(b) b1 = std::move(b);
        CHECK(b1(42) == 42);
    }
    CHECK(foo::record == "dm~mo~~");
    
    {
        foo::record.clear();
        auto b = makeBlock(fooMoveOnly{});
        decltype(b) b1 = std::move(b);
        CHECK(b1(42) == 42);
    }
    CHECK(foo::record == "dm~mo~~");
    
    {
        foo::record.clear();
        auto b = makeBlock(fooCopyOnly{});
        decltype(b) b1 = std::move(b);
        CHECK(b1(42) == 42);
    }
    CHECK(foo::record == "dc~co~~");
}

@interface MakeStrongCheck : NSObject {
@public int i;
}
@end

@implementation MakeStrongCheck
@end

TEST_CASE( "makeStrong" ) {
    
    auto str = [MakeStrongCheck new];
    auto weak = makeWeak(str);
    REQUIRE(bool(weak));
    auto str1 = makeStrong(weak);
    bool equal = str == str1;
    CHECK(equal);
}

TEST_SUITE_END();
