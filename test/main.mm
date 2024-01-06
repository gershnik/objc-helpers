
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        return doctest::Context(argc, argv).run();
    }
}
