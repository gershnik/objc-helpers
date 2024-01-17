#import <Foundation/Foundation.h>

#include "../include/objc-helpers/BlockUtil.h"
#include "../include/objc-helpers/BoxUtil.h"
#include "../include/objc-helpers/CoDispatch.h"
#include "../include/objc-helpers/NSStringUtil.h"
#include "../include/objc-helpers/NSObjectUtil.h"
#include "../include/objc-helpers/NSNumberUtil.h"

#include <iostream>
#include <unordered_map>
#include <map>

//MARK: - Blocks to Lambdas Demo

@interface BlockDemo : NSObject

@end

@implementation BlockDemo

- (void) callLambda {
    dispatch_sync(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), [weakSelf = makeWeak(self)]() {
        
        auto self = makeStrong(weakSelf);
        
        [self aMethod];
    });
}

- (void) aMethod {
    std::cout << "aMethod called\n";
}

@end

//MARK: - GCD Coroutines Demo

DispatchTask<int> CoroutineDemo() {
    
    //this runs on the main queue
    int a = co_await co_dispatch([]() {
        return 25;
    });
    
    auto conq = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
    
    //this runs on the conq queue
    int b = co_await co_dispatch(conq, []() {
        return 25;
    });
    
    //here you might be either on main queue (if the async method executed very fast)
    //or on conq, if not
    
    //we can manually await a switch to a different queue
    co_await resumeOnMainQueue(); //same as resumeOn(dispatch_get_main_queue())
    
    
    //but doing so is wasteful. A simpler way is
    b = co_await co_dispatch(conq, []() {
        return 25;
    }).resumeOnMainQueue(); //same as resumeOn(dispatch_get_main_queue())
    
    //we can convert a call with asynchronous callback to an awaitable
    
    try {
        auto str = co_await makeAwaitable<NSString *>([](auto promise) {
            NSError * err;
            [NSTask launchedTaskWithExecutableURL:[NSURL fileURLWithPath:@"/bin/bash"]
                                        arguments:@[@"-c", @"ls"]
                                            error:&err
                               terminationHandler:^(NSTask * res){
                
                if (res.terminationStatus == 0)
                    promise.success(@"call succeeded");
                else
                    promise.failure(std::runtime_error("fail"));
            }];
            if (err)
                throw std::runtime_error(err.description.UTF8String);
        });
        
        std::cout << str << '\n';
    } catch (std::exception & ex) {
        //failure passed to promise will be caught here
    }
    
    co_return a + b;
}

DispatchTask<> CoroutineAwaitingCoroutineDemo() {
    
    // DispatchTasks are themselves awaitable
    
    int res = co_await CoroutineDemo();
    
    //and they also can be told to resume on a specified queue
    
    res = co_await CoroutineDemo().resumeOn(dispatch_get_main_queue()); // or .resumeOnMainQueue()
}

DispatchGenerator<NSString *> AsyncGenerator() {
    co_yield @"Hello";
    co_yield co_await co_dispatch([] () {
        return @" World!";
    });
}

DispatchTask<> UsingGeneratorDemo() {
    
    //running generator asynchronously on the main queue
    auto dest = [NSMutableString new];
    for (auto it = co_await AsyncGenerator().begin(); it; co_await it.next()) {
        [dest appendString:*it];
    }
    std::cout << dest << '\n';
    
    //you can also control the queue generator runs on and the queue your loop
    //will resume on
    auto conq = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
    dest = [NSMutableString new];
    for (auto it = co_await AsyncGenerator().resumingOnMainQueue().beginOn(conq); it; co_await it.next()) {
        [dest appendString:*it];
    }
    std::cout << dest << '\n';
}

void CoroutineRunner() {
    
    //lets run our coroutines on the main loop
    
    bool shouldKeepRunning = true;
    
    auto executeAsyncDemos = [&]() -> DispatchTask<> {
        co_await CoroutineAwaitingCoroutineDemo();
        co_await UsingGeneratorDemo();
        shouldKeepRunning = false;
    };
    
    //if you don't await a coroutine it executes on its own asynchronously
    executeAsyncDemos();
    
    NSRunLoop * theRL = [NSRunLoop currentRunLoop];
    while (shouldKeepRunning && [theRL runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]);
}

//MARK: - Block Wrapping

void BlockWrapperDemo() {
    [[BlockDemo new] callLambda];
    
    struct ConstCallable {
        void operator()() const {
            std::cout << "const callable called\n";
        }
    };
    struct MutableCallable {
        void operator()() {
            std::cout << "mutable callable called\n";
        }
    };
    
    auto q = dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0);
    dispatch_sync(q, makeBlock(ConstCallable{}));
    dispatch_sync(q, makeMutableBlock(MutableCallable{}));
    
    //makeMutableBlock must be used with mutable lambdas
    std::vector<char> initial;
    dispatch_sync(q, makeMutableBlock([buffer = initial]() mutable {
        buffer.push_back('a');
    }));
    
    struct MultiCallable {
        void operator()() const {
            std::cout << "const multi callable called\n";
        }
        void operator()() {
            std::cout << "mutable multi callable called\n";
        }
    };
    
    //no type deduction with multiple calls possible - we must tell makeBlock the type manually
    dispatch_sync(q, makeBlock<void()>(MultiCallable{}));
    dispatch_sync(q, makeMutableBlock<void()>(MultiCallable{}));
    
    //move only type
    std::unique_ptr<int> p(new int(7));
    dispatch_sync(q, makeBlock([p=std::move(p)]() {
        std::cout << "got move only " << *p << "\n";
    }));
    
    //You can save the block pointer for later. Keep in mind that
    //ObjectiveC block machinery copies the block to heap in this case.
    void (^block)() = makeBlock([](){});
    block();
    
    //This is a block object not block pointer
    auto blockObj = makeBlock([](){});
    //It can be called directly
    blockObj();
    //or converted to block pointer
    block = blockObj;
}

//MARK: - Boxing arbitrary objects

struct non_copyable {
    non_copyable() = default;
    non_copyable(const non_copyable &) = delete;
    non_copyable(non_copyable &&) = default;
    
    friend std::ostream & operator<<(std::ostream & str, const non_copyable &) {
        return str << "something!";
    }
};

void BoxingDemo() {
    
    auto str1 = box(std::string("hello"));
    static_assert(std::is_same_v<decltype(str1), NSObject<BoxedValue, BoxedComparable, NSCopying> *>);
    
    assert([str1.description isEqualToString:@"hello"]);
    auto & val1 = boxedValue<std::string>(str1);
    assert(val1 == "hello");
    val1 = "aaa";
    assert([str1.description isEqualToString:@"aaa"]);
    
    auto str2 = box<std::string>(3, 'a');
    auto & val2 = boxedValue<std::string>(str2);
    assert(val2 == "aaa");
    assert([str1 isEqualTo:str2]);
    assert([str1 compare:str2] == NSOrderedSame);
    
    decltype(str2) str3 = [str2 copy]; //NSCopying's copy: returns id, ugh!
    assert([str2 isEqualTo:str3]);
    
    auto objcHash = str3.hash;
    auto cppHash = std::hash<std::string>()(boxedValue<std::string>(str3));
    assert(objcHash == cppHash);
    
    
    auto nc = box(non_copyable{});
    static_assert(std::is_same_v<decltype(nc), NSObject<BoxedValue> *>);
    
    assert([nc.description isEqualToString:@"something!"]);
    
    @try {
        [nc copy];
        assert(false);
    }
    @catch(NSException * ex) {
        assert([ex.name isEqualTo:NSInvalidArgumentException]);
    }
}

//MARK: - Printing ObjectiveC objects to iostreams

void IOStreamDemo() {
    
    std::cout << @"abc" << '\n';
    std::cout << @1 << '\n';
    std::cout << @[@1, @2, @3] << '\n';
    std::cout << [NSError errorWithDomain:NSURLErrorDomain code:1 userInfo:nil] << '\n';
}

//MARK: - Using ObjectiveC objects as keys in maps

void MapByStringDemo() {
    
    auto another_abc = [NSString stringWithFormat:@"%s", "abc"];
    assert((NSObject*)another_abc != (NSObject*)@"abc");
    
    std::map<NSString *, std::string, NSStringLess> map;
    std::unordered_map<NSString *, std::string, NSObjectHash, NSStringEqual> umap;
    
    map[@"abc"] = "xyz";
    std::cout << map[another_abc] << '\n';
    
    
    umap[@"abc"] = "xyz";
    std::cout << umap[another_abc] << '\n';
}

void MapByNumberDemo() {
    
    auto another_25 = [NSNumber numberWithLong:25];
    assert((NSObject*)another_25 != (NSObject*)@25);
    
    std::map<NSNumber *, std::string, NSNumberLess> map;
    std::unordered_map<NSNumber *, std::string, NSObjectHash, NSNumberEqual> umap;
    
    map[@25] = "abc";
    std::cout << map[another_25] << '\n';
    
    
    umap[@25] = "abc";
    std::cout << umap[another_25] << '\n';
}

void MapByObjectDemo() {
    
    std::unordered_map<NSURL *, int, NSObjectHash, NSObjectEqual> umap;
    
    auto url1 = [NSURL URLWithString:@"http://www.google.com"];
    auto url2 = [NSURL URLWithString:@"http://www.google.com"];
    assert((NSObject*)url1 != (NSObject*)url2);
    
    umap[url1] = 5;
    std::cout << umap[url2] << '\n';
}

//MARK: - Treating NSString as an STL container

void NSStringCharAccessDemo() {
    
    auto str = @"Hello World";
    auto access = NSStringCharAccess(str);
    
    std::u16string reversed(access.rbegin(), access.rend());
    
    str = [NSString stringWithCharacters:(const unichar *)reversed.data() length:reversed.size()];
    
    std::cout << str << '\n';
}

//MARK: - Conversions between NSString and character ranges

void NSStringConversionsDemo() {
    
    using namespace std::literals;
    
    auto eq = NSStringEqual();
    
    assert(eq(makeNSString(u"abc"), @"abc"));
    assert(eq(makeNSString(u"abc"s), @"abc"));
    assert(eq(makeNSString(u"abc"sv), @"abc"));
    
    const char16_t * str = nullptr;
    assert(makeNSString(str) == nullptr);
    str = u"abc";
    assert(eq(makeNSString(str), @"abc"));
    
    assert(eq(makeNSString({'a', 'b', 'c'}), @"abc"));
    
    std::vector<char> vec = {'a', 'b', 'c'};
    assert(eq(makeNSString(vec), @"abc"));
    
    auto span = std::span(vec.begin(), vec.end());
    assert(eq(makeNSString(span), @"abc"));
    
    assert(makeStdString<char16_t>(@"abc") == u"abc");
    assert(makeStdString<char>(@"abc", 1) == "bc");
    assert(makeStdString<char32_t>(@"abc", 1, 1) == U"b");
    assert(makeStdString<wchar_t>(NSStringCharAccess(@"abc") | std::views::take(2)) == L"ab");
    NSStringCharAccess access(@"abc");
    assert(makeStdString<char8_t>(access.begin(), access.end()) == u8"abc");
}

//MARK: - main

int main(int argc, const char * argv[]) {
    
    BlockWrapperDemo();
    
    BoxingDemo();
    
    IOStreamDemo();
    
    MapByStringDemo();
    
    MapByNumberDemo();
    
    MapByObjectDemo();
    
    NSStringCharAccessDemo();
    
    NSStringConversionsDemo();
    
    CoroutineRunner();
    
    return 0;
}
