#include <objc-helpers/BlockUtil.h>

#include "doctest.h"


TEST_SUITE_BEGIN( "BlockUtilTests" );

template<class Ret, class... Args>
Ret testInvoke(Ret (^block)(Args...), Args... args) {
    auto ret = block(std::forward<Args>(args)...);
    Block_release(block);
    return ret;
}

TEST_CASE( "simple" ) {
    
    {
        auto res = testInvoke(makeBlock([]() {
            return 7;
        }));
        CHECK(res == 7);
    }
    
    {
        auto res = testInvoke(makeBlock([](int i) {
            return i;
        }), 42);
        CHECK(res == 42);
    }
}

TEST_SUITE_END();
