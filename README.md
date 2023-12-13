# ObjC-Helpers #

A collection of utilities to make coding on Apple platforms in C++ or ObjectiveC++ more pleasant

<!-- TOC depthfrom:2 -->

- [What's included?](#whats-included)
    - [BlockUtil.h](#blockutilh)
    - [CoDispatch.h](#codispatchh)
    - [NSObjectUtil.h](#nsobjectutilh)
    - [NSStringUtil.h](#nsstringutilh)
    - [NSNumberUtil.h](#nsnumberutilh)
    - [General notes](#general-notes)

<!-- /TOC -->

## What's included? ##

The library is a collection of mostly independent header files. There is nothing to link with. Simply add these headers to your include path and include them as needed.

`sample` directory contains a sample that demonstrates the usage of main features.

The headers are as follows:

### `BlockUtil.h` ###

Allows clean and safe usage of C++ lambdas instead of ObjectiveC blocks (which are confusingly also available
in pure C++ on Apple platforms as an extension).
 
Why not use blocks? Blocks capture any variable mentioned in them automatically which
causes no end of trouble with inadvertent capture and circular references. The most 
common one is to accidentally capture `self` in a block. There is no protection in 
ObjectiveC - you have to manually inspect code to ensure this doesn't happen. 
Add in code maintenance, copy/paste and you are almost guaranteed to have bugs.
Even if you have no bugs your block code is likely littered with strongSelf
nonsense making it harder to understand.
 
C++ lambdas force you to explicitly specify what you capture, removing this whole problem.
Unfortunately lambdas cannot be used as blocks. This header provides utility functions
to fix this.
The intended usage is

```objc++ 
[foo doBar:makeBlock([weakSelf = makeWeak(self)] () {
    auto self = makeStrong(weakSelf);
    if (!self)
        return;
    [self doSomething];
}];
```
 
 Note that since in C++ code `self` isn't anything special we can use it as variable name
 for, well, actual self.

 Since blocks are supported even in plain C++ by Apple's clang compiler as an extension you can 
 use this facility both from plain C++ (.cpp) and ObjectiveC++ (.mm) files.

### `CoDispatch.h` ###

Allows you to use **asynchronous** C++ coroutines that execute on GCD dispatch queues. Yes there is [this library](https://github.com/alibaba/coobjc) but it is big, targeting Swift and ObjectiveC rather than C++/\[Objective\]C++ and has a library to integrate with. It also has more features, of course. Here you get basic powerful C++ coroutine support in a single not very large (~800 loc) header.

Working with coroutines is discussed in greater detail in [a separate doc](doc/CoDispatch.md).

Here is a small sample of what you can do:

```objc++
DispatchTask<int> coro() {

    //this will execute asyncronously on the main queue
    int i = co_await co_dispatch([]() {
        return 7;
    });

    //you can specify a different queue of course
    auto queue = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
    int j = co_await co_dispatch(queue, []() {
        return 42;
    }).resumeOnMainQueue(); //add this to resume back on main queue

    //you can convert ObjC APIs with asynchronous callbacks to couroutines
    auto status = co_await makeAwaitable<int>([](auto promise) {
        NSError * err;
        [NSTask launchedTaskWithExecutableURL:[NSURL fileURLWithPath:@"/bin/bash"]
                                    arguments:@[@"-c", @"ls"]
                                        error:&err
                           terminationHandler:^(NSTask * res){
            promise.success(res.terminationStatus);
        }];
        if (err)
            throw std::runtime_error(err.description.UTF8String);
    }).resumeOnMainQueue();

    //this will switch execution to a different queue
    co_await resumeOn(queue);
}

//coroutines can await other corotines
DispatchTask<int> anotherCoro() {
    int res = co_await coro();

    co_return res;
}

//you can also have asynchronous generators
DispatchGenerator<std::string> generator() {
    co_yield "Hello";
    co_yield "World";
    //in real life you probably will use something like
    //co_yield co_await somethingAsync(); 
}

DispatchTask<int> useGenerator() {
    std::vector<std::string> dest;
    //this will run generator asynchrnously on the main queue
    for (auto it = co_await generator().begin(); it; co_await it.next()) {
        res.push_back(*it);
    }

    //you can also say things like
    //auto it = generator().resumingOnMainQueue().beginOn(queue)
    //to control the running and resuming queues
}

int main() {

    //fire and forget
    anotherCoro();
    useGenerator();

    dispatch_main();
}
```

This facility can also be used both from plain C++ (.cpp) and ObjectiveC++ (.mm) files.


### `NSObjectUtil.h` ###

`NSObjectEqual` and `NSObjectHash` - functors that provide equality and hash code for any NSObject and allow them to be used as keys in `std::unordered_map` and `std::unordered_set` for example. These are implemented in terms of `isEqual` and `hash` methods of `NSObject`. 

`operator<<` for any NSObject to print it to an `std::ostream`. This behaves exactly like `%@` formatting flag by delegating either to `descriptionWithLocale:` or to `description`.

### `NSStringUtil.h` ###

`NSStringLess` and `NSStringLocaleLess` comparators. These allow `NSString` objects to be used as keys in `std::map` or `std::set `as well as used in STL sorting and searching algorithms..  

`NSStringEqual` comparator. This is more efficient than `NSObjectEqual` and is implemented in terms of `isEqualToString`. 

`operator<<` to print a `NSString` to an `std::ostream`. This outputs `UTF8String`.

`NSStringCharAccess` -  a fast accessor for NSString characters via STL container interface. This uses approach similar to [`CFStringInlineBuffer`](https://developer.apple.com/documentation/corefoundation/cfstringinlinebuffer?language=objc) one.

### `NSNumberUtil.h` ###

`NSNumberLess` comparator. This allows `NSNumber` objects to be used as keys in `std::map` or `std::set` as well as used in STL sorting and searching algorithms.  

`NSNumberEqual` comparator. This is more efficient than `NSObjectEqual` and is implemented in terms of `isEqualToNumber`. 

### General notes ###

For all comparators `nil`s are handled properly. A `nil` is equal to `nil` and is less than any non-`nil` object.

Why would you want to have `NSObject`s as keys in `std::map` and such instead of using `NSDictionary`? Usually because you want the values to be a non ObjectiveC type, require efficiency or STL compatibility.


