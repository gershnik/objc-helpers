#include <version>
#if __cpp_lib_coroutine

#include <objc-helpers/CoDispatch.h>

#include "doctest.h"

#if (defined(__APPLE__) && defined(__MACH__))
#include <mach-o/dyld.h>
#endif

#include <filesystem>
#include <vector>

#include "TestGlobal.h"

static auto checkIO() -> DispatchTask<> {
    
    auto conq = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0);
    
    co_await resumeOn(conq);
    
#if (defined(__APPLE__) && defined(__MACH__))
    char buf[1024];
    uint32_t size = sizeof(buf);
    REQUIRE(_NSGetExecutablePath(buf, &size) == 0);
    std::filesystem::path path(buf);
#else
    std::filesystem::path selfLink = "/proc/" + std::to_string(getpid()) + "/exe";
    std::error_code ec;
    std::filesystem::path path = read_symlink(selfLink, ec);
    REQUIRE(!ec);
#endif
    
    path = path.parent_path() / "test.txt";
    
    dispatch_data_t hello = dispatch_data_create("hello", 5, conq,  DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    dispatch_data_t world = dispatch_data_create(" world", 6, conq,  DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    dispatch_data_t yada = dispatch_data_create(" yada", 5, conq,  DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    
    {
        int wfd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);
        REQUIRE(wfd);
        
        auto res = co_await co_dispatch_write(wfd, hello, conq);
        REQUIRE(!res.error());
        REQUIRE(!res.data());
        dispatch_io_t wch = dispatch_io_create(DISPATCH_IO_STREAM, wfd, conq, ^(int /*error*/) {
            close(wfd);
        });
        res = co_await co_dispatch_io_write(wch, 5, world, conq);
        REQUIRE(!res.error());
        REQUIRE(!res.data());
        __block int count = 0;
        res = co_await co_dispatch_io_write(wch, 11, yada, conq, ^ (bool , dispatch_data_t, int) {
            ++count;
        });
        REQUIRE(!res.error());
        REQUIRE(!res.data());
        CHECK(count > 0);
    }
    
    {
        int rfd = open(path.c_str(), O_RDONLY | O_CLOEXEC, S_IRUSR | S_IWUSR);
        REQUIRE(rfd);
        
        auto res = co_await co_dispatch_read(rfd, 5, conq);
        REQUIRE(!res.error());
        REQUIRE((res.data() != nullptr));
        auto combined = res.data();
        dispatch_retain(combined);
        
        dispatch_io_t rch = dispatch_io_create(DISPATCH_IO_STREAM, rfd, conq, ^(int /*error*/) {
            close(rfd);
        });
        
        res = co_await co_dispatch_io_read(rch, 5, 6, conq);
        REQUIRE(!res.error());
        REQUIRE((res.data() != nullptr));
        
        auto temp = dispatch_data_create_concat(combined, res.data());
        dispatch_release(combined);
        combined = temp;
        __block int count = 0;
        res = co_await co_dispatch_io_read(rch, 11, 5, conq, ^ (bool, dispatch_data_t, int) {
            ++count;
        });
        REQUIRE(!res.error());
        REQUIRE((res.data() != nullptr));
        CHECK(count > 0);
        
        temp = dispatch_data_create_concat(combined, res.data());
        dispatch_release(combined);
        combined = temp;
        
        //CHECK(dispatch_data_get_size(combined) == 16);
        const void * realData;
        size_t realSize;
        [[maybe_unused]] dispatch_data_t dumb = dispatch_data_create_map(combined, &realData, &realSize);
        CHECK(realSize == 16);
        CHECK(memcmp(realData, "hello world yada", 16) == 0);
        dispatch_release(combined);

        DispatchIOResult res1 = res;
        CHECK((res1.data() == res.data()));
        CHECK(res1.error() == res.error());
        res = res1;
        CHECK((res1.data() == res.data()));
        CHECK(res1.error() == res.error());
    }
    
    remove(path);
    
    co_await resumeOnMainQueue();
}

static DispatchTask<> runTests() {
    
    auto conq = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0);
    
    int i = co_await co_dispatch([&]() {
        return 7;
    });
    CHECK(i == 7);
    
    i = co_await co_dispatch(conq, []() {
        return 2;
    }).resumeOnMainQueue();
    CHECK(i == 2);
    
    i = co_await co_dispatch(conq, []() {
        return 2;
    }).resumeOn(conq).resumeOnMainQueue();
    CHECK(i == 2);
    
    i = co_await co_dispatch(^ {
        return 47;
    });
    CHECK(i == 47);
    
    {
        auto generate = []() -> DispatchGenerator<int> {
            co_yield 1;
            co_yield 2;
            co_yield 3;
        };
        
        
        std::vector<int> res;
        for (auto it = co_await generate().resumingOnMainQueue().beginOn(conq); it; co_await it.next()) {
            res.push_back(*it);
        }
        CHECK(res == std::vector{1, 2, 3});
    }
    
    co_await checkIO();
    finishAsyncTest();
}

TEST_CASE("CoDispatchTestsCpp") {
    waitForAsyncTest(^ {
        runTests();
    });
}

#endif

