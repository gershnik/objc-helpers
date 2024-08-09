#include <objc-helpers/CoDispatch.h>

#include "doctest.h"

#include "TestGlobal.h"


TEST_CASE("CoDispatchTestsNoExcept") {
    
    startAsync();
    dispatch_async(dispatch_get_main_queue(), ^ {
        []() -> DispatchTask<> {
            auto i = co_await co_dispatch([]() {
                return 7;
            });
            CHECK(i == 7);
            endAsync();
        }();
    });
    waitForNoAsync();
}
