#include <objc-helpers/CoDispatch.h>

//#define DOCTEST_CONFIG_NO_EXCEPTIONS
#include "doctest.h"

#include <CoreFoundation/CoreFoundation.h>


static DispatchTask<> runTests(bool & shouldKeepRunning) {
    
    auto i = co_await co_dispatch([]() {
        return 7;
    });
    CHECK(i == 7);
    
    shouldKeepRunning = false;
}


TEST_CASE("CoDispatchTestsNoExcept") {
    
    bool shouldKeepRunning = true;
    
    runTests(shouldKeepRunning);
    
    while (shouldKeepRunning) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, std::numeric_limits<CFTimeInterval>::max(), true);
    }
    
}
