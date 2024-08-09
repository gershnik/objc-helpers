
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <dispatch/dispatch.h>

extern dispatch_queue_t g_TestQueue;
extern int g_IsMainKey;

int main(int argc, const char * argv[]) {
    g_TestQueue = dispatch_queue_create("tests", DISPATCH_QUEUE_SERIAL);
    dispatch_async(g_TestQueue, ^ {
        auto ret = doctest::Context(argc, argv).run();
        exit(ret);
    });
    dispatch_queue_set_specific(dispatch_get_main_queue(), &g_IsMainKey, (void*)1, nullptr);
    dispatch_main();
}
