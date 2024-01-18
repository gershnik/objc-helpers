#include <objc-helpers/XCTestUtil.h>

#include "doctest.h"

using namespace std::literals;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"

struct Failure {
    std::string filePath;
    NSUInteger lineNumber;
    bool expected;
    std::string condition;
    std::string message;
};

static std::vector<Failure> failures;


void _XCTFailureHandler(XCTestCase * _Nullable test, BOOL expected, const char *filePath, NSUInteger lineNumber, NSString *condition, NSString * _Nullable format, ...) {
    
    CHECK((__bridge void *)test == nullptr);
    va_list vl;
    va_start(vl, format);
    auto message = [[NSString alloc] initWithFormat:format arguments:vl];
    failures.push_back({filePath, lineNumber, expected != 0, condition.UTF8String, message.UTF8String});
    va_end(vl);
}

#define CHECK_NO_FAILURES() CHECK(failures.empty());

#define CHECK_FAILURE(line, exp, cond, mes) REQUIRE(failures.size() == 1); \
    CHECK(failures.back().filePath == __FILE__); \
    CHECK(failures.back().lineNumber == (line)); \
    CHECK(failures.back().expected == (exp)); \
    CHECK(failures.back().condition == (cond)); \
    CHECK(failures.back().message == (mes));


TEST_SUITE_BEGIN( "TestUtilTests" );


TEST_CASE( "integer" ) {
    
    //Equal
    failures.clear();
    XCTAssertCppEqual(1, 1);
    CHECK_NO_FAILURES();
    
    failures.clear();
    XCTAssertCppEqual(1, 2);
    CHECK_FAILURE(__LINE__ - 1, true, "((1) equal to (2)) failed: (\"1\") is not equal to (\"2\")", "");
    
    failures.clear();
    XCTAssertCppEqual(1, 2, "hello %d", 1);
    CHECK_FAILURE(__LINE__ - 1, true, "((1) equal to (2)) failed: (\"1\") is not equal to (\"2\")", "hello 1");
    
    //Not Equal
    failures.clear();
    XCTAssertCppNotEqual(1, 2);
    CHECK_NO_FAILURES();
    
    failures.clear();
    XCTAssertCppNotEqual(1, 1);
    CHECK_FAILURE(__LINE__ - 1, true, "((1) not equal to (1)) failed: (\"1\") is equal to (\"1\")", "");
    
    failures.clear();
    XCTAssertCppNotEqual(1, 1, "hello %d", 1);
    CHECK_FAILURE(__LINE__ - 1, true, "((1) not equal to (1)) failed: (\"1\") is equal to (\"1\")", "hello 1");
    
    //Greater Than
    failures.clear();
    XCTAssertCppGreaterThan(2, 1);
    CHECK_NO_FAILURES();
    
    failures.clear();
    XCTAssertCppGreaterThan(1, 2);
    CHECK_FAILURE(__LINE__ - 1, true, "((1) greater than (2)) failed: (\"1\") is not greater than (\"2\")", "");
    
    failures.clear();
    XCTAssertCppGreaterThan(2, 2);
    CHECK_FAILURE(__LINE__ - 1, true, "((2) greater than (2)) failed: (\"2\") is not greater than (\"2\")", "");
    
    failures.clear();
    XCTAssertCppGreaterThan(1, 2, "hello %d", 1);
    CHECK_FAILURE(__LINE__ - 1, true, "((1) greater than (2)) failed: (\"1\") is not greater than (\"2\")", "hello 1");
    
    //Greater Than or Equal
    failures.clear();
    XCTAssertCppGreaterThanOrEqual(2, 1);
    CHECK_NO_FAILURES();
    
    failures.clear();
    XCTAssertCppGreaterThanOrEqual(2, 2);
    CHECK_NO_FAILURES();
    
    failures.clear();
    XCTAssertCppGreaterThanOrEqual(1, 2);
    CHECK_FAILURE(__LINE__ - 1, true, "((1) greater than or equal to (2)) failed: (\"1\") is less than (\"2\")", "");
    
    failures.clear();
    XCTAssertCppGreaterThanOrEqual(1, 2, "hello %d", 1);
    CHECK_FAILURE(__LINE__ - 1, true, "((1) greater than or equal to (2)) failed: (\"1\") is less than (\"2\")", "hello 1");
    
    //Less than
    failures.clear();
    XCTAssertCppLessThan(4, 5);
    CHECK_NO_FAILURES();
    
    failures.clear();
    XCTAssertCppLessThan(5, 4);
    CHECK_FAILURE(__LINE__ - 1, true, "((5) less than (4)) failed: (\"5\") is not less than (\"4\")", "");
    
    failures.clear();
    XCTAssertCppLessThan(5, 5);
    CHECK_FAILURE(__LINE__ - 1, true, "((5) less than (5)) failed: (\"5\") is not less than (\"5\")", "");
    
    failures.clear();
    XCTAssertCppLessThan(5, 4, "hello %d", 1);
    CHECK_FAILURE(__LINE__ - 1, true, "((5) less than (4)) failed: (\"5\") is not less than (\"4\")", "hello 1");
    
    //Less Than or Equal
    failures.clear();
    XCTAssertCppLessThanOrEqual(4, 5);
    CHECK_NO_FAILURES();
    
    failures.clear();
    XCTAssertCppLessThanOrEqual(4, 4);
    CHECK_NO_FAILURES();
    
    failures.clear();
    XCTAssertCppLessThanOrEqual(5, 4);
    CHECK_FAILURE(__LINE__ - 1, true, "((5) less than or equal to (4)) failed: (\"5\") is greater than (\"4\")", "");
    
    failures.clear();
    XCTAssertCppLessThanOrEqual(5, 4, "hello %d", 1);
    CHECK_FAILURE(__LINE__ - 1, true, "((5) less than or equal to (4)) failed: (\"5\") is greater than (\"4\")", "hello 1");
}

TEST_CASE( "string" ) {
    
    failures.clear();
    XCTAssertCppEqual("a"s, "b"s);
    CHECK_FAILURE(__LINE__ - 1, true, "((\"a\"s) equal to (\"b\"s)) failed: (\"a\") is not equal to (\"b\")", "");
    
}

namespace {
    struct foo {
        int i;
        
        friend bool operator==(foo, foo) = default;
        
        friend NSString * testDescription(foo f) { return [NSString stringWithFormat:@"%d", f.i]; }
    };
}

TEST_CASE( "TestDescriptable" ) {
    failures.clear();
    XCTAssertCppEqual(foo{1}, foo{2});
    CHECK_FAILURE(__LINE__ - 1, true, "((foo{1}) equal to (foo{2})) failed: (\"1\") is not equal to (\"2\")", "");
}

namespace {
    struct bar {
        int i;
        
        friend bool operator==(bar, bar) = default;
    };
}

TEST_CASE( "Generic" ) {
    failures.clear();
    XCTAssertCppEqual(bar{1}, bar{2});
    CHECK_FAILURE(__LINE__ - 1, true, "((bar{1}) equal to (bar{2})) failed: (\"(anonymous namespace)::bar {\n  int i = 1\n}\n\") is not equal to (\"(anonymous namespace)::bar {\n  int i = 2\n}\n\")", "");
}

namespace {
    struct baz {
        int i;
        
        friend bool operator==(baz, baz) {
            throw std::runtime_error("ha");
        }
    };
}

TEST_CASE( "Exception" ) {
    failures.clear();
    XCTAssertCppEqual(baz{1}, baz{2});
    CHECK_FAILURE(__LINE__ - 1, false, "((baz{1}) equal to (baz{2})) failed: throwing \"std::runtime_error: ha\"", "");
}

TEST_SUITE_END();

#pragma clang diagnostic pop
