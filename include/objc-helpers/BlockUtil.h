/*
 Copyright 2020 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_BLOCK_UTIL_INCLUDED
#define HEADER_BLOCK_UTIL_INCLUDED

#if !__cpp_concepts
    #error This header requires C++20 mode or above with concepts support
#endif

#include <concepts>
#include <type_traits>
#include <cstdlib>
#include <cassert>
#include <cstddef>
#include <utility>

#include <Block.h>

/**
 @file
  
 With modern Clang compiler you can seamlessly convert C++ lambdas to blocks like this:
 ```
 dispatch_async(someQueue, []() {
     //do something
 })
 ```
 This works and works great but there are a few things that don't:
 * You can only pass a *lambda* as a block, not any other kind of callable. For example this does not compile:
   ```
   struct foo { void operator()() const {} };
   dispatch_async(someQueue, foo{});
   ```
 * You cannot pass a *mutable* lambda this way. This doesn't compile either
   ```
   dispatch_async(someQueue, []() mutable {
      //do something
   });
   ```
   Neither cannot you pass a block that captures anything mutable (like your lambda) - captured variables are all const
 * Your lambda captured variables are always *copied* into the block, not *moved*. If you have captures that are
   expensive to copy - oh well...
 * Because of the above you cannot have move-only thinks in your block. Forget about using `std::unique_ptr` for example.

 This  header gives you an ability to solve all of these problems.

 It provides two functions: `makeBlock` and `makeMutableBlock` that take any C++ callable as an input and return an object
 that is implicitly convertible to a block and can be passed to any block-taking API. They (or rather the object they return)
 have the following features:

 * You can wrap any C++ callable, not just a lambda.
 * `makeBlock` returns a block that invokes `operator()` on a `const` callable and
   `makeMutableBlock` returns a block that invokes it on a non-const one. Thus `makeMutableBlock` can be used with
   mutable lambdas or any other callable that provides non-const `operator()`.
 * If callable is movable it will be moved into the block, not copied. It will also be moved if the block is "copied to heap"
   by ObjectiveC runtime or `Block_copy` in plain C++.
 * It is possible to use move-only callables.
 * All of this is accomplished with NO dynamic memory allocation
 
 It also provides two helpers: `makeWeak` and `makeStrong` that simplify the "strongSelf"
 casting dance around avoiding circular references when using blocks/lambdas.

 Here is the intended usage:

 ```
 dispatch_async(someQueue, [weakSelf = makeWeak(self)] () {
     auto self = makeStrong(weakSelf);
     if (!self)
         return;
     [self doSomething];
 });
*/


namespace BlockUtil
{
#ifdef __OBJC__
    inline namespace ObjC {
#else
    inline namespace Cpp {
#endif
            
        //see https://clang.llvm.org/docs/Block-ABI-Apple.html for details on enums and layout
        
        enum BlockFlags : int {
            // Set to true on blocks that have captures (and thus are not true
            // global blocks) but are known not to escape for various other
            // reasons. For backward compatibility with old runtimes, whenever
            // BlockIsNoEscape is set, BLOCK_IS_GLOBAL is set too. Copying a
            // non-escaping block returns the original block and releasing such a
            // block is a no-op, which is exactly how global blocks are handled.
            BlockIsNoEscape      =  (1 << 23),

            BlockHasCopyDispose  =  (1 << 25),
            BlockHasCtor =          (1 << 26), // helpers have C++ code
            BlockIsGlobal =         (1 << 28),
            BlockHasStret =         (1 << 29), // IFF BlockHasSignature
            BlockHasSignature =     (1 << 30),
        };
        
        template<class Block>
        struct Descriptor {
            unsigned long int reserved;
            unsigned long int Block_size;
            void (*copy_helper)(Block * dst, Block * src);
            void (*dispose_helper)(Block *);
        };
        
        template<class Derived, class Ret, class... Args>
        struct Block {
            void * isa = &_NSConcreteStackBlock;
            int flags = BlockHasCopyDispose | BlockHasCtor | BlockHasStret;
            int reserved;
            Ret (* invoke)(Derived *, Args...) = Derived::invokeBlock;
            Descriptor<Derived> * descriptor = &s_descriptor;
            
            static inline Descriptor<Derived> s_descriptor = {0, sizeof(Derived), Derived::copyBlock, Derived::disposeBlock};
        };
        
        /**
         * A block object that wraps a C++ callable
         *
         * @tparam Callable the callable object to wrap
         * @tparam IsMutable whether the operator() should be called on non-const Callable or a const one
         * @tparam Ret return value type
         * @tparam Args call argument types
         *
         * Usually you don't want to create instances of this class directly. Instead use `makeBlock` and
         * `makeMutableBlock` factory functions that will do all the necessary deductions and checks for you.
         *
         * This class is NOT the same as block pointer like void (^)(). It IS a block and is more akin to an std::function
         * (only without any dynamic memory allocations). It can be copied by value, stored etc. and,
         * when needed, **converted to a block pointer**
         *
         * The lifetime of the block is the lifetime of this object. However, when converted to a block pointer
         * the usual block machinery kicks in. In ObjectiveC++ converting to a local variable ARC pointer
         * will internally copy the block to the heap and extend its lifetime.
         * @code
         * int (^block)() = makeBlock([](){ });
         * @endcode
         *
         * However **this doesn't work** if you convert into a pointer that outlives the current block. In this
         * case you need to use copy() method
         *
         * @code
         *  _instanceVar = copy(makeBlock([](){ }));
         * @endcode
         *
         * In plain C++ with no ARC you **always** need to use copy() method for this.
         * @code
         * int (^block)() = copy(makeBlock([](){ }));
         * ...
         * Block_release(block);
         * @endcode
         */
        template<class Callable, bool IsMutable, class Ret, class... Args>
        class BlockWithCallable : Block<BlockWithCallable<Callable, IsMutable, Ret, Args...>, Ret, Args...> {
            
            friend Block<BlockWithCallable, Ret, Args...>;
            
        public:
            using BlockType = Ret (^)(Args...);
            using TypeToCall = std::conditional_t<IsMutable, Callable, std::add_const_t<Callable>>;
            
            struct AlwaysTrue {
                AlwaysTrue() {}
                AlwaysTrue(bool) {};
                void operator=(bool) {};
                operator bool() const { return true; }
            };
            using CopyCanBeMoveFlag = std::conditional_t<std::is_copy_constructible_v<Callable>, bool, AlwaysTrue>;
            
        public:
            BlockWithCallable(const Callable & callable)
            requires(std::is_copy_constructible_v<Callable>) : copyCanBeMove(false) {
                new (callableBuf) Callable(callable);
            }
            BlockWithCallable(Callable && callable)
            requires(std::is_move_constructible_v<Callable>) : copyCanBeMove(false) {
                new (callableBuf) Callable(std::move(callable));
            }
            ~BlockWithCallable() {
                disposeBlock(this);
#ifndef NDEBUG
                //catch lifecycle issues in debug mode
                this->invoke = invokeDeletedBlock;
#endif
            }
            
            BlockWithCallable(const BlockWithCallable & src)
            requires(std::is_copy_constructible_v<Callable>) {
                reallyCopyBlock(this, &src);
            }
            
            BlockWithCallable(BlockWithCallable && src) 
            requires(std::is_move_constructible_v<Callable>) {
                moveBlock(this, &src);
            }
             
            BlockWithCallable & operator=(const BlockWithCallable & src) 
            requires(std::is_copy_constructible_v<Callable>) {
                if (this != &src) {
                    disposeBlock(this);
                    reallyCopyBlock(this, &src);
                }
                return *this;
            }
            
            BlockWithCallable & operator=(BlockWithCallable && src) 
            requires(std::is_move_constructible_v<Callable>) {
                if (this != src) {
                    disposeBlock(this);
                    moveBlock(this, &src);
                }
                return *this;
            }
            
            operator BlockType() const & noexcept
                { return castToBlockType();}
            
            operator BlockType() && noexcept {
                this->copyCanBeMove = true;
                return castToBlockType();
            }
            
            auto get() const & noexcept -> BlockType {
                return castToBlockType();
            }
            
            auto get() && noexcept -> BlockType {
                this->copyCanBeMove = true;
                return castToBlockType();
            }
            
            auto operator()(Args... args) -> Ret {
                auto & callable = *reinterpret_cast<TypeToCall *>(this->callableBuf);
                return callable(std::forward<Args>(args)...);
            }
            
#ifdef __OBJC__
            friend auto copy(const BlockWithCallable & obj) -> BlockType {
                auto dummy = (BlockType)obj;
                return dummy;
            }
            friend auto copy(BlockWithCallable && obj) -> BlockType {
                auto dummy = (BlockType)std::move(obj);
                return dummy;
            }
#else
            friend auto copy(const BlockWithCallable & obj) -> BlockType {
                return Block_copy((BlockType)&obj);
            }
            friend auto copy(BlockWithCallable && obj) -> BlockType {
                obj.copyCanBeMove = true;
                return Block_copy((BlockType)&obj);
            }
#endif

        private:
            static void copyBlock(BlockWithCallable * dst, BlockWithCallable * src) {
                if constexpr (std::is_move_constructible_v<Callable>) {
                    if (src->copyCanBeMove) {
                        moveBlock(dst, src);
                        return;
                    }
                }
                if constexpr (!std::is_copy_constructible_v<Callable>) {
                    //BUGBUG: copy reached for a move-only object
                    abort();
                } else {
                    reallyCopyBlock(dst, src);
                }
            }
            static void reallyCopyBlock(BlockWithCallable * dst, const BlockWithCallable * src) {
                new (dst->callableBuf) Callable(*reinterpret_cast<const Callable *>(src->callableBuf));
                dst->copyCanBeMove = false;
            }
            static void moveBlock(BlockWithCallable * dst, BlockWithCallable * src) {
                new (dst->callableBuf) Callable(std::move(*reinterpret_cast<Callable *>(src->callableBuf)));
                dst->copyCanBeMove = false;
            }
            static void disposeBlock(BlockWithCallable * me) {
                reinterpret_cast<Callable *>(me->callableBuf)->~Callable();
            }
            static auto invokeBlock(BlockWithCallable * me, Args... args) -> Ret {
                auto & callable = *reinterpret_cast<TypeToCall *>(me->callableBuf);
                return callable(std::forward<Args>(args)...);
            }
            
            static auto invokeDeletedBlock(BlockWithCallable *, Args...) -> Ret {
                //You are trying to invoke a deleted block (do you need to copy() it?)"
                abort();
            }
            
            auto castToBlockType() const noexcept -> BlockType {
#ifdef __OBJC__
                //This, despite appearances, does the right thing by fooling ARC
                //When the return value is assigned to a block pointer ARC will do
                //objc_retainBlock copying to heap as expected.
                //However, when the return value is immediately passed to another function
                //it will do objc_retainAutoreleasedReturnValue which is no-op for our stack
                //pointer
                return (__bridge_transfer BlockType)this;
#else
                //In absence of ARC this cannot outlive us
                //Use copy(...) below to retain the pointer by copying to heap
                return (BlockType)this;
#endif
            }
            
            
            
        private:
            alignas(alignof(Callable)) std::byte callableBuf[sizeof(Callable)];
            [[no_unique_address]] CopyCanBeMoveFlag copyCanBeMove;
        };
                
        /**
         Things that can be wrapped in a block must be destructible and copy and/or move constructible
         */
        template<class T>
        concept BlockWrappable = std::is_destructible_v<T> && (std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>);
        
        template<bool IsMutable, class Callable>
        struct StripSignature;

        #  if defined(__cpp_static_call_operator) && __cpp_static_call_operator >= 202207L

        template <bool IsMutable, class Ret, class ...Args>
        struct StripSignature<IsMutable, Ret(*)(Args...)> {
            using type = Ret(Args...);
        };

        template <bool IsMutable, class Ret, class ...Args>
        struct StripSignature<IsMutable, Ret(*)(Args...) noexcept> {
            using type = Ret(Args...);
        };

        #  endif // defined(__cpp_static_call_operator) && __cpp_static_call_operator >= 202207L

        template<class Ret, class T, class ...Args>
        struct StripSignature<true, Ret (T::*) (Args...)> { using type = Ret(Args...); };
        template<bool IsMutable, class Ret, class T, class ...Args>
        struct StripSignature<IsMutable, Ret (T::*) (Args...) const> { using type = Ret(Args...); };
        template<class Ret, class T, class ...Args>
        struct StripSignature<true, Ret (T::*) (Args...) volatile> { using type = Ret(Args...); };
        template<bool IsMutable, class Ret, class T, class ...Args>
        struct StripSignature<IsMutable, Ret (T::*) (Args...) const volatile> { using type = Ret(Args...); };

        template<class Ret, class T, class ...Args>
        struct StripSignature<true, Ret (T::*) (Args...) &> { using type = Ret(Args...); };
        template<bool IsMutable, class Ret, class T, class ...Args>
        struct StripSignature<IsMutable, Ret (T::*) (Args...) const &> { using type = Ret(Args...); };
        template<class Ret, class T, class ...Args>
        struct StripSignature<true, Ret (T::*) (Args...) volatile &> { using type = Ret(Args...); };
        template<bool IsMutable, class Ret, class T, class ...Args>
        struct StripSignature<IsMutable, Ret (T::*) (Args...) const volatile &> { using type = Ret(Args...); };

        template<class Ret, class T, class ...Args>
        struct StripSignature<true, Ret (T::*) (Args...) noexcept> { using type = Ret(Args...); };
        template<bool IsMutable, class Ret, class T, class ...Args>
        struct StripSignature<IsMutable, Ret (T::*) (Args...) const noexcept> { using type = Ret(Args...); };
        template<class Ret, class T, class ...Args>
        struct StripSignature<true, Ret (T::*) (Args...) volatile noexcept> { using type = Ret(Args...); };
        template<bool IsMutable, class Ret, class T, class ...Args>
        struct StripSignature<IsMutable, Ret (T::*) (Args...) const volatile noexcept> { using type = Ret(Args...); };

        template<class Ret, class T, class ...Args>
        struct StripSignature<true, Ret (T::*) (Args...) & noexcept> { using type = Ret(Args...); };
        template<bool IsMutable, class Ret, class T, class ...Args>
        struct StripSignature<IsMutable, Ret (T::*) (Args...) const & noexcept> { using type = Ret(Args...); };
        template<class Ret, class T, class ...Args>
        struct StripSignature<true, Ret (T::*) (Args...) volatile & noexcept> { using type = Ret(Args...); };
        template<bool IsMutable, class Ret, class T, class ...Args>
        struct StripSignature<IsMutable, Ret (T::*) (Args...) const volatile & noexcept> { using type = Ret(Args...); };

        template<bool IsMutable, class Ret, class ...Args>
        consteval auto deduceCallType(Ret (*)(Args...)) {
            return (Ret (*)(Args...))nullptr;
        }
        
        template<bool IsMutable, class Callable, class Stripped = typename StripSignature<IsMutable, decltype(&Callable::operator())>::type>
        consteval auto deduceCallType(Callable *) {
            return (Stripped *)nullptr;
        }
        
        /**
         * From a given Callable deduce the Ret(Args...) type of how it can be called
         */
        template<bool IsMutable, class Callable>
        using DeduceCallType = std::remove_reference_t<decltype(*deduceCallType<IsMutable>((Callable *)nullptr))>;
        
        /**
         This concept is satisfied when a T provides exactly one "operator()" callable on const object
         
         If it is not satisfied we cannot deduce how to call a callable without user telling us.
         */
        template<class T>
        concept UniquelyConstCallable = requires {
            typename DeduceCallType<false, T>;
        };
        
        /**
         This concept is satisfied when a T provides exactly one "operator()" callable on a non-const object
         
         If it is not satisfied we cannot deduce how to call a callable without user telling us.
         */
        template<class T>
        concept UniquelyMutableCallable = requires {
            typename DeduceCallType<true, T>;
        };
        
        template<bool IsMutable, class Callable, class Ret, class... Args>
        consteval auto isInvocableWithHelper(Ret (*)(Args...)) -> bool {
            using ObjectType = std::remove_cvref_t<Callable>;
            using TypeThatWillBeCalled = std::conditional_t<IsMutable, ObjectType, std::add_const_t<ObjectType>>;
            return std::is_invocable_r_v<Ret, TypeThatWillBeCalled, Args...>;
        }
        
        template<bool IsMutable, class Callable, class CallType>
        consteval auto isInvocableWith() -> bool {
            return isInvocableWithHelper<IsMutable, Callable>((CallType *)nullptr);
        }
        
        /**
         Whether a given **const** object of Callable type can be invoked with a given CallType
         */
        template<class Callable, class CallType>
        constexpr bool IsConstInvocableWith = isInvocableWith<false, Callable, CallType>();
        
        /**
         Whether a given **non const** object of Callable type can be invoked with a given CallType
         */
        template<class Callable, class CallType>
        constexpr bool IsMutableInvocableWith = isInvocableWith<true, Callable, CallType>();
        
        template<bool IsMutable, class Callable, class Ret, class... Args>
        auto makeBlockHelper(Callable && callable, Ret (*)(Args...)) {
            using ObjectType = std::remove_cvref_t<Callable>;
            return BlockWithCallable<ObjectType, IsMutable, Ret, Args...>(std::forward<Callable>(callable));
        }
        
        /**
         * Wraps any C++ Callable in a block that invokes the const Callable
         *
         * @return A BlockWithCallable object
         *
         * This overload deduces the type of call to exposed by Callable automatically
         * but can only do so if the Callable has exactly one way to call it. If it has multiple
         * `operator()` overloads you will need to use `makeBlock<CallType>()` overload.
         *
         * Usage:
         * @code
         * someFunc(int (^)());
         *
         * someFunc(makeBlock([...]() { return 7; });
         * @endcode
         * Or
         * @code
         * struct callable {
         *    void operator()() const {}
         * };
         * someFunc(makeBlock(callable{}))
         * @endcode
        */
        template<class Callable>
        requires(BlockWrappable<std::remove_cvref_t<Callable>> &&
                 UniquelyConstCallable<std::remove_cvref_t<Callable>>)
        auto makeBlock(Callable && callable) {
            using CallType = DeduceCallType<false, std::remove_cvref_t<Callable>>;
            return makeBlockHelper<false>(std::forward<Callable>(callable), (CallType *)nullptr);
        }
        
        /**
         * Wraps any C++ Callable in a block that invokes the const Callable
         *
         * @return A BlockWithCallable object
         *
         * This overload requires you to specify the type of call to perform on the Callable.
         * Use it if Callable has multiple `operator()` overloads.
         *
         * Usage:
         * @code
         * someFunc(int (^)());
         * struct callable {
         *    void operator()() const {}
         *    void operator()(int) const {}
         * };
         * someFunc(makeBlock<void (int)>(callable{});
         * @endcode
        */
        template<class CallType, class Callable>
        requires(BlockWrappable<std::remove_cvref_t<Callable>> &&
                 IsConstInvocableWith<Callable, CallType>)
        auto makeBlock(Callable && callable) {
            return makeBlockHelper<false>(std::forward<Callable>(callable), (CallType *)nullptr);
        }
        
        /**
         * Wraps any C++ Callable in a block that invokes the NON-const Callable
         *
         * @return A BlockWithCallable object
         *
         * This overload deduces the type of call to exposed by Callable automatically
         * but can only do so if the Callable has exactly one way to call it. If it has multiple
         * `operator()` overloads you will need to use `makeMutableBlock<CallType>()` overload.
         *
         * Usage:
         * @code
         * someFunc(int (^)());
         *
         * someFunc(makeMutableBlock([...]() mutable { return 7; });
         * @endcode
         * Or
         * @code
         * struct callable {
         *    void operator()() {}
         * };
         * someFunc(makeMutableBlock(callable{}))
         * @endcode
        */
        template<class Callable>
        requires(BlockWrappable<std::remove_cvref_t<Callable>> &&
                 UniquelyMutableCallable<std::remove_cvref_t<Callable>>)
        auto makeMutableBlock(Callable && callable) {
            using CallType = DeduceCallType<true, std::remove_cvref_t<Callable>>;
            return makeBlockHelper<true>(std::forward<Callable>(callable), (CallType *) nullptr);
        }
        
        /**
         * Wraps any C++ Callable in a block that invokes the NON-const Callable
         *
         * @return A BlockWithCallable object
         *
         * This overload requires you to specify the type of call to perform on the Callable.
         * Use it if Callable has multiple `operator()` overloads.
         *
         * Usage:
         * @code
         * someFunc(int (^)());
         * struct callable {
         *    void operator()() {}
         *    void operator()(int) {}
         * };
         * someFunc(makeMutableBlock<void (int)>(callable{});
         * @endcode
        */
        template<class CallType, class Callable>
        requires(BlockWrappable<std::remove_cvref_t<Callable>>&&
                 IsMutableInvocableWith<Callable, CallType>)
        auto makeMutableBlock(Callable && callable) {
            return makeBlockHelper<true>(std::forward<Callable>(callable), (CallType *) nullptr);
        }

        #if __has_feature(objc_arc)
        
            /**
             Convert strong pointer to a weak pointer of the same type
             
             Usage:
             @code
             [foo doBar:BlockWrapper([weakSelf = makeWeak(self)] () {
                
             }];
             @endcode
            */
            template<class T>
            auto makeWeak(T * __strong obj) -> T * __weak
                { return obj; }
        
            /**
             Convert strong block pointer to a weak pointer of the same type
            */
            template<class R, class... Args>
            auto makeWeak(R (^ __strong obj)(Args...)) -> R (^ __weak)(Args...)
                { return obj; }

            /**
             Convert weak pointer to a strong pointer of the same type
             
             Usage:
             @code
             [foo doBar:BlockWrapper([weakSelf = makeWeak(self)] () {
                 auto self = makeStrong(self);
                 if (!self)
                     return;
             }];
            @endcode
            */
            template<class T>
            auto makeStrong(T * __weak obj) -> T * __strong
                { return obj; }
        
            /**
             Convert weak block pointer to a strong pointer of the same type
            */
            template<class R, class... Args>
            auto makeStrong(R (^ __weak obj)(Args...)) -> R (^ __strong)(Args...)
                { return obj; }

        #endif

    }
}

using BlockUtil::BlockWithCallable;
using BlockUtil::makeBlock;
using BlockUtil::makeMutableBlock;
#if __has_feature(objc_arc)
    using BlockUtil::makeWeak;
    using BlockUtil::makeStrong;
#endif




#endif
