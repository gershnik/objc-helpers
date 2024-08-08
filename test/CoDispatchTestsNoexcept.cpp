#include <objc-helpers/CoDispatch.h>

#include "doctest.h"



TEST_CASE("CoDispatchTestsNoExcept") {
    
    []() -> DispatchTask<> {
        auto i = co_await co_dispatch([]() {
            return 7;
        });
        CHECK(i == 7);
    }();
}
