#include <objc-helpers/NSNumberUtil.h>

#include "doctest.h"

TEST_CASE("comparison") {
    
    CHECK(NSNumberEqual()(@(1), @(1)));
    CHECK(!NSNumberEqual()(@(1), @(2)));
    CHECK(!NSNumberEqual()(nullptr, @(0)));
    CHECK(!NSNumberEqual()(@(0), nullptr));
    CHECK(NSNumberEqual()(nullptr, nullptr));
    
    CHECK(NSNumberLess()(@(3), @(5.5)));
    CHECK(!NSNumberLess()(@(3), @(3)));
    CHECK(NSNumberLess()(nullptr, @(3)));
    CHECK(NSNumberLess()(nullptr, @(0)));
    CHECK(!NSNumberLess()(@(0), nullptr));
    CHECK(!NSNumberLess()(nullptr, nullptr));
}
