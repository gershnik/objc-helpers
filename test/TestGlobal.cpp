#include "TestGlobal.h"

#include <mutex>
#include <condition_variable>

#include <dispatch/dispatch.h>


static int g_AsyncCount = 0;
static std::mutex g_AsyncCountMutex;
static std::condition_variable g_AsyncCountCond;
dispatch_queue_t g_TestQueue;
int g_IsMainKey;

void startAsync() {
    std::lock_guard lk(g_AsyncCountMutex);
    ++g_AsyncCount;
}

void endAsync() {
    std::lock_guard lk(g_AsyncCountMutex);
    if (--g_AsyncCount == 0)
        g_AsyncCountCond.notify_one();
        
}

void waitForNoAsync() {
    std::unique_lock lk(g_AsyncCountMutex);
    g_AsyncCountCond.wait(lk, []{ return g_AsyncCount == 0; });
}

bool isMainQueue() {
    return (intptr_t)dispatch_get_specific(&g_IsMainKey) == 1;
}

