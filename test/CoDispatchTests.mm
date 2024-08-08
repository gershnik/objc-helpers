#include <objc-helpers/CoDispatch.h>

#include "doctest.h"

#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>

#include <string>
#include <string_view>
#include <map>
#include <filesystem>
#include <chrono>

using namespace std::literals;
using namespace std::chrono;

template<class T>
struct Sequence {
    std::string value;
    std::map<T *, int> ids;
    int maxId = 0;
    
    void add(T * ptr, const std::string_view & str) {
        auto & id = ids[ptr];
        if (id == 0)
            id = ++maxId;
        value += std::to_string(id);
        value += ':';
        value += str;
    }
    
    void clear() {
        value.clear();
        ids.clear();
        maxId = 0;
    }
};

struct CopyableAndMovable {
    
    static auto sequence() -> Sequence<CopyableAndMovable> & {
        static Sequence<CopyableAndMovable> ret;
        return ret;
    }
    
    CopyableAndMovable() : value(0) {
        sequence().add(this, "c()");
    }
    CopyableAndMovable(int v) : value(v) {
        sequence().add(this, "c(int)");
    }
    ~CopyableAndMovable() {
        sequence().add(this, "~d()");
    }
    CopyableAndMovable(CopyableAndMovable && src) : value(std::exchange(src.value, 0)) {
        sequence().add(this, "c(&&)");
    }
    CopyableAndMovable(const CopyableAndMovable & src) : value(src.value) {
        sequence().add(this, "c(const&)");
    }
    CopyableAndMovable & operator=(CopyableAndMovable && src) {
        sequence().add(this, "=(&&)");
        if (this != &src)
            value = std::exchange(src.value, 0);
        return *this;
    }
    CopyableAndMovable & operator=(const CopyableAndMovable & src) {
        sequence().add(this, "=(const&)");
        value = src.value;
        return *this;
    }
    
    int value;
};

template<bool Noexcept = false>
struct CopyableOnly {
    
    static auto sequence() -> Sequence<CopyableOnly> & {
        static Sequence<CopyableOnly> ret;
        return ret;
    }
    
    CopyableOnly() noexcept(Noexcept) : value(0) {
        sequence().add(this, "c()");
    }
    CopyableOnly(int v) noexcept(Noexcept) : value(v) {
        sequence().add(this, "c(int)");
    }
    ~CopyableOnly() {
        sequence().add(this, "~d()");
    }
    CopyableOnly(const CopyableOnly & src) noexcept(Noexcept) : value(src.value) {
        sequence().add(this, "c(const&)");
    }
    CopyableOnly & operator=(const CopyableOnly & src) noexcept(Noexcept) {
        sequence().add(this, "=(const&)");
        value = src.value;
        return *this;
    }
    
    int value;
};

template<bool Noexcept = true>
struct MovableOnly {
    
    static auto sequence() -> Sequence<MovableOnly> & {
        static Sequence<MovableOnly> ret;
        return ret;
    }
    
    MovableOnly() noexcept(Noexcept): value(0) {
        sequence().add(this, "c()");
    }
    MovableOnly(int v) noexcept(Noexcept) : value(v) {
        sequence().add(this, "c(int)");
    }
    ~MovableOnly() {
        sequence().add(this, "~d()");
    }
    MovableOnly(MovableOnly && src) noexcept(Noexcept) : value(std::exchange(src.value, 0)) {
        sequence().add(this, "c(&&)");
    }
    MovableOnly & operator=(MovableOnly && src) noexcept(Noexcept) {
        sequence().add(this, "=(&&)");
        if (this != &src)
            value = std::exchange(src.value, 0);
        return *this;
    }
    
    int value;
};

float aFloat = 0.7f;
void * aPointer = &aFloat;
typedef void FuncType();
void aFunc() {}

static_assert(noexcept(Util::ValueCarrier<int, SupportsExceptions::No>().moveOut()));
static_assert(noexcept(Util::ValueCarrier<int, SupportsExceptions::No>().getValueToken()));
static_assert(noexcept(Util::ValueCarrier<FuncType *, SupportsExceptions::No>().moveOut()));
static_assert(noexcept(Util::ValueCarrier<FuncType *, SupportsExceptions::No>().getValueToken()));
static_assert(noexcept(Util::ValueCarrier<void, SupportsExceptions::No>().moveOut()));
static_assert(noexcept(Util::ValueCarrier<MovableOnly<true>, SupportsExceptions::No>().moveOut()));
static_assert(noexcept(Util::ValueCarrier<MovableOnly<true>, SupportsExceptions::No>().getValueToken()));

static_assert(!noexcept(Util::ValueCarrier<MovableOnly<false>, SupportsExceptions::No>().moveOut()));
static_assert(noexcept(Util::ValueCarrier<MovableOnly<false>, SupportsExceptions::No>().getValueToken()));
static_assert(!noexcept(Util::ValueCarrier<MovableOnly<false>, SupportsExceptions::No>::moveOutValue(nullptr)));

static_assert(noexcept(Util::ValueCarrier<CopyableOnly<true>, SupportsExceptions::No>().moveOut()));
static_assert(noexcept(Util::ValueCarrier<CopyableOnly<true>, SupportsExceptions::No>().getValueToken()));
static_assert(noexcept(Util::ValueCarrier<CopyableOnly<true>, SupportsExceptions::No>::moveOutValue(nullptr)));

static_assert(!noexcept(Util::ValueCarrier<CopyableOnly<false>, SupportsExceptions::No>().moveOut()));
static_assert(noexcept(Util::ValueCarrier<CopyableOnly<false>, SupportsExceptions::No>().getValueToken()));
static_assert(!noexcept(Util::ValueCarrier<CopyableOnly<false>, SupportsExceptions::No>::moveOutValue(nullptr)));

template<class T>
decltype(auto) delay(NSTimeInterval interval, T && val) {
    [NSThread sleepForTimeInterval:interval];
    return std::forward<T>(val);
}

static auto checkReturnPropagation() -> DispatchTask<> {
    
//    apparently decltype doesn't work on co_await!
//    I have no idea how to check for void result then
//    {
//        static_assert(std::is_same_v<decltype(co_await co_dispatch([&]() -> void {
//        })), void>);
//    }
    
    {
        decltype(auto) res = co_await co_dispatch([&]() -> float & {
            return aFloat;
        });
        static_assert(std::is_same_v<decltype(res), float &>);
        CHECK(&res == &aFloat);
    }
    
    {
        decltype(auto) res = co_await co_dispatch([&]() -> float && {
            return std::move(aFloat);
        });
        static_assert(std::is_same_v<decltype(res), float &&>);
        CHECK(&res == &aFloat);
    }
    
    {
        decltype(auto) res = co_await co_dispatch([&]() -> float  {
            return aFloat;
        });
        static_assert(std::is_same_v<decltype(res), float>);
        CHECK(res == aFloat);
    }
    
    {
        decltype(auto) res = co_await co_dispatch([&]() -> void * {
            return aPointer;
        });
        static_assert(std::is_same_v<decltype(res), void *>);
        CHECK(res == aPointer);
    }
    
    {
        decltype(auto) res = co_await co_dispatch([&]() -> FuncType * {
            return aFunc;
        });
        static_assert(std::is_same_v<decltype(res), FuncType *>);
        CHECK(res == aFunc);
    }
    
    {
        decltype(auto) res = co_await co_dispatch([&]() -> FuncType & {
            return aFunc;
        });
        static_assert(std::is_same_v<decltype(res), FuncType &>);
        CHECK(res == aFunc);
    }
    
    CopyableAndMovable::sequence().clear();
    {
        decltype(auto) res = co_await co_dispatch([&]() -> CopyableAndMovable {
            return CopyableAndMovable{8};
        });
        static_assert(std::is_same_v<decltype(res), CopyableAndMovable>);
        CHECK(res.value == 8);
    }
    CHECK(CopyableAndMovable::sequence().value == "1:c(int)2:c(&&)1:~d()3:c(&&)2:~d()3:~d()");
    
    CopyableAndMovable::sequence().clear();
    {
        decltype(auto) res = co_await co_dispatch([&]() -> const CopyableAndMovable {
            return CopyableAndMovable{8};
        });
        static_assert(std::is_same_v<decltype(res), const CopyableAndMovable>);
        CHECK(res.value == 8);
    }
    CHECK(CopyableAndMovable::sequence().value == "1:c(int)2:c(const&)1:~d()3:c(&&)2:~d()3:~d()");
    
    CopyableOnly<false>::sequence().clear();
    {
        decltype(auto) res = co_await co_dispatch([&]() -> CopyableOnly<false> {
            return CopyableOnly<false>{8};
        });
        static_assert(std::is_same_v<decltype(res), CopyableOnly<false>>);
        CHECK(res.value == 8);
    }
    CHECK(CopyableOnly<false>::sequence().value == "1:c(int)2:c(const&)1:~d()3:c(const&)2:~d()3:~d()");
    
    MovableOnly<false>::sequence().clear();
    {
        decltype(auto) res = co_await co_dispatch([&]() -> MovableOnly<false> {
            return MovableOnly<false>{8};
        });
        static_assert(std::is_same_v<decltype(res), MovableOnly<false>>);
        CHECK(res.value == 8);
    }
    CHECK(MovableOnly<false>::sequence().value == "1:c(int)2:c(&&)1:~d()3:c(&&)2:~d()3:~d()");
    
    try {
        co_await co_dispatch([&]() {
            throw std::runtime_error("haha");
        });
        FAIL("exception not thrown");
    } catch (std::runtime_error & ex) {
        CHECK(strcmp(ex.what(), "haha") == 0);
    }
    
    try {
        co_await co_dispatch([&]() -> int {
            throw std::runtime_error("haha");
        });
        FAIL("exception not thrown");
    } catch (std::runtime_error & ex) {
        CHECK(strcmp(ex.what(), "haha") == 0);
    }
    
    @try {
        co_await co_dispatch([&]() -> int {
            [NSException raise:@"hello" format:@"%d", 15];
            return 7;
        });
        FAIL("exception not thrown");
    } @catch (NSException * ex) {
        CHECK([ex.name isEqualToString:@"hello"]);
        CHECK([ex.reason isEqualToString:@"15"]);
    }
    
    {
        int i = co_await co_dispatch(^ {
            return 47;
        });
        CHECK(i == 47);
    }
    
    {
        auto pid = co_await co_dispatch(getpid);
        CHECK(pid == getpid());
    }
    
    {
        struct MoveOnlyFunc {
            MoveOnlyFunc() = default;
            MoveOnlyFunc(MoveOnlyFunc &&) = default;
            MoveOnlyFunc(const MoveOnlyFunc &) = delete;
            int operator()() const { return 65; }
        };
        auto res = co_await co_dispatch(MoveOnlyFunc());
        CHECK(res == 65);
    }
    
    {
        struct CopyOnlyFunc {
            CopyOnlyFunc() = default;
            CopyOnlyFunc(CopyOnlyFunc &&) = delete;
            CopyOnlyFunc(const CopyOnlyFunc &) = default;
            int operator()() const { return 65; }
        };
        auto res = co_await co_dispatch(CopyOnlyFunc());
        CHECK(res == 65);
        
        CopyOnlyFunc x;
        res = co_await co_dispatch(x);
        CHECK(res == 65);
    }
}

static auto checkDispatchToDifferentQueue() -> DispatchTask<> {
    
    auto conq = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
    auto i = co_await co_dispatch(conq, []() {
        [NSThread sleepForTimeInterval:0.2];
        return 1;
    });
    CHECK(i == 1);
    CHECK(!NSThread.isMainThread);
    
    i = co_await co_dispatch(conq, []() {
        [NSThread sleepForTimeInterval:0.2];
        return 2;
    }).resumeOnMainQueue();
    CHECK(i == 2);
    CHECK(NSThread.isMainThread);
    
    co_await resumeOn(conq);
    CHECK(!NSThread.isMainThread);
    
    co_await resumeOnMainQueue();
    CHECK(NSThread.isMainThread);
    
    co_await resumeOn(conq);
    CHECK(!NSThread.isMainThread);
    
    co_await resumeOnMainQueue(dispatch_time(DISPATCH_TIME_NOW, nanoseconds(200ms).count()));
    CHECK(NSThread.isMainThread);
    
    co_await resumeOn(conq, dispatch_time(DISPATCH_TIME_NOW, nanoseconds(200ms).count()));
    CHECK(!NSThread.isMainThread);
    
    i = co_await delay(0.2, co_dispatch(dispatch_get_main_queue(), []() {
        return 3;
    }));
    CHECK(i == 3);
    CHECK(!NSThread.isMainThread);
    
    i = co_await delay(0.2, co_dispatch(conq, []() {
        return 4;
    }).resumeOn(dispatch_get_main_queue()));
    CHECK(i == 4);
    CHECK(NSThread.isMainThread);
}

static auto checkMakeAwaitable() -> DispatchTask<> {
    
    int i = co_await makeAwaitable<int>([](auto promise) {
        dispatch_async(dispatch_get_main_queue(), ^ () {
            promise.success(7);
        });
    });
    CHECK(i == 7);
    
    try {
        co_await makeAwaitable<int>([](auto) {
            throw std::runtime_error("hehe");
        });
        FAIL("exception not thrown");
    } catch (std::runtime_error & ex) {
        CHECK(strcmp(ex.what(), "hehe") == 0);
    }
    
    try {
        co_await makeAwaitable<int>([](auto promise) {
            dispatch_async(dispatch_get_main_queue(), ^ () {
                promise.failure(std::runtime_error("hoho"));
            });
        });
        FAIL("exception not thrown");
    } catch (std::runtime_error & ex) {
        CHECK(strcmp(ex.what(), "hoho") == 0);
    }
    
    try {
        co_await makeAwaitable<int>([](auto promise) {
            dispatch_async(dispatch_get_main_queue(), ^ () {
                auto eptr = std::make_exception_ptr(std::runtime_error("hoho"));
                promise.failure(eptr);
            });
        });
        FAIL("exception not thrown");
    } catch (std::runtime_error & ex) {
        CHECK(strcmp(ex.what(), "hoho") == 0);
    }
    
    {
        struct Thrower {
            Thrower(int) {
                throw std::logic_error("bam");
            }
        };
        
        try {
            co_await makeAwaitable<Thrower>([](auto promise) {
                dispatch_async(dispatch_get_main_queue(), ^ () {
                    try {
                        promise.success(6);
                    } catch (...) {
                        promise.failure(std::current_exception());
                    }
                });
            });
            FAIL("exception not thrown");
        } catch (std::logic_error & ex) {
            CHECK(strcmp(ex.what(), "bam") == 0);
        }
    }
    
    {
        
        struct Foo {
            int i;
            float f;
        };
        
        decltype(auto) res = co_await makeAwaitable<Foo>([](auto promise) {
            dispatch_async(dispatch_get_main_queue(), ^ () {
                promise.success({1, .4f});
            });
        });
        static_assert(std::is_same_v<decltype(res), Foo>);
        CHECK(res.i == 1);
        CHECK(res.f == .4f);
    }
    
    {
        decltype(auto) res = co_await makeAwaitable<std::vector<char>>([](auto promise) {
            dispatch_async(dispatch_get_main_queue(), ^ () {
                promise.success(3, 'a');
            });
        });
        static_assert(std::is_same_v<decltype(res), std::vector<char>>);
        CHECK(res == std::vector<char>{'a', 'a', 'a'});
    }
    
    {
        co_await makeAwaitable<void>([](auto promise) {
            dispatch_async(dispatch_get_main_queue(), ^ () {
                promise.success();
            });
        });
    }
    
    i = co_await makeAwaitable<int, SupportsExceptions::No>([](auto promise) {
        dispatch_async(dispatch_get_main_queue(), ^ () {
            promise.success(7);
        });
    });
    CHECK(i == 7);
}

static auto checkTasks() -> DispatchTask<> {
    
    auto exceptor = []() -> DispatchTask<int> {
        auto conq = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
        
        co_await resumeOn(conq);
        throw std::runtime_error("whopwhop");
    };
    
    try {
        co_await exceptor();
        FAIL("exception not thrown");
    } catch (std::runtime_error & ex) {
        CHECK(strcmp(ex.what(), "whopwhop") == 0);
    }
    
    co_await resumeOnMainQueue();
    CHECK(NSThread.isMainThread);
    
    
    auto coro = []() -> DispatchTask<int> {
        auto conq = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
        
        co_await resumeOn(conq);
        co_return 25;
    };
    
    co_await coro().resumeOnMainQueue();
    CHECK(NSThread.isMainThread);
    
    co_await coro().resumeOnMainQueue(dispatch_time(DISPATCH_TIME_NOW, nanoseconds(200ms).count()));
    CHECK(NSThread.isMainThread);
    
    co_await delay(0.2, coro().resumeOnMainQueue(dispatch_time(DISPATCH_TIME_NOW, nanoseconds(1ms).count())));
    CHECK(NSThread.isMainThread);
}

static auto checkGenerator() -> DispatchTask<> {
    
    auto conq = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
    
    {
        auto generate = []() -> DispatchGenerator<int> {
            co_yield 1;
            co_yield 2;
            co_yield 3;
        };
        
        
        std::vector<int> res;
        for (auto it = co_await generate().resumingOnMainQueue().beginOn(conq); it; co_await it.next()) {
            CHECK(NSThread.isMainThread);
            res.push_back(*it);
        }
        CHECK(res == std::vector{1, 2, 3});
    }
    
    {
        auto generate = []() -> DispatchGenerator<int> {
            co_return;
        };
        std::vector<int> res;
        for (auto it = co_await generate().begin(); it; co_await it.next()) {
            CHECK(NSThread.isMainThread);
            res.push_back(*it);
        }
        CHECK(res.empty());
    }
    
    {
        auto generate = []() -> DispatchGenerator<int> {
            throw std::runtime_error("oops");
        };
        std::vector<int> res;
        try {
            for (auto it = co_await generate().begin(); it; co_await it.next()) {
                res.push_back(*it);
            }
            FAIL("exception not thrown");
        } catch(std::runtime_error & ex) {
            CHECK(strcmp(ex.what(), "oops") == 0);
        }
        CHECK(res.empty());
    }
    
    {
        auto generate = []() -> DispatchGenerator<int> {
            co_yield 1;
            throw std::runtime_error("oops");
        };
        std::vector<int> res;
        auto it = co_await generate().begin();
        try {
            for (; it; co_await it.next()) {
                res.push_back(*it);
            }
            FAIL("exception not thrown");
        } catch(std::runtime_error & ex) {
            CHECK(strcmp(ex.what(), "oops") == 0);
            CHECK(!it);
        }
        CHECK(res == std::vector{1});
    }
    
    {
        auto generate = []() -> DispatchGenerator<int> {
            co_yield 1;
            co_yield 2;
            co_yield 3;
        };
        
        std::vector<int> res;
        for (auto it = co_await generate().beginSync(); it; co_await it.next()) {
            res.push_back(*it);
        }
        CHECK(res == std::vector{1, 2, 3});
    }
    
    {
        auto generate = []() -> DispatchGenerator<int> {
            co_yield 1;
            co_yield 2;
            co_yield 3;
        };
        
        {
            generate();
        }
        {
            auto it = co_await generate().begin();
        }
        {
            auto it = co_await generate().begin();
            it.next();
        }
    }
    
}

static auto checkIO() -> DispatchTask<> {
    
    auto conq = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
    
    co_await resumeOn(conq);
    
    char buf[1024];
    uint32_t size = sizeof(buf);
    REQUIRE(_NSGetExecutablePath(buf, &size) == 0);
    std::filesystem::path path(buf);
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
        
        dispatch_io_t rch = dispatch_io_create(DISPATCH_IO_STREAM, rfd, conq, ^(int /*error*/) {
            close(rfd);
        });
        
        res = co_await co_dispatch_io_read(rch, 5, 6, conq);
        REQUIRE(!res.error());
        REQUIRE((res.data() != nullptr));
        
        combined = dispatch_data_create_concat(combined, res.data());
        __block int count = 0;
        res = co_await co_dispatch_io_read(rch, 11, 5, conq, ^ (bool, dispatch_data_t, int) {
            ++count;
        });
        REQUIRE(!res.error());
        REQUIRE((res.data() != nullptr));
        CHECK(count > 0);
        
        combined = dispatch_data_create_concat(combined, res.data());
        
        //CHECK(dispatch_data_get_size(combined) == 16);
        const void * realData;
        size_t realSize;
        [[maybe_unused]] dispatch_data_t dumb = dispatch_data_create_map(combined, &realData, &realSize);
        CHECK(realSize == 16);
        CHECK(memcmp(realData, "hello world yada", 16) == 0);
    }
    
    remove(path);
    
    co_await resumeOnMainQueue();
}

TEST_CASE("CoDispatchTests") {
    []() -> DispatchTask<> {
        co_await checkReturnPropagation();
        co_await checkDispatchToDifferentQueue();
        co_await checkMakeAwaitable();
        co_await checkTasks();
        co_await checkGenerator();
        co_await checkIO();
    }();
}

