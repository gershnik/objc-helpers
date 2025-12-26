/*
 Copyright 2023 Eugene Gershnik

 Use of this source code is governed by a BSD-style
 license that can be found in the LICENSE file or at
 https://github.com/gershnik/objc-helpers/blob/main/LICENSE
*/

#ifndef HEADER_XCTEST_UTIL_INCLUDED
#define HEADER_XCTEST_UTIL_INCLUDED

#if !__cpp_concepts
    #error This header requires C++20 mode or above with concepts support
#endif

#if !__has_feature(cxx_exceptions)
    #error This header requires C++ exceptions to be enabled
#endif

#ifdef __OBJC__

#import <XCTest/XCTest.h>

#include <sstream>
#include <cxxabi.h>
#include <exception>

namespace TestUtil {
    using std::to_string;
    
    template<class T>
    concept TestDescriptable = requires(const T & obj) {
        { testDescription(obj) } -> std::same_as<NSString *>;
    };
    
    
    template<class T>
    concept ToStringDescriptable = requires(T obj) {
        { to_string(obj) } -> std::same_as<std::string>;
    };
    
    
    template<class T>
    concept OStreamDescriptable = requires(T obj, std::ostream & str) {
        { str << obj };
    };
    
    inline auto demangle(const char * name) -> std::string {

        int status = 0;
        std::unique_ptr<char, void(*)(void*)> res {
            abi::__cxa_demangle(name, nullptr, nullptr, &status),
            std::free
        };
        return (status==0) ? res.get() : name ;
    }
    
    template<class T>
    auto describeForTest(const T & val) {
        if constexpr (TestDescriptable<T>) {
            return testDescription(val);
        }
        else if constexpr (ToStringDescriptable<T>) {
            using std::to_string;
            auto str = to_string(val);
            return @(str.c_str());
        } else if constexpr (OStreamDescriptable<T>) {
            std::ostringstream str;
            str << val;
            return @(str.str().c_str());
        } else {
            return [NSString stringWithFormat:@"%s object", demangle(typeid(T).name()).c_str()];
        }
    }

    inline NSString * getCurrentExceptionReason() {
        std::exception_ptr ex = std::current_exception();
        try {
            std::rethrow_exception(ex);
        } catch (NSException * nex) {
            return nex.reason;
        } catch (const std::exception & cex) {
            return [NSString stringWithFormat:@"%s: %s", demangle(typeid(cex).name()).c_str(), cex.what()];
        } catch(...) {
            return @"unknown exception type";
        }
    }
}

#if defined(_XCT_TRY) && defined(_XCT_CATCH) && defined(_XCT_THROW)
    #define OBJCH_XCT_TRY _XCT_TRY
    #define OBJCH_XCT_CATCH(T) _XCT_CATCH(T)
    #define OBJCH_XCT_THROW _XCT_THROW
#else
    #define OBJCH_XCT_TRY try
    #define OBJCH_XCT_CATCH(T) catch(T)
    #define OBJCH_XCT_THROW throw
#endif

#ifdef _XCTGetCurrentExceptionReason
    #define objchGetCurrentExceptionReason _XCTGetCurrentExceptionReason
#else
    #define objchGetCurrentExceptionReason TestUtil::getCurrentExceptionReason
#endif

#define XCTPrimitiveAssertCpp(test, op, type, expression1, expressionStr1, expression2, expressionStr2, ...) \
({ \
    OBJCH_XCT_TRY { \
        __typeof__(expression1) expressionValue1 = (expression1); \
        __typeof__(expression2) expressionValue2 = (expression2); \
        if (expressionValue1 op expressionValue2) { \
            _XCTRegisterFailure(test, _XCTFailureDescription((type), 0, expressionStr1, expressionStr2, TestUtil::describeForTest(expressionValue1), TestUtil::describeForTest(expressionValue2)), __VA_ARGS__); \
        } \
    } \
    OBJCH_XCT_CATCH (_XCTestCaseInterruptionException *interruption) { [interruption raise]; } \
    OBJCH_XCT_CATCH (...) { \
        NSString *_xct_reason = objchGetCurrentExceptionReason(); \
        _XCTRegisterUnexpectedFailure(test, _XCTFailureDescription((type), 1, expressionStr1, expressionStr2, _xct_reason), __VA_ARGS__); \
    } \
})

#define XCTAssertCppEqual(expression1, expression2, ...) \
    XCTPrimitiveAssertCpp(nil, !=, _XCTAssertion_Equal, expression1, @#expression1, expression2, @#expression2, __VA_ARGS__)

#define XCTAssertCppNotEqual(expression1, expression2, ...) \
    XCTPrimitiveAssertCpp(nil, ==, _XCTAssertion_NotEqual, expression1, @#expression1, expression2, @#expression2, __VA_ARGS__)

#define XCTAssertCppGreaterThan(expression1, expression2, ...) \
    XCTPrimitiveAssertCpp(nil, <=, _XCTAssertion_GreaterThan, expression1, @#expression1, expression2, @#expression2, __VA_ARGS__)

#define XCTAssertCppGreaterThanOrEqual(expression1, expression2, ...) \
    XCTPrimitiveAssertCpp(nil, <, _XCTAssertion_GreaterThanOrEqual, expression1, @#expression1, expression2, @#expression2, __VA_ARGS__)

#define XCTAssertCppLessThan(expression1, expression2, ...) \
    XCTPrimitiveAssertCpp(nil, >=, _XCTAssertion_LessThan, expression1, @#expression1, expression2, @#expression2, __VA_ARGS__)

#define XCTAssertCppLessThanOrEqual(expression1, expression2, ...) \
    XCTPrimitiveAssertCpp(nil, >, _XCTAssertion_LessThanOrEqual, expression1, @#expression1, expression2, @#expression2, __VA_ARGS__)


#endif //__OBJC__

#endif

