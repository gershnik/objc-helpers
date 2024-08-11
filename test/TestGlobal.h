#ifndef HEADER_TEST_GLOBAL_H_INCLUDED
#define HEADER_TEST_GLOBAL_H_INCLUDED

void waitForAsyncTest(void (^block)());
void finishAsyncTest();

bool isMainQueue();
void runMainQueue();

#endif
