/*
 Copyright 2020 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_BLOCK_UTIL_INCLUDED
#define HEADER_BLOCK_UTIL_INCLUDED

#include <functional>

/*
 The purpose of this header is to allow nice and safe usage of C++ lambdas instead of
 ObjectiveC blocks.
 
 Why not use blocks? Blocks capture any variable mentioned in them automatically which
 causes no end of trouble with inadvertent capture and circular references. The most 
 common one is to accidentally capture `self` in a block. There is no protection in 
 ObjectiveC - you have to manually inspect code to ensure this doesn't happen. 
 Add in code maintainance, copy/paste and you are almost guaranteed to have bugs.
 Even if you have no bugs your block code is likley littered with strongSelf
 nonsense making it harder to understand.
 
 C++ lambdas force you to explicitly specify what you capture, removing this whole problem.
 Unfortunately lambdas cannot be used as blocks. This header provides utility functions
 to fix this.
 The intended usage is
 
 [foo doBar:makeBlock([weakSelf = makeWeak(self)] () {
    auto self = makeStrong(self);
    if (!self)
        return;
    [self doSomething];
 }];
 
 Note that since in C++ code `self` isn't anything special we can use it as variable name
 for, well, actual self.
 
 */


//Internal stuff in this namespace
namespace BlockUtil
{

    template<class T>
    auto deduceFunctionType(T callable)
    {
        return std::function(callable);
    }

#ifdef __OBJC__
    namespace ObjC
    {
        template<class T, class Ret, class... Args>
        auto makeBlockHelper(T callable, std::function<Ret (Args...)> * dummy) -> Ret (^) (Args...)
        {
            return ^ (Args... args) {
                
                return callable(std::forward<Args>(args)...);
            };
        }

        template<class T>
        auto makeBlock(T callable)
        {
            using DeducedFunctionType = decltype(deduceFunctionType(callable));
            return makeBlockHelper(callable, (DeducedFunctionType *) nullptr);
        }
    }
#else
    namespace Cpp
    {
        template<class T, class Ret, class... Args>
        auto makeBlockHelper(T callable, std::function<Ret (Args...)> * dummy) -> Ret (^) (Args...)
        {
            return Block_copy(^ (Args... args) {
                
                return callable(std::forward<Args>(args)...);
            });
        }

        template<class T>
        auto makeBlock(T callable)
        {
            using DeducedFunctionType = decltype(deduceFunctionType(callable));
            return makeBlockHelper(callable, (DeducedFunctionType *) nullptr);
        }
    }
#endif

}

/**
 Usage:
 `makeBlock([...](...) { ... })`
 or
 ```
 std::function func = ...;
 makeBlock(func)
 ```
 */
#ifdef __OBJC__
    using BlockUtil::ObjC::makeBlock;
#else
    using BlockUtil::Cpp::makeBlock;
#endif


/**
 Usage:
 ```
 [foo doBar:makeBlock([weakSelf = makeWeak(self)] () {
    
 }];
 ```
 */
template<class T>
auto makeWeak(T * __strong obj) -> T * __weak
{
    return obj;
}

/**
 Usage:
 ```
 [foo doBar:makeBlock([weakSelf = makeWeak(self)] () {
    auto self = makeStrong(self);
    if (!self)
        return;
 }];
 ```
 */
template<class T>
auto makeStrong(T * __weak obj) -> T * __strong
{
    return obj;
}


#endif
