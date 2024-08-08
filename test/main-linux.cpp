
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <dispatch/dispatch.h>

int main(int argc, const char * argv[]) {
    dispatch_async(dispatch_get_main_queue(), ^ {
        auto ret = doctest::Context(argc, argv).run();
        exit(ret);
    });
    dispatch_main();
}
