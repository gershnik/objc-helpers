/*
 Copyright 2020 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_CO_DISPATCH_INCLUDED
#define HEADER_CO_DISPATCH_INCLUDED

#ifndef __cplusplus
    #error This header requires C++
#endif

#include <version>

#if !__cpp_impl_coroutine || !__cpp_lib_coroutine
    #error This header requires C++20 mode or above with coroutine support
#endif

#include <coroutine>
#include <variant>
#include <memory>
#include <atomic>
#include <cassert>

#include <dispatch/dispatch.h>
#ifndef __OBJC__
    #include <Block.h>
#endif


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-extension"


/**
 A marker for various things whether they can carry exceptions in addition to regular payload
 */
enum class SupportsExceptions : bool {
    No,
    Yes
};

#ifdef __cpp_exceptions
    #define CO_DISPATCH_ADD_NS_SUFFIX(a) a
    #define CO_DISPATCH_DEFAULT_SE SupportsExceptions::Yes
#else
    #define CO_DISPATCH_ADD_NS_SUFFIX(a) a##Noexcept
    #define CO_DISPATCH_DEFAULT_SE SupportsExceptions::No
#endif

#define CO_DISPATCH_CONCAT1(a, b) a##b
#define CO_DISPATCH_CONCAT(a, b) CO_DISPATCH_CONCAT1(a, b)

#if OS_OBJECT_USE_OBJC
    #define CO_DISPATCH_NS CO_DISPATCH_ADD_NS_SUFFIX(CoDispatch)
#else
    #define CO_DISPATCH_NS CO_DISPATCH_ADD_NS_SUFFIX(CoDispatchCpp)
#endif

inline namespace CO_DISPATCH_NS {
    
    namespace Util {
        
        //MARK: - Intrusive reference counting
        
        /**
         We need to do some low level intrusive reference counting below
         This is a very simple intrusive refcounting smart pointer to avoid pulling some external library in
         */
        
        template<class T> struct Ref { T * __nullable ptr; };
        template<class T> struct Noref { T * __nullable ptr; };
        
        template<class T>
        class [[clang::trivial_abi]] RefcntPtr {
            template<class Y> friend class RefcntPtr;
        public:
            RefcntPtr(Ref<T> src) noexcept :
                m_ptr(src.ptr) {
                if (m_ptr)
                    m_ptr->addRef();
            }
            RefcntPtr(Noref<T> src) noexcept :
                m_ptr(src.ptr) {
            }
            RefcntPtr(const RefcntPtr & src) noexcept:
                m_ptr(src.m_ptr) {
                if (m_ptr)
                    m_ptr->addRef();
            }
            RefcntPtr(RefcntPtr && src) noexcept:
                m_ptr(std::exchange(src.m_ptr, nullptr))
            {}
            RefcntPtr & operator=(const RefcntPtr &) = delete;
            RefcntPtr & operator=(RefcntPtr &&) = delete;
            [[clang::always_inline]] ~RefcntPtr() noexcept {
                if (m_ptr)
                    m_ptr->subRef();
            }
            
            void reset() {
                if (m_ptr)
                    m_ptr->subRef();
                m_ptr = nullptr;
            }
            
            auto operator->() const noexcept -> T * __nullable
                { return m_ptr; }
            auto operator*() const noexcept -> T &
                { return *m_ptr; }
            auto get() const noexcept -> T * __nullable
                { return m_ptr; }
            operator bool() const noexcept
                { return m_ptr != nullptr; }
        private:
            T * __nullable m_ptr;
        };
        
        template<class T>
        auto ref(T * __nullable ptr) noexcept
            { return RefcntPtr{Ref<T>{ptr}}; }
        template<class T>
        auto noref(T * __nullable ptr) noexcept
            { return RefcntPtr{Noref<T>{ptr}}; }
        
        //MARK: - Dispatch object holders
        
        //We need to manually do dispatch object reference counting when compiling without ObjC ARC and storing them
        
#if OS_OBJECT_USE_OBJC
        template<class D>
        using DispatchHolder = D __nullable;
        
#else
        template<class D>
        class DispatchHolder {
        public:
            DispatchHolder() noexcept : m_object(nullptr)
            {}
            explicit DispatchHolder(D obj) noexcept : m_object(obj) {
                if (m_object)
                    dispatch_retain(m_object);
            }
            ~DispatchHolder() noexcept {
                if (m_object)
                    dispatch_release(m_object);
            }
            DispatchHolder(const DispatchHolder & src) noexcept: m_object(src.m_object) {
                if (m_object)
                    dispatch_retain(m_object);
            }
            DispatchHolder(DispatchHolder && src) noexcept: m_object(std::exchange(src.m_object, nullptr))
            {}
            auto operator=(const DispatchHolder & src) noexcept -> DispatchHolder & {
                *this = src.m_object;
                return *this;
            }
            auto operator=(DispatchHolder && src) noexcept -> DispatchHolder & {
                std::swap(m_object, src.m_object);
                return *this;
            }
            auto operator=(D obj) noexcept -> DispatchHolder & {
                auto old = std::exchange(m_object, obj);
                if (m_object)
                    dispatch_retain(m_object);
                if (old)
                    dispatch_release(old);
                return *this;
            }
            operator D() const noexcept
                { return m_object; }
        private:
            D m_object;
        };
#endif
        
        using QueueHolder = DispatchHolder<dispatch_queue_t>;
        using DataHolder = DispatchHolder<dispatch_data_t>;
        
        
        //MARK: - Moving values around
        
        /**
         We need a way to transfer return values from coroutines to clients.
         This is a generic "holder of value" that can handle anything that can be returned by a function.
         It can, potentially also store an exception depending on E template parameter.
         If the stored type is not `void` it also has a "not set" state used by generators to distinguish
         yield from return and coroutines to trap potential logic errors in our code.
         */
        template<class T, SupportsExceptions E>
        class ValueCarrier {
        public:
            static constexpr bool isVoid = std::is_void_v<T>;
            static constexpr bool supportsExceptions = (E == SupportsExceptions::Yes);
            using MoveInArgType = std::conditional_t<isVoid,
                                    void,
                                    std::conditional_t<std::is_reference_v<T>, T,
                                        std::conditional_t<std::is_const_v<T>,
                                                std::add_lvalue_reference_t<T> ,
                                                std::add_rvalue_reference_t<T>>
                                        >
                                  >;
            
        private:
            using Holder = std::conditional_t<isVoid, void,
                                std::conditional_t<std::is_reference_v<T>, std::remove_reference_t<T> *, std::remove_const_t<std::decay_t<T>>>
                           >;
            using Storage = std::conditional_t<isVoid,
                                std::conditional_t<supportsExceptions,
                                    std::exception_ptr,
                                    std::monostate>,
                                std::conditional_t<supportsExceptions,
                                    std::variant<std::monostate, Holder, std::exception_ptr>,
                                    std::variant<std::monostate, Holder>>
                            >;
            
        public:
            
            using ValueToken = std::conditional_t<isVoid, void, Holder *>;
            
            template<class... Args>
            static constexpr bool IsEmplaceableFrom =
                isVoid ?                 (sizeof...(Args) == 0) : (
                std::is_reference_v<T> ? (sizeof...(Args) == 1 && std::is_constructible_v<Holder, std::add_pointer_t<std::remove_reference_t<Args>>...>) : (
                /*else*/                 std::is_constructible_v<Holder, Args...>
            ));
            
            template<class... Args>
            static constexpr bool IsNoexceptEmplaceableFrom = IsEmplaceableFrom<Args...> && (
                isVoid ?                 true : (
                std::is_reference_v<T> ? true : (
                /*else*/                 std::is_nothrow_constructible_v<Holder, Args...>
            )));
            
            static constexpr bool IsNoExceptExtractable =
                isVoid ?                 true : (
                std::is_reference_v<T> ? true : (
                /*else*/                 (std::is_move_constructible_v<T> && std::is_nothrow_move_constructible_v<T>) || (!std::is_move_constructible_v<T> && std::is_nothrow_copy_constructible_v<T>)
                ));
        public:
            template<class... Args>
            requires(IsEmplaceableFrom<Args...>)
            void emplaceValue(Args && ...args) noexcept(IsNoexceptEmplaceableFrom<Args...>) {
                if constexpr (isVoid) {
                    //nothing
                } else if constexpr (std::is_reference_v<T>) {
                    this->m_storage = std::addressof(args...);
                } else if constexpr (noexcept(this->m_storage.template emplace<Holder>(std::forward<Args>(args)...))) {
                    this->m_storage.template emplace<Holder>(std::forward<Args>(args)...);
                } else {
                    this->m_storage.template emplace<Holder>(std::forward<Args>(args)...);
                }
            }
            
            void storeException(std::exception_ptr ptr) noexcept
            requires(ValueCarrier::supportsExceptions)
                { this->m_storage = ptr; }
            
            auto moveOut() noexcept(!supportsExceptions && IsNoExceptExtractable) -> T {
                if constexpr (!isVoid) {
                    Holder & holder = std::visit([] (auto & val) -> Holder & {
                        
                        using Stored = std::remove_cvref_t<decltype(val)>;
                        
                        if constexpr (std::is_same_v<Stored, Holder>) {
                            return val;
                        } else if constexpr (std::is_same_v<Stored, std::exception_ptr>) {
                            std::rethrow_exception(val);
                        } else {
                            std::terminate();
                        }
                        
                    }, this->m_storage);
                    
                    if constexpr (std::is_reference_v<T>)
                        return static_cast<T>(*holder);
                    else
                        return std::move(holder);
                    
                } else if constexpr (supportsExceptions) {
                    if (this->m_storage)
                        std::rethrow_exception(this->m_storage);
                } else {
                    //nothing
                }
            }
            
            void clear() noexcept {
                if constexpr (!isVoid) {
                    this->m_storage = std::monostate{};
                } else if constexpr (supportsExceptions) {
                    this->m_storage = nullptr;
                }
            }
            
            auto getValueToken() noexcept(!supportsExceptions) -> ValueCarrier::ValueToken
            requires(!ValueCarrier::isVoid) {
                return std::visit([] (auto & val) -> Holder * {
                    
                    using Stored = std::remove_cvref_t<decltype(val)>;
                    
                    if constexpr (std::is_same_v<Stored, Holder>) {
                        return &val;
                    } else if constexpr (std::is_same_v<Stored, std::exception_ptr>) {
                        std::rethrow_exception(val);
                    } else {
                        return nullptr;
                    }
                    
                }, this->m_storage);
            }
            
            template<class X=ValueCarrier>
            static auto moveOutValue(X::ValueToken token) noexcept(IsNoExceptExtractable) -> T
            requires(!ValueCarrier::isVoid && std::is_same_v<X, ValueCarrier>) {
                Holder & holder = *token;
                if constexpr (std::is_reference_v<T>)
                    return static_cast<T>(*holder);
                else
                    return std::move(holder);
            }
            
            
        private:
            [[no_unique_address]] Storage m_storage;
        };
        
        
        //MARK: - Basic Promise
        
        /**
         Base class for all coroutine promises in this library
         
         Provides services for both server and client sides of a coroutine and communication between them.
         This is the core class of this library. Everything else just wraps its usages. It implements a state
         machine that tracks client and server behavior to avoid locking.
        
         @tparam Derived the derived class for CRTP
         @tparam DelayedValue type of ValueCarrier to store asynchronous result
         */
        template<class Derived, class DelayedValue>
        class BasicPromise {
        public:
            //Lifecycle
            
            /**
             To be called from `await_ready`
             @returns Whether the result is already available
             */
            auto isReady() const noexcept -> bool {
                if (m_resumeQueue) {
                    if (m_when != DISPATCH_TIME_NOW)
                        return false;
                    m_awaiterOnResumeQueue = isCurrentQueue(m_resumeQueue);
                    if (!m_awaiterOnResumeQueue)
                        return false;
                }
                auto state = m_state.load(std::memory_order_acquire);
                assert(state == s_runningMarker || state == s_completedMarker);
                return state != s_runningMarker;
            }
            
            /**
             To be called from `await_resume`
             @returns whether the client should remain suspended
             */
            auto clientAwait(std::coroutine_handle<> h) noexcept -> bool {
                auto oldState = m_state.exchange(reinterpret_cast<uintptr_t>(h.address()), std::memory_order_acq_rel);
                assert(oldState != s_notStartedMarker && oldState != s_abandonedMarker);
                if (oldState != s_runningMarker) {
                    if (m_resumeQueue && !m_awaiterOnResumeQueue) {
                        resumeHandleAsync(h.address());
                        return true;
                    }
                    return false;
                }
                return true;
            }
            
            /**
             Resumes execution for generators or coroutines that start suspended
             */
            auto resumeExecution(dispatch_queue_t __nullable queue) noexcept {
                m_value.clear();
                m_awaiterOnResumeQueue = false;
                [[maybe_unused]] auto oldstate = m_state.exchange(s_runningMarker, std::memory_order_acq_rel);
                assert(oldstate != s_runningMarker && oldstate != s_abandonedMarker);
                auto myHandle = std::coroutine_handle<BasicPromise>::from_promise(*this);
                if (queue) {
                    dispatch_async_f(queue, myHandle.address(), [](void * addr) {
                        std::coroutine_handle<>::from_address(addr).resume();
                    });
                } else {
                    myHandle.resume();
                }
            }
            
            /**
             Indicates that the client is no longer using this object
             */
            void clientAbandon() noexcept {
                auto oldState = m_state.exchange(s_abandonedMarker, std::memory_order_acquire);
                assert(oldState != s_abandonedMarker);
                if (oldState != s_runningMarker)
                    static_cast<const Derived *>(this)->destroy();
            }
            
            /**
             Indicates that server has completed processing and is suspended
             */
            auto serverComplete() noexcept -> std::coroutine_handle<> {
                auto oldState = m_state.exchange(s_completedMarker, std::memory_order_acq_rel);
                assert(oldState != s_completedMarker && oldState != s_notStartedMarker);
                if (oldState == s_abandonedMarker) {
                    static_cast<const Derived *>(this)->destroy();
                } else if (oldState != s_runningMarker) {
                    if (!m_resumeQueue || isCurrentQueue(m_resumeQueue)) {
                        return std::coroutine_handle<>::from_address(reinterpret_cast<void *>(oldState));
                    } else {
                        resumeHandleAsync(reinterpret_cast<void *>(oldState));
                    }
                }
                return std::noop_coroutine();
            }
            
            //Client awaiter interface
            //These methods can only be called when server is known to be suspended
            
            /**
             Moves out stored value at once
             
             This is used for coroutines.
             @pre Value must have been set. If not the call will abort at runtime
             @return Stored value
             @throws Stored exception if stored instead of value
             */
            decltype(auto) moveOutValue() noexcept(noexcept(m_value.moveOut())) {
                return m_value.moveOut();
            }
            
            /**
             Get a token that allows later extraction of the value.
             
             This is used by generators and is only defined if DelayedValue doesn't store `void`
             @return A token if the value has been set or nullptr otherwise
             @throws Stored exception if stored instead of value
             */
            decltype(auto) getValueToken() noexcept(noexcept(m_value.getValueToken()))
            requires(!DelayedValue::isVoid)
                { return m_value.getValueToken(); }
            
            /**
             Moves out value from previously extracted token.
             @return Stored value
             */
            template<class X=DelayedValue>
            static decltype(auto) moveOutValue(typename X::ValueToken token) noexcept(noexcept(X::moveOutValue(token)))
            requires(!DelayedValue::isVoid && std::is_same_v<X, DelayedValue>)
                { return X::moveOutValue(token); }
            
            //Client awaitable interface
            //These methods can only be called by client _before_ it awaits
            
            /**
             Specify a queue on which resume client
             */
            void setResumeQueue(dispatch_queue_t __nullable queue, dispatch_time_t when) noexcept {
                m_resumeQueue = queue;
                m_when = when;
            }
            
            
            //Server interface
            //These methods can only be called when client is known to be suspended
            
            template<class... Args>
            requires(DelayedValue::template IsEmplaceableFrom<Args...>)
            void emplaceReturnValue(Args && ...args) noexcept(noexcept(m_value.emplaceValue(std::forward<Args>(args)...)))
                { m_value.emplaceValue(std::forward<Args>(args)...); }
            
            void storeException(std::exception_ptr ptr) noexcept
            requires(DelayedValue::supportsExceptions)
                { m_value.storeException(ptr); }
            
            /**
             Direct implementation of `unhandled_exception` which is common to all
             derived coroutine promise types
             */
            void unhandled_exception() noexcept {
                if constexpr (DelayedValue::supportsExceptions)
                    m_value.storeException(std::current_exception());
                else
                    std::terminate();
            }
            
        protected:
            BasicPromise() = default;
            BasicPromise(bool running) :
                m_state(running ? s_runningMarker : s_notStartedMarker)
            {}
            ~BasicPromise() noexcept = default;
            BasicPromise(BasicPromise &&) = delete;
            
        private:
            //This little trick allows us to detect if current queue is the same as the argument
            //Since Apple doesn't allow us to ask "what is the current queue" this appears to be
            //the only way to optimize unnecessary dispatches.
            //Setting the key is probably not cheap but likely much cheaper than suspending and
            //scheduling dispatch when none is needed.
            auto isCurrentQueue(dispatch_queue_t __nonnull queue) const {
                dispatch_queue_set_specific(queue, this, const_cast<BasicPromise *>(this), nullptr);
                bool ret = (dispatch_get_specific(this) == this);
                dispatch_queue_set_specific(queue, this, nullptr, nullptr);
                return ret;
            }
            
            void resumeHandleAsync(void * __nonnull handleAddr) {
                
                auto resumer = [](void * addr) {
                    std::coroutine_handle<>::from_address(addr).resume();
                };
                
                if (m_when == DISPATCH_TIME_NOW)
                    dispatch_async_f(m_resumeQueue, handleAddr, resumer);
                else
                    dispatch_after_f(m_when, m_resumeQueue, handleAddr, resumer);
            }
            
        private:
            static constexpr uintptr_t s_runningMarker = 0;
            static constexpr uintptr_t s_notStartedMarker = 1;
            static constexpr uintptr_t s_completedMarker = 2;
            static constexpr uintptr_t s_abandonedMarker = 3;
            
            std::atomic<uintptr_t> m_state = s_runningMarker;
            QueueHolder m_resumeQueue;
            dispatch_time_t m_when = DISPATCH_TIME_NOW;
            mutable bool m_awaiterOnResumeQueue = false;
            DelayedValue m_value;
        };
        
        template<class Promise>
        struct PromiseClientAbandoner {
            void operator()(Promise * __nonnull ptr) const noexcept
                { ptr->clientAbandon(); }
        };
        template<class Promise>
        using ClientAbandonPtr = std::unique_ptr<Promise, PromiseClientAbandoner<Promise>>;
        
        //MARK: - Common Awaiter
        
        template<class Promise>
        struct Awaiter {
            ClientAbandonPtr<Promise> promise;
            
            auto await_ready() const noexcept -> bool
                { return promise->isReady(); }
            auto await_suspend(std::coroutine_handle<> handle) noexcept -> bool
                { return promise->clientAwait(handle); }
        };
        
    }
    
    //MARK: - Async function calls
    
    /**
     Awaitable for asynchronous function calls
     */
    template<class T, SupportsExceptions E>
    class DispatchAwaitable {
    private:
        using DelayedValue = Util::ValueCarrier<T, E>;
        
        struct State : public Util::BasicPromise<State, DelayedValue> {
            void destroy() const noexcept
                { delete this; }
            virtual ~State()
                {}
            
            //Reference counting
            //Unfortunately Promise (below) that will carry state to the clients cannot be move-only
            //It will likely need to be passed to ObjC blocks that can only capture copyable objects (sigh!).
            //So we add reference counting here to the State even though it isn't otherwise needed and makes
            //it less safe.
            
            [[clang::always_inline]] void addRef() const noexcept {
                [[maybe_unused]] auto oldcount = m_refCount.fetch_add(1, std::memory_order_relaxed);
                assert(oldcount > 0);
                assert(oldcount < std::numeric_limits<decltype(oldcount)>::max());
            }
            [[clang::always_inline]] void subRef() const noexcept {
                auto oldcount = m_refCount.fetch_sub(1, std::memory_order_release);
                assert(oldcount > 0);
                if (oldcount == 1)
                {
                    std::atomic_thread_fence(std::memory_order_acquire);
                    //remember, refcounting is for server (Promise) only!
                    //it is safe to cast in this case because the object is non const to begin with
                    //and destruction is outside of const-ness purview
                    auto h = const_cast<State *>(this)->serverComplete();
                    h.resume();
                }
            }
            
            mutable std::atomic<unsigned> m_refCount = 1;
        };
        
        using ClientStatePtr = Util::ClientAbandonPtr<State>;
        using AwaiterBase = Util::Awaiter<State>;
        
        template<class Func>
        using FunctionFromReference = std::decay_t<std::remove_cvref_t<Func>>;
        
        template<class Func>
        struct StateForFunc : State {
            template<class Arg>
            requires(std::is_constructible_v<Func, Arg &&>)
            StateForFunc(Arg && arg) noexcept(std::is_nothrow_constructible_v<Func, Arg &&>):
                func(std::forward<Arg>(arg))
            {}
            
            template<class Arg>
            requires(std::is_constructible_v<Func, const Arg &>)
            StateForFunc(const Arg & arg) noexcept(std::is_nothrow_constructible_v<Func, const Arg &>) :
                func(arg)
            {}
            
            Func func;
        };
#ifndef __OBJC__
        template<class Ret>
        struct StateForFunc<Ret (^)()> : State {
            StateForFunc(Ret (^ __nonnull block)()) noexcept:
                func(Block_copy(block))
            {}
            ~StateForFunc() noexcept
                { Block_release(func); }
            StateForFunc(StateForFunc &&) = delete;
            StateForFunc(const StateForFunc &) = delete;
            
            Ret (^ __nonnull func)();
        };
#endif
    public:
        class Promise {
        public:
            Promise(State * __nonnull state):
                m_sharedState(noref(state))
            {}
            
            //These methods need to be const because promise captured in blocks will be const.
            //Which means m_sharedState needs to be mutable. Sigh.
            
            template<class... Args>
            requires(DelayedValue::template IsEmplaceableFrom<Args...>)
            void success(Args && ...args) const noexcept(noexcept(m_sharedState->emplaceReturnValue(std::forward<Args>(args)...))) 
                { m_sharedState->emplaceReturnValue(std::forward<Args>(args)...); }
            
            template<class X=DelayedValue>
            void success(typename X::MoveInArgType val) const noexcept(noexcept(m_sharedState->emplaceReturnValue(std::forward<decltype(val)>(val))))
            requires(!DelayedValue::isVoid && std::is_same_v<X, DelayedValue>)
                { m_sharedState->emplaceReturnValue(std::forward<decltype(val)>(val)); }
            
            void failure(std::exception_ptr ptr) const noexcept
            requires(DelayedValue::supportsExceptions)
                { m_sharedState->storeException(ptr); }
            
            template<class Exc>
            void failure(Exc && exc) const noexcept
            requires(DelayedValue::supportsExceptions)
                { m_sharedState->storeException(std::make_exception_ptr(std::forward<Exc>(exc))); }
        private:
            mutable Util::RefcntPtr<State> m_sharedState;
        };
    public:
        //You must use a temporary to co_await or do co_await std::move(...) on a stored awaitable
        void operator co_await() & = delete;
        void operator co_await() const & = delete;
        auto operator co_await() && noexcept  {
            struct awaiter : AwaiterBase {
                decltype(auto) await_resume() const noexcept(noexcept(AwaiterBase::promise->moveOutValue()))
                    { return AwaiterBase::promise->moveOutValue(); }
            };
            return awaiter{std::move(m_sharedState)};
        }
        
        auto resumeOn(dispatch_queue_t __nullable queue,
                      dispatch_time_t when = DISPATCH_TIME_NOW) && noexcept -> DispatchAwaitable && {
            m_sharedState->setResumeQueue(queue, when);
            return std::move(*this);
        }
        auto resumeOnMainQueue(dispatch_time_t when = DISPATCH_TIME_NOW) && noexcept -> DispatchAwaitable &&
            { return std::move(*this).resumeOn(dispatch_get_main_queue(), when); }
        
        template<class Func>
        requires(std::is_invocable_v<FunctionFromReference<Func>>)
        static auto invokeOnQueue(dispatch_queue_t __nonnull queue, Func && func) -> DispatchAwaitable {
            auto * state = new StateForFunc<FunctionFromReference<Func>>(std::forward<Func>(func));
            dispatch_async_f(queue, state, DispatchAwaitable::invokeFromState<Func>);
            return DispatchAwaitable(state);
        }
        
        template<class Func>
        requires(std::is_invocable_r_v<void, Func, Promise>)
        static auto invokeDirectly(Func && func) -> DispatchAwaitable {
            auto * state = new State;
            DispatchAwaitable ret(state);
#ifdef __cpp_exceptions
            try {
#endif
                std::forward<Func>(func)(Promise(state));

#ifdef __cpp_exceptions
            } catch (...) {
                state->unhandled_exception();
            }
#endif
            return ret;
        }
        
    private:
        DispatchAwaitable(State * __nonnull state) noexcept :
            m_sharedState(state)
        {}
        
        template<class Func>
        requires(std::is_invocable_v<FunctionFromReference<Func>>)
        static void invokeFromState(void * __nonnull ptr) noexcept {
            auto * state = static_cast<StateForFunc<FunctionFromReference<Func>> *>(ptr);
            Promise promise(state);
#ifdef __cpp_exceptions
            if constexpr (!DelayedValue::supportsExceptions) {
#endif
                invoke(state->func, promise);
#ifdef __cpp_exceptions
            } else {
                try {
                    invoke(state->func, promise);
                } catch (...) {
                    promise.failure(std::current_exception());
                }
            }
#endif
        }
        
        template<class Func>
        requires(std::is_invocable_v<Func>)
        static void invoke(Func && func, const Promise & promise) noexcept(!DelayedValue::supportsExceptions) {
            if constexpr (DelayedValue::isVoid) {
                std::forward<Func>(func)();
                promise.success();
            } else {
                promise.success(std::forward<Func>(func)());
            }
        }
    private:
        ClientStatePtr m_sharedState;
    };
    
    template<class Func, class... Args>
    using DispatchAwaitableFor = DispatchAwaitable<decltype(std::declval<Func>()(std::declval<Args>()...)),
#ifdef __cpp_exceptions
                                                   std::is_nothrow_invocable_v<Func, Args...> ?
                                                            SupportsExceptions::No :
                                                            SupportsExceptions::Yes
#else
                                                   SupportsExceptions::No
#endif
                                 >;
    
    
    
    /**
     @function
     Converts a call with an asynchronous callback to an awaitable call
     @param func a callable you supply that will eventually cause the promise passed to it to be fulfilled in some callback
     */
    template<class Ret, SupportsExceptions E, class Func>
    requires(std::is_invocable_r_v<void, Func, typename DispatchAwaitable<Ret, E>::Promise>)
    auto makeAwaitable(Func && func) {
        return DispatchAwaitable<Ret, E>::invokeDirectly(std::forward<Func>(func));
    }
    
    /**
     @function
     Converts a call with an asynchronous callback to an awaitable call
     
     This overload uses default value for SupportsExceptions setting. If exceptions are enabled in compilation it supports them
     otherwise no.
     @param func a callable you supply that will eventually cause the promise passed to it to be fulfilled in some callback
     */
    template<class Ret, class Func>
    requires(std::is_invocable_r_v<void, Func, typename DispatchAwaitable<Ret, CO_DISPATCH_DEFAULT_SE>::Promise>)
    auto makeAwaitable(Func && func) {
        return DispatchAwaitable<Ret, CO_DISPATCH_DEFAULT_SE>::invokeDirectly(std::forward<Func>(func));
    }
    
    
    /**
     @function
     Executes a callable on a queue and makes it awaitable from a coroutine
     */
    template<class Func>
    requires(std::is_invocable_v<Func>)
    auto co_dispatch(dispatch_queue_t __nonnull queue, Func && func) {
        return DispatchAwaitableFor<Func>::invokeOnQueue(queue, std::forward<Func>(func));
    }
    
    /**
     @function
     Executes a callable on the main queue and makes it awaitable from a coroutine
     */
    template<class Func>
    requires(std::is_invocable_v<Func>)
    auto co_dispatch(Func && func) {
        return co_dispatch(dispatch_get_main_queue(), std::forward<Func>(func));
    }
    
    //MARK: - Coroutine task
    
    /**
     Return type for coroutines
     */
    template<class Ret = void, SupportsExceptions E = CO_DISPATCH_DEFAULT_SE>
    class DispatchTask
    {
    private:
        using DelayedValue = Util::ValueCarrier<Ret, E>;
        
        struct Promise;
        using BasicPromise = Util::BasicPromise<Promise, DelayedValue>;
        
        template<bool IsVoid>
        struct PromiseBase : BasicPromise {
            template<class Arg>
            requires(DelayedValue::template IsEmplaceableFrom<Arg>)
            void return_value(Arg && arg) noexcept(noexcept(this->emplaceReturnValue(std::forward<Arg>(arg))))
                { this->emplaceReturnValue(std::forward<Arg>(arg)); }
        };
        
        template<>
        struct PromiseBase<true> : BasicPromise {
            void return_void() noexcept
                { this->emplaceReturnValue(); }
        };
        
        
        struct Promise : PromiseBase<std::same_as<Ret, void>> {
            auto get_return_object() noexcept -> DispatchTask
                { return {this}; }
            auto initial_suspend() noexcept -> std::suspend_never
                { return {}; }
            auto final_suspend() noexcept  {
                struct awaiter {
                    Promise & me;
                    constexpr bool await_ready() const noexcept { return false; }
                    auto await_suspend(std::coroutine_handle<> ) const noexcept
                        { return me.serverComplete(); }
                    constexpr void await_resume() const noexcept {}
                };
                return awaiter{*this};
            }
            
            void destroy() const noexcept {
                auto handle = std::coroutine_handle<Promise>::from_promise(const_cast<Promise &>(*this));
                handle.destroy();
            }
        };
        
        using AwaiterBase = Util::Awaiter<Promise>;
        
    public:
        using promise_type = Promise;
        
    public:
        DispatchTask(const DispatchTask &) = delete;
        DispatchTask(DispatchTask &&) noexcept = default;
        
        //You must use a temporary to co_await or do co_await std::move(...) on a stored task
        void operator co_await() & = delete;
        void operator co_await() const & = delete;
        auto operator co_await() && noexcept  {
            struct awaiter : AwaiterBase {
                decltype(auto) await_resume() const noexcept(noexcept(AwaiterBase::promise->moveOutValue()))
                    { return AwaiterBase::promise->moveOutValue(); }
            };
            return awaiter{std::move(m_promise)};
        }
        
        
        auto resumeOn(dispatch_queue_t __nullable queue, dispatch_time_t when = DISPATCH_TIME_NOW) && noexcept -> DispatchTask && {
            m_promise->setResumeQueue(queue, when);
            return std::move(*this);
        }
        auto resumeOnMainQueue(dispatch_time_t when = DISPATCH_TIME_NOW) && noexcept -> DispatchTask &&
            { return std::move(*this).resumeOn(dispatch_get_main_queue(), when); }
        
    private:
        DispatchTask(Promise * __nonnull promise) noexcept :
            m_promise(promise)
        {}
        
    private:
        Util::ClientAbandonPtr<Promise> m_promise;
    };
    
    //MARK: - Generator
    
    /**
     Return type for generators
     */
    template<class Ret, SupportsExceptions E = CO_DISPATCH_DEFAULT_SE>
    requires(!std::is_void_v<Ret>)
    class DispatchGenerator
    {
    private:
        using DelayedValue = Util::ValueCarrier<Ret, E>;
        
        struct Promise;
        using BasicPromise = Util::BasicPromise<Promise, DelayedValue>;
        
        struct Promise : BasicPromise {
            
            Promise() : BasicPromise(false)
            {}
            
            auto get_return_object() noexcept -> DispatchGenerator
                { return {this}; }
            auto initial_suspend() noexcept -> std::suspend_always
                { return {}; }
            auto final_suspend() noexcept  {
                struct awaiter {
                    Promise & me;
                    constexpr bool await_ready() const noexcept { return false; }
                    auto await_suspend(std::coroutine_handle<> ) const noexcept
                        { return me.serverComplete(); }
                    constexpr void await_resume() const noexcept {}
                };
                return awaiter{*this};
            }
            
            template<class Arg>
            requires(DelayedValue::template IsEmplaceableFrom<Arg>)
            auto yield_value(Arg && arg) noexcept(noexcept(this->emplaceReturnValue(std::forward<Arg>(arg))))  {
                this->emplaceReturnValue(std::forward<Arg>(arg));
                struct awaiter {
                    Promise & me;
                    constexpr bool await_ready() const noexcept { return false; }
                    auto await_suspend(std::coroutine_handle<> ) const noexcept
                        { return me.serverComplete(); }
                    constexpr void await_resume() const noexcept {}
                };
                return awaiter{*this};
            }
            
            void return_void() noexcept 
            {}
            
            void destroy() const noexcept {
                auto handle = std::coroutine_handle<Promise>::from_promise(const_cast<Promise &>(*this));
                handle.destroy();
            }
        };
        
        using AwaiterBase = Util::Awaiter<Promise>;
        
    public:
        using promise_type = Promise;
        
    public:
        DispatchGenerator(const DispatchGenerator &) = delete;
        DispatchGenerator(DispatchGenerator &&) noexcept = default;
        
        class Iterator {
            friend DispatchGenerator;
        private:
            struct FirstAwaitable {
                Util::ClientAbandonPtr<Promise> m_promise;
                Util::QueueHolder m_queue;
                
                void operator co_await() & = delete;
                void operator co_await() const & = delete;
                auto operator co_await() && noexcept  {
                    struct awaiter : AwaiterBase {
                        Util::QueueHolder queue;
                        auto await_resume() noexcept(noexcept(AwaiterBase::promise->getValueToken())) -> Iterator {
                            return Iterator(std::move(AwaiterBase::promise), queue, AwaiterBase::promise->getValueToken());
                        }
                    };
                    return awaiter{{std::move(m_promise)}, Util::QueueHolder{m_queue}};
                };
            };
            struct NextAwaitable {
                Util::ClientAbandonPtr<Promise> promise;
                Iterator * __nonnull it;
                
                void operator co_await() & = delete;
                void operator co_await() const & = delete;
                auto operator co_await() && noexcept  {
                    struct awaiter : AwaiterBase {
                        Iterator * __nonnull it;
                        void await_resume() noexcept(noexcept(it->m_promise->getValueToken())) {
                            it->m_promise = std::move(AwaiterBase::promise);
                            it->m_valueToken = it->m_promise->getValueToken();
                        }
                    };
                    return awaiter{{std::move(promise)}, it};
                }
            };
        public:
            Iterator(Iterator && src) noexcept = default;
            
            decltype(auto) operator*() const noexcept(noexcept(Promise::moveOutValue(m_valueToken)))
                { return Promise::moveOutValue(m_valueToken); }
            auto next() noexcept {
                m_valueToken = nullptr;
                m_promise->resumeExecution(m_queue);
                return NextAwaitable{{std::move(m_promise)}, this};
            }
            operator bool() const noexcept {
                return m_valueToken;
            }
        private:
            Iterator(Util::ClientAbandonPtr<Promise> && promise, dispatch_queue_t __nullable queue, DelayedValue::ValueToken valueToken):
                m_promise(std::move(promise)),
                m_queue(queue),
                m_valueToken(valueToken)
            {}
        private:
            Util::ClientAbandonPtr<Promise> m_promise;
            Util::QueueHolder m_queue;
            DelayedValue::ValueToken m_valueToken;
        };
        
        auto beginOn(dispatch_queue_t __nullable queue) && noexcept {
            m_promise->resumeExecution(queue);
            return typename Iterator::FirstAwaitable{{std::move(m_promise)}, Util::QueueHolder{queue}};
        }
        
        auto begin() && noexcept
            { return std::move(*this).beginOn(dispatch_get_main_queue()); }
        
        auto beginSync() && noexcept
            { return std::move(*this).beginOn(nullptr); }
        
        auto resumingOn(dispatch_queue_t __nullable queue) && noexcept -> DispatchGenerator && {
            m_promise->setResumeQueue(queue, DISPATCH_TIME_NOW);
            return std::move(*this);
        }
        auto resumingOnMainQueue() && noexcept -> DispatchGenerator &&
            { return std::move(*this).resumingOn(dispatch_get_main_queue()); }
    
    private:
        DispatchGenerator(Promise * __nonnull promise) noexcept :
            m_promise(promise)
        {}
        
    private:
        Util::ClientAbandonPtr<Promise> m_promise;
    };
    
    //MARK: - Switching queues
    
    /**
     @function
     `co_await`ing this will resume the coroutine on a given queue optionally on or after a given time
     
     If you pass your current queue and non default `when` this is equivalent to an asynchronous sleep
     until `when` - now.
     */
    inline auto resumeOn(dispatch_queue_t __nonnull queue, dispatch_time_t when = DISPATCH_TIME_NOW) noexcept {
        struct Awaitable
        {
            dispatch_queue_t queue;
            dispatch_time_t when;
            auto await_ready() noexcept
                { return false; }
            auto await_suspend(std::coroutine_handle<> h) noexcept {
                if (when == DISPATCH_TIME_NOW)
                    dispatch_async_f(queue, h.address(), Awaitable::resume);
                else
                    dispatch_after_f(when, queue, h.address(), Awaitable::resume);
                return std::noop_coroutine();
            }
            void await_resume() noexcept
                {}
            
            static void resume(void * addr) noexcept {
                auto h = std::coroutine_handle<>::from_address(addr);
                h.resume();
            }
        };
        return Awaitable{queue, when};
    }
    
    /**
     @function
     `co_await`ing this will resume a coroutine on the main queue optionally on or after a given time
     
     If you are already on the main queue and pass non default  `when` this is equivalent to an asynchronous sleep
     until `when` - now.
     */
    inline auto resumeOnMainQueue(dispatch_time_t when = DISPATCH_TIME_NOW) noexcept {
        return resumeOn(dispatch_get_main_queue(), when);
    }
    
    //MARK: - Dispatch IO wrappers
    
    /**
     Result returned from all Dispatch IO operations
     */
    struct DispatchIOResult {
        DispatchIOResult() noexcept = default;
        
        DispatchIOResult(dispatch_data_t __nullable data, int error) noexcept:
            m_data(data),
            m_error(error)
        {}
        
        /**
         Dispatch data associated with the result.
         
         For reads this is the data read. For writes the data that could not be written.
         Can be nullptr
         */
        auto data() const noexcept -> dispatch_data_t __nullable
            { return m_data; }
        /**
         This value is 0 if the data was read/written successfully. If an error occurred, it contains the error number.
         */
        auto error() const noexcept -> int
            { return m_error; }
    private:
        Util::DataHolder m_data;
        int m_error = 0;
    };
    
    /**
     @function 
     Coroutine version of `dispatch_io_read`
     
     Unlike `dispatch_io_read` the progressHandler parameter is optional and is only needed if you
     want to monitor the operation progress. The final result is returned as coroutine result.
     @return DispatchIOResult object with operation result
     */
    inline auto co_dispatch_io_read(dispatch_io_t __nonnull channel, off_t offset, size_t length, dispatch_queue_t __nonnull queue, dispatch_io_handler_t __nullable progressHandler = nullptr) {
        return makeAwaitable<DispatchIOResult, SupportsExceptions::No>([channel, offset, length, queue, progressHandler](auto promise) {
            dispatch_io_read(channel, offset, length, queue, ^ (bool done, dispatch_data_t data, int error){
                if (progressHandler)
                    progressHandler(done, data, error);
                if (done)
                    promise.success(data, error);
            });
        });
    }
    
    /**
     @function 
     Coroutine version of `dispatch_read`
     
     @return DispatchIOResult object with operation result
     */
    inline auto co_dispatch_read(dispatch_fd_t fd, size_t length, dispatch_queue_t __nonnull queue) {
        return makeAwaitable<DispatchIOResult, SupportsExceptions::No>([fd, length, queue](auto promise) {
            dispatch_read(fd, length, queue, ^ (dispatch_data_t data, int error) {
                promise.success(data, error);
            });
        });
    }
    
    /**
     @function 
     Coroutine version of `dispatch_io_write`
     
     Unlike `dispatch_io_write` the progressHandler parameter is optional and is only needed if you
     want to monitor the operation progress. The final result is returned as coroutine result.
     
     @return DispatchIOResult object with operation result
     */
    inline auto co_dispatch_io_write(dispatch_io_t __nonnull channel, off_t offset, dispatch_data_t __nonnull data, dispatch_queue_t __nonnull queue, dispatch_io_handler_t __nullable progressHandler = nullptr) {
        return makeAwaitable<DispatchIOResult, SupportsExceptions::No>([channel, offset, data, queue, progressHandler](auto promise) {
            dispatch_io_write(channel, offset, data, queue, ^ (bool done, dispatch_data_t dataRemaining, int error){
                if (progressHandler)
                    progressHandler(done, data, error);
                if (done)
                    promise.success(dataRemaining, error);
            });
        });
    }
    
    /**
     @function 
     Coroutine version of `dispatch_write`
     
     @return DispatchIOResult object with operation result
     */
    inline auto co_dispatch_write(dispatch_fd_t fd, dispatch_data_t __nonnull data, dispatch_queue_t __nonnull queue) {
        return makeAwaitable<DispatchIOResult, SupportsExceptions::No>([fd, data, queue](auto promise) {
            dispatch_write(fd, data, queue, ^ (dispatch_data_t dataRemaining, int error) {
                promise.success(dataRemaining, error);
            });
        });
    }
    
}

#pragma clang diagnostic pop


#endif
