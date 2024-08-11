
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <dispatch/dispatch.h>

#include "TestGlobal.h"

static dispatch_queue_t g_TestQueue;

int main(int argc, const char * argv[]) {
    //We run tests on a separate serial queue because we can then block it waiting for async operations
    //launched from a test to finish. We cannot do this from main queue since some async tests must themselves
    //run on main queue. There is no way to portably have "modal loop" with libdispatch. On Mac only
    //we could run a runloop but not on Linux.
#ifdef __OBJC__
    @autoreleasepool {
#endif
        g_TestQueue = dispatch_queue_create("tests", DISPATCH_QUEUE_SERIAL);
        dispatch_async(g_TestQueue, ^ {
#ifdef __OBJC__
            @autoreleasepool {
#endif
                auto ret = doctest::Context(argc, argv).run();
                exit(ret);
#ifdef __OBJC__
            }
#endif
        });
        runMainQueue();

#ifdef __OBJC__
    }
#endif
}
