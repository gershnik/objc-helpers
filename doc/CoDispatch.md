# Asynchronous coroutines with dispatch queues

This guide assumes you are familiar with the concept of [coroutines][coroutines] either 
as [supported in C++ 20 or above][cpp-coroutines] or in another language such as JavaScript 
or Python.

<!-- TOC -->

- [Asynchronous coroutines with dispatch queues](#asynchronous-coroutines-with-dispatch-queues)
    - [Pre-requisites](#pre-requisites)
    - [Replacing dispatch_async with coroutines](#replacing-dispatch_async-with-coroutines)
        - [Controlling execution and resumption queues](#controlling-execution-and-resumption-queues)
        - [Exceptions](#exceptions)
        - [Ensuring proper queue when catching exceptions](#ensuring-proper-queue-when-catching-exceptions)
        - [Delaying co_await](#delaying-co_await)
        - [Not calling co_await](#not-calling-co_await)
    - [Switching queues](#switching-queues)
    - [Converting callbacks](#converting-callbacks)
    - [Writing coroutines](#writing-coroutines)
        - [Awaiting coroutines](#awaiting-coroutines)
        - [Returning values](#returning-values)
        - [Coroutines and exceptions](#coroutines-and-exceptions)
        - [Coroutines and queues](#coroutines-and-queues)
        - [Calling coroutines from regular functions](#calling-coroutines-from-regular-functions)
    - [Asynchronous generators](#asynchronous-generators)
        - [Iteration queues](#iteration-queues)
        - [Delaying co_await](#delaying-co_await)
        - [Iteration exceptions](#iteration-exceptions)
    - [Wrappers for Dispatch IO](#wrappers-for-dispatch-io)
    - [Usage of coroutines across .cpp and .mm files](#usage-of-coroutines-across-cpp-and-mm-files)
    - [Compiling with exceptions disabled](#compiling-with-exceptions-disabled)

<!-- /TOC -->


## Pre-requisites

You will need your compiler set to C++20 mode or above either via Xcode settings or command line.

All the code in this guide assumes inclusion of [CoDispatch.h][header] header.

```cpp
#include <objc-helpers/CoDispatch.h>
```

This is the only header you need and you can use it either from plain C++ (.cpp) or ObjectiveC++ (.mm) files.
You can also use this header with or without C++ exception support (`-fno-exceptions`). The header doesn't use RTTI and is agnostic to whether it is enabled or not. 

Currently there is no namespace you need to be `using` - all the symbols you need are in global namespace.
(There is one exceptions to this - see the section about [mixing plain C++ and ObjectiveC++ code](#usage-of-coroutines-across-cpp-and-mm-files) for more details).

## Replacing dispatch_async with coroutines

The most basic usage of coroutines can be demonstrated as follows.
Let's start with the basic traditional `dispatch_async` ladder:

```objc++

void foo() {
    //do some initial work
    int i = 1;
    dispatch_async(dispatch_get_main_queue(), ^ {
        //do more work asynchronously
        i += 7;

        dispatch_async(dispatch_get_main_queue(), ^ {
            //and more work asynchrnously again
            i += 15;
        });
    });

    //this will print 1 since dispatch_async is "fire and forget"
    NSLog(@"i=%d", i); 
}
```

This code can be replaced with a coroutine as follows

```objc++
DispatchTask<> foo() {
    //do some initial work
    int i = 1;
    co_await co_dispatch([&]() {
        //do more work asynchronously
        i += 7;
    });
    co_await co_dispatch([&]() {
        //and more work asynchrnously again
        i += 15;
    });
    //this will print 23 
    NSLog(@"i=%d", i); 
}
```

You can use any `Callable` object with `co_dispatch`: a lambda, a function pointer, a functor, and, 
yes, even an ObjectiveC block.

The callable you pass can return values to the calling coroutine. For example

```objc++
std::vector<int> vec = co_await co_dispatch([]() {
    return std::vector{1, 2, 3};
});
```

You can return pretty much anything a normal function can return: an object, a pointer or a reference (both rvalue and lvalue ones). 

> ℹ️️ Efficiency note: when returning object by value they are moved twice (if they are movable). Once from the callable body into a coroutine storage and then from there into your code object. If the object is not movable it is copied twice. (You cannot return neither copyable nor movable object from either a function)

Note that the `void` return type of `foo()` had to be changed to `DispatchTask<>` once it became a coroutine.
Coroutines return tasks - not plain values.

You are not required to use `DispatchTask` from this library - any coroutine task from other libraries will
do as well. However, using `DispatchTask` will allow you to recursively await on the coroutine itself (it is an *awaitable* task) and control its interaction with dispatch queues. 

`DispatchTask` and how to use it will be described in detail [below](#writing-coroutines).

### Controlling execution and resumption queues

Which queue does `co_dispatch` execute its callable on? By default, as in example above, it will execute on the main queue. You can have it run on any queue you wish by passing in the queue as the first parameter just like you'd do with `dispatch_async`.

```objc++
auto queue = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);

co_await co_dispatch(queue, []() {
    //this will excute on queue
});
```

Now the interesting question is _which queue will the calling coroutine resume on_ after `co_await` returns? 

The obvious answer would be "on the same queue it was originally running" but this is not what happens. By default the `co_await` can resume **EITHER on the original queue or on the same queue `co_dispatch` was executing**. 

To provide maximum performance, if it so happens that the dispatched call can run concurrently with the originating queue _and_ it completes before `co_await` has a chance to suspend the caller, `co_await` will simply complete there and then without doing any suspension. The calling coroutine will simply continue going.

If the dispatched call takes more time, `co_await` will suspend the caller and then it will eventually resumed on the same queue the dispatched call was running on. This is, again, for performance reasons. If you don't care where continuation runs (as when, for example calling from one background queue to background queue) you don't need to pay performance penalty of switching.

But what if, as so often happens you want to make sure that the resumed code runs on a specific queue? This is often the case with UI code running on the main queue is delegating work to background tasks and need to resume on the main queue to update the UI with the results of computation. You can easily accomplish this via:

```objc++
auto queue = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
auto myq = dispatch_get_main_queue();
co_await co_dispatch(queue, [](){

}).resumeOn(myq);
```

or more succinctly:

```objc++
auto queue = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
co_await co_dispatch(queue, [](){

}).resumeOnMainQueue();
```

Of course you can also request the await to resume on any queue you wish not just your original one opening the door to some really complicated scenarios.

What happens when you use `resumeOn` or `resumeOnMainThread` is as follows:
- If the dispatched call completes before the caller was suspended _and_ it runs on the same queue as the caller (this can happen on a concurrent queue) the caller simply proceeds without any queue switch.
- If the caller is suspended then and the conclusion of the dispatched call the caller is resumed on the specified queue.

Why isn't there a way to say "resume on the current queue"?  This is because, Apple, in their infinite wisdom[^1] no longer allows us to ask "what is the current queue" - this call is [deprecated][dispatch-get-current-queue]. Thus `co_dispatch` implementation couldn't resume on the "current" queue even if it wanted to - it has no idea what that queue is!

It is, however, still possible to ask "is a given queue the same one I am currently running on" thought it needs a little trick[^2]. This is how `resumeOn` avoid queue switch if the given queue happens to be the current one.


[^1]: This might have something to do with the fact that "the current queue" as such is the wrong entity to execute on since it might be a delegated part of another queue or a sequence of them.
[^2]: See `isCurrentQueue` method of `BasicPromise` class in the [header][header] for the trick 

### Exceptions

Any C++ or ObjectiveC exceptions raised in the body of your callable will be propagated back to the calling coroutine. Which means you can write the following natural code

```objc++
try {

    co_await co_dispatch([]() {

        if (something bad)
            throw std::runtime_error("something");
    });

} catch(std::exception & ex) {

}
```

Supporting this comes at some performance cost that `co_dispatch` will avoid if it can see that your callback is `noexcept` (or if compiled with exceptions disabled). Thus it is recommended that you always explicitly mark your callable `noexcept` if you do not use exceptions.

```objc++
auto result = co_await co_dispatch([]() noexcept {
    if (something)
        //return error via retun code or other means
});
```

### Ensuring proper queue when catching exceptions

> ℹ️️ Note that even if your callable is `noexcept` the `co_dispatch` call can still throw exceptions (unless you compile with exceptions disabled). 

This can happen in 2 cases:
1. Memory allocation failure. `co_dispatch` needs to allocate a small memory area to keep state shared between the caller and asynchronous code. If allocating this area fails an exception will be thrown
2. Copying/moving your callable object to throws an exception. `co_dispatch` needs to store a copy of your callable for execution on the dispatch queue. If possible it will move (and hopefully your move constructors don't throw!). If move is not available, it will copy. If copy (or move!) throws this exception will propagate out of `co_dispatch`

Note that such exceptions will also be reported directly to your calling code before anything asynchronous happen, thus they will not honor `resumeOn` request. Which means that if your `catch` handler is sensitive to which queue it is running on you will need to [manually ensure](#switching-queues) that you are on that queue

```objc++
try {
    co_await co_dispatch(...).resumeOnMainQueue();
} catch(std::exception & ex) {
    //ensure that we are on the main queue in case of an exception
    //the .resumeOnMainQueue() might not be enough
    co_await resumeOnMainQueue(); //this is noexcept
    [MyApplication doSomethingOrOther]; //this must run on main queue
}
```

### Delaying `co_await`

In all the examples above the result of `co_dispatch` was immediately passed to `co_await`. Usually this is exactly what you want to do. However, on rare occasions it might be more efficient or convenient to schedule execution early and await it later.

This is certainly possible but you need to remember to *move* the returned *awaitable* object.

```objc++
auto awaitable = co_dispatch([](){
    return 5;
});
...
int i = co_await std::move(awaitable);
```

> ℹ️️ An awaitable returned from `co_dispatch` can be awaited **only once**. The required `std::move` syntax is a gentle reminder of that. If you try to await it again you will crash. This rule applies to all awaitables produced by this library.

Note: It is possible to support multiple awaits but doing it comes at a large performance and complexity cost. The scenarios where multiple awaits are useful are so rare that supporting them does not justify the cost and penalty on normal users.

### Not calling `co_await`

Unlike JavaScript where every async function has to be awaited you do not have to `co_await` the result of `co_dispatch`.

The tasks are reference counted and will self destruct once they run to completion if not referenced by any caller. Calling `co_dispatch` without awaiting is entirely equivalent to direct call to `dispatch_async` - you "fire and forget" an asynchronous task.

## Switching queues

On occasion it might be convenient to simply switch coroutine execution to a different queue. While it is possible to accomplish it with `co_dispatch` and an empty lambda, a simple transition like this can be done much more efficiently. The library provides standalone functions that do so

```objc++
co_await resumeOn(someQueue);
//and
co_await resumeOnMainQueue();
```

This accomplishes the switch in the fastest possible manner without any extra overhead. The awaitable returned by these methods has the same behavior as any other awaitable in this library: single await only and needs to be `std::move`d if stored.

Both methods never throw exceptions and can be used in `catch` handlers to ensure running on a desired queue.

## Converting callbacks

Many Apple and 3rd party libraries on Apple platform use the callback pattern to report their results asynchronously. You call an API and pass it a callback (a block or sometimes a function). The API initiates some asynchronous work (using dispatch queues internally) and returns quickly. Later the callback is invoked on some queue with the results.

As an example here is how `NSTask` class allows you to launch a task

```objc++
auto * url = [NSURL fileURLWithPath:@"/bin/bash"];
NSError * err;
[NSTask launchedTaskWithExecutableURL:url
                            arguments:@[@"-c", @"ls"]
                                error:&err
                    terminationHandler:^(NSTask * res){
    //this is called asynchronously after the task completes
    //you can examine the res object to see its exit code etc.
}];
if (err) {
    //uh oh launchedTaskWithExecutableURL failed immediately before
    //doing anything asynchronously 
}
```

This library allows you to easily convert such callback pattern into an awaitable call that your coroutine can `co_await` on. (If you are familiar with JavaScript Promisify library this is an exact equivalent).

Here is how to do it:

```objc++
try {
    auto task = co_await makeAwaitable<NSTask *>([](auto promise) {
        auto * url = [NSURL fileURLWithPath:@"/bin/bash"];
        NSError * err;
        [NSTask launchedTaskWithExecutableURL:url
                                    arguments:@[@"-c", @"ls"]
                                        error:&err
                            terminationHandler:^(NSTask * res){
            promise.success(res);
        }];
        if (err) {
            promise.failure(std::runtime_error(err.description.UTF8String));
            //you could just throw the exception here since this part runs synchronously
        }
    });
} catch (std::runtime_error & ex) {
    //handle the exception
}
```

The call that performs the conversion is `makeAwaitable<return type>()`. You have to specify the return type as template argument because `makeAwaitable` itself cannot deduce it from anywhere - what it is is defined by your implementation of the callback. `makeAwaitable` needs to be passed a callable, usually a lambda but any callable will do, that takes one argument, conventionally called `promise` as in "a promise to fulfill". (Note that it is related to but not the same as C++ coroutine promise type). The promise object has a type but spelling it out is clunky and is left as an implementation detail. You are recommended to use `auto`.

Inside the callable it is your responsibility to invoke whatever API needs to be invoked and set up its callback however it needs to be set up. Inside the callback you need to eventually either call `promise.success(return value)` or `promise.failure(exception or std::exception_ptr)` to report an exception.

Once your callback does that the `co_await makeAwaitable...` call will resume returning the value you passed or throwing the exception you reported. Thus the general pattern for the callable is this:

```objc++

makeAwaitable<SomeType>([](auto promise) {
    anApiWithCallback(^ (callback_params) {
        if (success)
            promise.success(arguments to construct SomeType);
            //use promise.success() if SomeType is void
        else
            promise.failure(exception or std::exception_ptr);
    });
});

```

Note that `promise.success` can be passed any arguments that can be passed to `SomeType` constructor. In other words it acts like various standard library `emplace` calls. What it does, of course, is to construct an object of SomeType in a temporary storage to be passed later to the awaiting code.

### Resumption queue

Just like with `co_dispatch`, by default the awaiting coroutine may resume either immediately on the same queue it was running or on whatever random queue the callback happened to be executed. To control the resumption queue you also use the same approach as with `co_dispatch`:

```c++

try {

    auto result = co_await makeAwaitable<SomeType>([](auto promise) {
        ... as before ...
    }).resumeOn(someQueue); //or resumeOnMainQueue()

} catch(std::exception & ex) {
    //this will also execute on the queue you specify unless
    //makeAwaitable fails before invoking your callable
}

```

In general, all the rules that apply to the awaitable returned by `co_dispatch` also apply to the one retuned by `makeAwaitable`. You can delay calling `co_await` or not call it all (though in that case there seems to be no point in using `makeAwaitable` at all - you could just call the wrapped function).


### Disabling exceptions

By default `makeAwaitable` enables exception support as described above. If you don't use exceptions in either your callable or callback you can gain some performance by disabling it. The second template argument to `makeAwaitable` lets you do that:

```c++
co_await makeAwaitable<int, SupportsExceptions::No>([](auto promise) {
    anApiWithCallback(^ (callback_params) {
        promise.success(42);
        //promise.failure(...) won't compile!
    });
});
```

Note that even with `SupportsExceptions::No` it is still possible for `makeAwaitable` to throw. This can happen if internal memory allocation fails or if transferring your callable to another queue throws an exception. See the [same considerations described for `co_dispatch`](#ensuring-proper-queue-when-catching-exceptions) for the proper way of dealing with it.


## Writing coroutines

So far we were looking at how to await various things inside a coroutine. Now let's take a closer look at what you can do with coroutines themselves. 

As mentioned before you don't have to use this library `DispatchTask` as the coroutine return task. Any task from any library will do and yo will have to look at that library documentation about how to use it. 

What `DispatchTask` provides is the ability for _other coroutines_ to `co_await` the one returning it as well as interaction with Apple dispatch queues.

### Awaiting coroutines

Here is how one coroutine can await another 

```cpp
DispatchTask<> corotine1() {
    co_await co_dispatch(...);
}

DispatchTask<> coroutine2() {
    co_await corotine1();
}
```

Similar to the awaitables returned by `co_dispatch` you can store the awaitable task and await it later.

```cpp
DispatchTask<> coroutine2() {
    auto task = corotine1();
    ...
    co_await std::move(task);
}
```

A task can be awaited only once and must be moved to do so and remind you of this fact.

### Returning values

The first template parameter of `DispatchTask` is the type of the coroutine return value. By default it is `void` but it doesn't have to be.

Here is a coroutine that returns a value:

```cpp
DispatchTask<std::string> foo() {
    int i = 1;
    co_await co_dispatch([&]() {
        i += 7;
    });
    co_await co_dispatch([&]() {
        i += 15;
    });
    co_return std::to_string(i);
}

DispatchTask<> callerOfFoo() {

    std::string result = co_await foo();
}

```

### Coroutines and exceptions

The full declaration of `DispatchTask` is

```c++
template<class Ret = void, SupportsExceptions E = ...depends...>
class DispatchTask;
```

The second template parameter specifies whether the coroutine can throw an exception _when awaited_. Do not confuse this with marking the coroutine itself `noexcept`. The `noexcept` marker on the coroutine only tells whether the `DispatchTask` itself is produced in `noexcept` fashion. The coroutine can still produce exception when awaited. This is a perfectly valid coroutine

```cpp
DispatchTask<int> coroutine() noexcept /*pointless*/ {
    throw std::runtime_error("abc");
}
```

and when awaited it will, indeed throw

```cpp
DispatchTask<> anotherCoroutine()  {
    try {
        int i = co_await coroutine();
    } catch(std::runtime_error & ex) {
        //this catchblock will be executed
    }
}
```

As was mentioned above in connection to `co_dispatch` supporting exceptions comes at a cost so if you know that your coroutine body never actually throws you need to tell `DispatchTask` about it

```c++
DispatchTask<int, SupportsExceptions::No> noThrowCoroutine() {
    //if you uncomment this it will abort at runime
    //throw std::runtime_error("abc");
    co_return 7;
}
```

If you specify `SupportsExceptions::No` and throw despite of this the execution will aborted via `std::terminate()`.

By default the second template argument is set to `SupportsExceptions::Yes` if you compile with exceptions enabled and to `SupportsExceptions::No` otherwise. 

Even if you mark `DispatchTask` as `SupportsExceptions::No` and never throw any exception it is still possible for the `co_await` expression to throw. Initialization of coroutine in C++ includes memory allocation and, in principle, it can fail.
Similar to [what was described for `co_dispatch`](#ensuring-proper-queue-when-catching-exceptions) your `catch` block might need to guard against that possibility.

### Coroutines and queues

Coroutines begin their execution "eagerly" (aka "hot start"). They do not need to be awaited to start executing. One consequence of this is that coroutines start executing on whatever queue their caller is. If you want to change the queue you can switch to it just before starting the coroutine or right after entering it:

```cpp
DispatchTask<int> addOne(int val) {
    co_return val + 1;
}
DispatchTask<int> addOne(dispatch_queue_t queue, int val) {
    co_await resumeOn(queue);
    co_return val + 1;
}

DispatchTask<> caller() {
    dispatch_queue_t queue = ...;
    co_await resumeOn(queue);
    int val = addOne(5);

    ...

    val = addOne(queue, 5);
}
```

Note that the parts of coroutine prior to the first queue switch all execute synchronously like a regular function call from the caller. If you use the second approach and are writing something that should not delay caller (perhaps because the caller is on the main queue) you need to switch queues as soon as possible.  

Similar to the `co_dispatch` and `makeAwaitable` the queue on which the _caller_ is resumed after `co_await`ing the coroutine is whichever queue the coroutine happened to run when it exited. If you want a predictable queue you need to specify so in the same way:

```cpp
DispatchTask<> changesQueue() {
    auto queue = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
    co_await resumeOn(queue);
}

DispatchTask<> caller() {

    co_await changesQueue().resumeOnMainQueue();
    //without resumeOnMainQueue() here we would be running on a different queue
    //you can also say .resumeOn(some_other_queue)
}

```

### Calling coroutines from regular functions

Coroutines can await other coroutines but eventually this chain must start from a normal function.
If the caller is not itself a coroutine and that doesn't `co_await` the returned task then the task is simply executed
in "fire and forget" fashion. For example:

```objc++
DispatchTask<> reactToEvent {

    //this runs synchronously from the caller
    NSLog("something);

    //this continues asynchronously
    //the control is returned to the caller
    co_await co_dispatch(...);
}

-(void) someEventHandler {
    reactToEvent();
    //once reactToEvent() is first suspended the call above exits
    //and we continue this function execution
    //the reactToEvent() task continues to run on its own
}
```

As with `co_dispatch` and `makeAwaitable` you do not need to eventually `co_await` all tasks. They will run to completion and clean up after themselves. 

Here is a simplest example how to run coroutines from a traditional C++ `main` function:

```c++

DispatchTask<> first() {
    ...
    co_await something;
    co_await somethingElse;
}

DispatchTask<> second() {
    ...
    co_await something;
    co_await somethingElse;

    exit(0); //we need to terminate execution somehow
}

int main() {

    first();
    second();

    dispatch_main(); //this never exits
}

```

## Asynchronous generators

Generators are coroutines that can be awaited multiple times and return a new value every time they are awaited. 

To make a generator you use `DispatchGenerator` return type rather than `DispatchTask` and use `co_yield` operator inside to yield each return value. Here is a simple example:

```c++
DispatchGenerator<int> generate() {
    co_yield 1;
    co_yield 2;
    co_yield 3;
}

DispatchTask<> caller() {
    std::vector<int> res;
    for (auto it = co_await generate().begin(); it; co_await it.next()) {
        res.push_back(*it);
    }
}

```

A few things to note here:
* The generator finishes when it runs to the closing brace or you can early exit with `co_return;`. There can be no argument to `co_return` since generators doesn't have traditional "return value".
* You always have to specify return type for `DispatchGenerator`. There is no `DispatchGenerator<>` and `DispatchGenerator<void>` is won't compile.

In order to use `DispatchGenerator` object you need to obtain an `Iterator` from it. This iterator is similar in concept but not in usage to STL iterators. Unfortunately C++ has no concept of [for await][for-await] loop like JavaScript or other languages so you have to write one by hand like in example above. Similarly STL algorithms cannot work with async iterators for now.

The rules of using `Iterator` objects are as follows:

* You obtain an iterator by `co_await`ing one of the `begin()` methods on `DispatchGenerator` (described below).
* You increment an by `co_await`ing its `next()` method. You are only allowed to do so if it is not "done". Calling `next()` on a "done" iterator produces _undefined behavior_.
* To check whether iterator is "done" you cast it to `bool`. You are only allowed to do so after first obtaining the iterator or after `co_await`ing its `next()` method. 
* If an iterator is not "done" you can read its value by dereferencing it. Dereferencing a "done" iterator produces _undefined behavior_.
* The iterators are _input iterators_. Which means you can call `*it` only once. Calling it multiple times produces _undefined behavior_.

The loop in the example above demonstrates how to canonically iterate following all the rules.

> ℹ️️ Why require `it.next()` rather than "natural" `++it` for iterator increment? The problem is that `++it` canonically returns a reference to the iterator. Not that anybody ever uses it but that what it is supposed to do. The "increment" in this case return an awaitable that needs to be `co_awaited`. Thus I've decided to make the spelling different to avoid unintentionally confusing some generic code. 

There is no requirement to run the iteration to completion. Abandoning the iterator will destroy the generator. Immediately if it is suspended or once it reaches a suspension point, if not.

### Iteration queues

You obtain an iterator by calling one of the `DispatchGenerator`'s `begin()` method. Which method to use depends on where you want the iteration to run. Unlike `DispatchTask` a `DispatchGenerator` doesn't start running until you call one of the begin methods. Where it runs depends on a method:

| Method         | Where it runs |
|----------------|---------------|
| begin()        | Main queue    |
| beginOn(queue) | Given queue   |
| beginSync()    | Synchronously |

Running synchronously means that the calling coroutine is suspended and generator runs from that point without any queue switch. Of course the generator itself is free to change queues as it wishes. This mode is identical to how regular `DispatchTask`s execute.

Note that you tell the iterator where to execute only once - by using one of the `begin()` methods. After that each invocation of `next()` will use the same method. For example if you chose `begin(queue)` each invocation of `next()` will resume the generator on that queue[^3].

Which queue will the caller code resume after `co_await` on `begin()` or `next()` returns? As usual with this library by default it will resume wherever the generator ended up running before suspending. Of course you can modify this by using `resumingOn(queue)` or `resumingOnMainQueue()` call on the `DispatchGenerator` (not the iterator!). 
Here is an iteration that runs asynchronously but the body of the loop resumes always on the main queue:

```c++
DispatchTask<> caller() {
    auto queue = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);
    std::vector<int> res;
    for (auto it = co_await generate().resumingOnMainQueue().begin(queue); it; co_await it.next()) {
        res.push_back(*it);
    }
}
```

> ℹ️️ Note the spelling "resum**ing**On" as opposed to "resumeOn" in other places in this library. This is intentional to remind you that this modifies not just the `begin()` call but all subsequent calls to `next()`.

[^3]: It would have been possible, at the cost of some extra complexity to let each `next()` also be able to use a different method. There seems to be no use case for this though.

### Delaying co_await

It is possible to delay `co_await`ing the results of `begin()` and `next()`. If you do that the following usual rules apply:

* An awaitable can be awaited only once
* You must `std::move` the awaitables
* Don't forget that `*it` and `bool(it)` can be called only after awaiting!

```c++
auto begin = generator().begin();
....
auto it = co_await std::move(begin);
if (it) {
    value = *it;

    auto next = it.next();
    //do not use it after this point!
    ...

    co_await std::move(next);
    //now you can use it again
}

```

You can also safely abandon any awaitable and not `co_await` it. Doing so is safe and simply means that you stop the iteration.

### Iteration exceptions

By default generators can throw exceptions. These exceptions will be reported from awaiting `begin()` or `next()` calls.

```c++
DispatchGenerator<int> generate() {
    co_yield 1;
    co_yield 2;
    co_yield 3;
    throw std::runtime_error("oops");
}

DispatchTask<> caller() {
    std::vector<int> res;
    try {
        for (auto it = co_await generate().begin(); it; co_await it.next()) {
            res.push_back(*it);
        }
    } catch (std::exception & ex) {
       
    }
}
```

Just like with `DispatchTask` you can avoid the overhead of supporting exceptions by telling `DispatchGenerator` about that your code isn't expected to throw.

```c++
DispatchGenerator<int, SupportsExceptions::No> generate() {
    co_yield 1;
    co_yield 2;
    co_yield 3;
    //uncommenting this will terminate the application via std::terminate
    //you promised not to throw...
    //throw std::runtime_error("oops"); 
}
```

## Wrappers for Dispatch IO

Grand Central Dispatch provides methods for asynchronous I/O that rely on callback to communicate completion. This library provides convenience wrappers (implemented in terms of `makeAwaitable`) that convert them to coroutines. All operation return value of `DispatchIOResult` type when awaited. It exposes two methods: `error()` that returns operation error if any and `data()` that returns final `dispatch_data_t` object. For reads this is the data read, for writes this is data that couldn't be written.

The first two wrappers are:

```c++
co_dispatch_io_read(dispatch_io_t __nonnull channel, 
                    off_t offset, 
                    size_t length, 
                    dispatch_queue_t __nonnull queue, 
                    dispatch_io_handler_t __nullable progressHandler = nullptr);
co_dispatch_io_write(dispatch_io_t __nonnull channel, 
                     off_t offset, 
                     dispatch_data_t __nonnull data, 
                     dispatch_queue_t __nonnull queue, 
                     dispatch_io_handler_t __nullable progressHandler = nullptr);
```
These wraps [`dispatch_io_read`](https://developer.apple.com/documentation/dispatch/1388941-dispatch_io_read?language=objc) and [`dispatch_io_write`](https://developer.apple.com/documentation/dispatch/1388932-dispatch_io_write?language=objc) respectively.

Their parameters have the same meaning as wrapped function. However the last parameter: `progressHandler` is optional and need to only only be used if you want to be notified about the operation progress (to update some progress bar, perhaps). When the operation is completed `co_await`ing these will return with final `DispatchIOResult`

The second two wrappers are:

```c++
co_dispatch_read(dispatch_fd_t fd, size_t length, dispatch_queue_t __nonnull queue);
co_dispatch_write(dispatch_fd_t fd, dispatch_data_t __nonnull data, dispatch_queue_t __nonnull queue);
```
These wraps [`dispatch_read`](https://developer.apple.com/documentation/dispatch/1388933-dispatch_read?language=objc) and [`dispatch_write`](https://developer.apple.com/documentation/dispatch/1388969-dispatch_write?language=objc) respectively.

The parameters to these are the same as to wrapped functions. 

Here is a small example of writing to and reading from a file

```c++
auto queue = dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0);

int wfd = open("testfile", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, S_IRUSR | S_IWUSR);

dispatch_data_t wd = dispatch_data_create("Hello World!", 12, queue,  DISPATCH_DATA_DESTRUCTOR_DEFAULT);
do {
    auto res = co_await co_dispatch_write(wfd, hello, queue).resumeOnMainQueue();
    if (res.error()) {
        ... handle error...
    }
    wd = res.data();
} while (wd);
close(wfd);

int rfd = open("testfile", O_RDONLY | O_CLOEXEC, S_IRUSR | S_IWUSR);

size_t remaining = 12;
do {
    auto res = co_await co_dispatch_read(rfd, remaining, queue).resumeOnMainQueue();
    if (res.error()) {
        ... handle error ...
    }
    ... use res.data() ...
    remaining -= dispatch_data_get_size(res.data());
} while (remaining);


```


## Usage of coroutines across .cpp and .mm files

As mentioned before you can use `CoDispatch.h` header and all the facilities described above in either plain C++ (.cpp) or ObjectiveC++ (.mm) code. If your entire codebase is composed of only one of them that's all there is to it - things will just work. If you mix C++ and ObjectiveC++ in the same executable or library there is one gotcha to be aware of.

The problem is that the _implementation_ of various library classes is actually different when compiled in C++ vs. ObjectiveC++ mode. This has to do with the nature of things like `dispatch_queue_t` or blocks. In plain C++ these are just plain pointers - the code that uses them needs to manually perform reference counting using `dispatch_retain`, `Block_copy` and such. In ObjectiveC++ these are ARC "smart pointers" that the compiler manages automatically. 

If this library naively provided the same class _names_ to the calling code regardless of whether it is C++ or ObjectiveC++ the generated classes would have the same name but different implementation! This would cause a severe violation of [One Definition Rule][odr]. With very very bad consequences at runtime.

To avoid this the library puts almost all its code into an `inline namespace` whose name is different depending on which mode it is compiled under. Thus when you use, say, `DispatchTask` you actually use:
* `CoDispatch::DispatchTask` if compiled under ObjectiveC++
* `CoDispatchCpp::DispatchTask` if compiled under C++

This largely works invisibly to you but there is one situation where it does become visible. Suppose you have a common header used from both C++ and ObjectiveC++ that declares a coroutine:

```c++
//CommonHeader.h
DispatchTask<int> someTask();
```

The coroutine is implemented in C++
```c++
//file1.cpp

#include "CommonHeader.h"

//actually defines CoDispatchCpp::DispatchTask<int> someTask()
DispatchTask<int> someTask() {
    co_return 7;
}
```

And used in ObjectiveC++:

```c++
//file1.mm

#include "CommonHeader.h"

void someFunc() {

    //expects CoDispatch::DispatchTask<int> someTask()
    someTask();
}
```

The situation could be reversed, of course. This will compile just fine but fail at link time because the name of the function defined in .cpp in not the same as expected in .mm. 

To fix this you need to specify the namespace used in the definition explicitly in the header file:

```c++
//CommonHeader.h
CoDispatchCpp::DispatchTask<int> someTask(); 
```

## Compiling with exceptions disabled

As mentioned [before](#pre-requisites), this library can be used with C++ exception support disabled (`-fno-exceptions` compiler switch). In this mode everything defaults to `SupportsExceptions::No` and the library never tries to propagate exceptions. 

To enable mixing of code compiled with exceptions disabled and enabled in the same executable the inline namespaces used become

* `CoDispatchNoexcept` if compiled under ObjectiveC++
* `CoDispatchCppNoexcept` if compiled under C++

As explained in the [previous section](#usage-of-coroutines-across-cpp-and-mm-files) you might need to be aware of those if you use common headers.

<!-- Links -->

[coroutines]: https://en.wikipedia.org/wiki/Coroutine
[cpp-coroutines]: https://en.cppreference.com/w/cpp/language/coroutines
[dispatch-get-current-queue]: https://developer.apple.com/documentation/dispatch/1493248-dispatch_get_current_queue?language=objc
[header]: ../include/objc-helpers/CoDispatch.h
[for-await]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/for-await...of
[odr]: https://en.cppreference.com/w/cpp/language/definition

<!-- End Links --->

