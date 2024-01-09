# ObjC-Helpers #

An ever-growing collection of utilities to make coding on Apple platforms in C++ or ObjectiveC++ more pleasant

<!-- TOC depthfrom:2 -->

- [What's included?](#whats-included)
    - [BlockUtil.h](#blockutilh)
    - [CoDispatch.h](#codispatchh)
    - [BoxUtil.h](#boxutilh)
    - [NSObjectUtil.h](#nsobjectutilh)
    - [NSStringUtil.h](#nsstringutilh)
    - [NSNumberUtil.h](#nsnumberutilh)
    - [XCTestUtil.h](#xctestutilh)
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

### `BoxUtil.h` ###

Sometimes you want to store a C++ object where an ObjectiveC object is expected. Perhaps there is
some `NSObject * tag` which you really want to put an `std::vector` in or something similar. You can,
of course, do that by creating a wrapper ObjectiveC class that stores `std::vector` but it is a huge annoyance. Yet another ObjectiveC class to write (so a new header and a .mm file) lots of boilerplate code for `init` and value access and, after all that, it is going to to be `std::vector` specific. If you later need to wrap another C++ class you need yet another wrapper. 

For plain C structs ObjectiveC has a solution: `NSValue` that can store any C struct and let you retrieve it back later. Unfortunately in C++ this only works for "trivially copyable" types (which more or less correspond to "plain C structs"). Trying to stick anything else in `NSValue` will appear to work but likely do very bad things - it simply copies object bytes into it and out! Whether bytes copied out will work as the original object is undefined.

To solve this issue `BoxUtil.h` provides generic facilities for wrapping and unwrapping of any C++ object in an `NSObject`-derived classes without writing any code. Such wrapping and unwrapping of native objects in higher-level language ones are usually called "boxing" and "unboxing", hence the
name of the header and it's APIs. 

The only requirement for the C++ class to be wrappable is having a public destructor and at least one public constructor. The constructor doesn't need to be default - boxing works with objects that need to be "emplaced".

You use it like this:

```objc++
std::vector<int> someVector{1,2,3};
//this copies the vector into the wrapper
NSObject * obj1 = box(someVector);
//and this moves it
NSObject * obj2 = box(std::move(someVector));
//you can also do this
NSObject * obj3 = box(std::vector<int>{1,2,3});
//and you can emplace the object directly rather than copy or move it
NSObject * obj4 = box<std::vector<int>>(5, 3); //emplaces {3,3,3,3,3}

//You can get a reference to wrapped object
//This will raise an ObjectiveC exception if the type doesn't macth

auto & vec = boxedValue<std::vector<int>>(obj1);
assert(vec.size() == 3);
assert(vec[1] == 2);

The reference you get back is mutable by default. If you want immutability do this
NSObject * immuatbleObj = box<const std::vector<int>>(...any of the stuff above...);

//if your C++ object has a copy constructor the wrapper 
//will implement NSCopying
auto * obj5 = (NSObject *)[obj1 copy];

//this uses operator== if available, which it is
assert([obj1 isEqual:obj3]);

//and this uses std::hash if available
//it will raise an exception if you have operator== but not std::hash!
//as incositent equality and hashing is one of the most common ObjectiveC errors
auto hash = obj1.hash

//you can obtain a sensible description
//it will try to use:
//std::to_string 
//iostream << 
//fall back on "boxed object of type <name of the class>"

auto desc = obj1.description;

//if your object supports <=> operator that returns std::strong_ordering
//you can use compare: method
assert([box(5) compare:box(6)] == NSOrderingAscending);

```

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

### XCTestUtil.h ###

When using XCTest framework you might be tempted to use `XCTAssertEqual` and similar on C++ objects. While this works and works safely you will quickly discover that when the tests fail you get a less than useful failure message that shows _raw bytes_ of the C++ object instead of any kind of logical description. This happens because in order to obtain the textual description of the value `XCTAssertEqual` and friends stuff it into an `NSValue` and then query its description. And, as mentioned in [BoxUtil.h](#boxutilh) section, `NSValue` simply copies raw bytes of a C++ object. 

While this is still safe, because nothing except the description is ever done with those bytes the end result is hardly usable. To fix this `XCTestUtil.h` header provides the following replacement macros:

- `XCTAssertCppEqual`
- `XCTAssertCppNotEqual`
- `XCTAssertCppGreaterThan`
- `XCTAssertCppGreaterThanOrEqual`
- `XCTAssertCppLessThan`
- `XCTAssertCppLessThanOrEqual`

That, in the case of failure, try to obtain description using the following methods:

- If there is an ADL call `testDescription(obj)` that produces `NSString *`, use that.
- Otherwise, if there is an ADL call `to_string(obj)` in `using std::to_string` scope, use that
- Otherwise, if it is possible to do `ostream << obj`, use that
- Finally produce `"<full name of the type> object"` string.

Thus if an object is printable using the typical means those will be automatically used. You can also make your own objects printable using either of the means above. The `testDescription` approach specifically exists to allow you to print something different for tests than in normal code.

### General notes ###

For all comparators `nil`s are handled properly. A `nil` is equal to `nil` and is less than any non-`nil` object.

Why would you want to have `NSObject`s as keys in `std::map` and such instead of using `NSDictionary`? Usually because you want the values to be a non ObjectiveC type, require efficiency or STL compatibility.


