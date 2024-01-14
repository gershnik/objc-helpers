#include <objc-helpers/BlockUtil.h>

#include "doctest.h"

TEST_SUITE_BEGIN( "BlockUtilTestsCpp" );

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

TEST_CASE( "makeBlock" ) {
    
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
        std::string (^strblock)() = copy(makeBlock([](){
            return std::string("abcdefghijklmnopqrstuvwxyz");
        }));
        CHECK(strblock() == "abcdefghijklmnopqrstuvwxyz");
        Block_release(strblock);
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
        int (^block)(int) = copy(makeBlock(foo{}));
        
        CHECK(block(6) == 6);
        
        Block_release(block);
    }
    CHECK(foo::record == "dmm~~o~");
    
    {
        foo::record.clear();
        int (^block1)(int) = copy(makeBlock(foo{}));
        int (^block2)(int) = Block_copy(block1);
        
        CHECK(block2(6) == 6);
        
        Block_release(block1);
        Block_release(block2);
    }
    CHECK(foo::record == "dmm~~o~");
    
    {
        foo::record.clear();
        int (^block)(int) = copy(makeBlock(fooCopyOnly{}));
        
        CHECK(block(6) == 6);
        Block_release(block);
    }
    CHECK(foo::record == "dcc~~o~");
    
    {
        foo::record.clear();
        int (^block)(int) = copy(makeBlock(fooMoveOnly{}));
        
        CHECK(block(6) == 6);
        Block_release(block);
    }
    CHECK(foo::record == "dmm~~o~");
    
    {
        foo::record.clear();
        auto b = makeBlock(fooCopyOnly{});
        decltype(b) b1 = std::move(b);
    }
    CHECK(foo::record == "dc~c~~");
}

TEST_SUITE_END();
