#include "TestGlobal.h"

#include <mutex>
#include <condition_variable>

#include <dispatch/dispatch.h>


static int g_AsyncCount = 0;
static std::mutex g_AsyncCountMutex;
static std::condition_variable g_AsyncCountCond;
static int g_IsMainKey;

static void startAsync() {
    std::lock_guard lk(g_AsyncCountMutex);
    ++g_AsyncCount;
}

static void endAsync() {
    std::lock_guard lk(g_AsyncCountMutex);
    if (--g_AsyncCount == 0)
        g_AsyncCountCond.notify_one();
        
}

static void waitForNoAsync() {
    std::unique_lock lk(g_AsyncCountMutex);
    g_AsyncCountCond.wait(lk, []{ return g_AsyncCount == 0; });
}

void waitForAsyncTest(void (^block)()) {
    startAsync();
    dispatch_async(dispatch_get_main_queue(), block);
    waitForNoAsync();
}

void finishAsyncTest() {
    endAsync();
}

bool isMainQueue() {
    return (intptr_t)dispatch_get_specific(&g_IsMainKey) == 1;
}

void runMainQueue() {
    dispatch_queue_set_specific(dispatch_get_main_queue(), &g_IsMainKey, (void*)intptr_t(1), nullptr);
    dispatch_main();
}

