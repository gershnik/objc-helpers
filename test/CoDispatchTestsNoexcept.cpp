#include <objc-helpers/CoDispatch.h>

#include "doctest.h"

#include "TestGlobal.h"

static DispatchTask<> runTests() {
    auto i = co_await co_dispatch([]() {
        return 7;
    });
    CHECK(i == 7);
    finishAsyncTest();
}


TEST_CASE("CoDispatchTestsNoExcept") {
    waitForAsyncTest(^ {
        runTests();
    });
}
