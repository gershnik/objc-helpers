# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## Unreleased

### Added
- `BlockUtil.h` and `CoDispatch.h` are now supported on Linux

## [3.0] - 2024-01-17

### Changed
- `BlockUtil.h`: `makeBlock` functionality is completely reworked. New functionality: 
  * Wrap any callables including mutable lambdas or any other callable that provides non-const `operator()`.
  * If the callable is movable it will be moved into the block, not copied. It will also be moved if the block is "copied to heap" 
    by ObjectiveC runtime or `Block_copy` in plain C++.
  * It is possible to use move-only callables.
  * All of this is accomplished with NO dynamic memory allocation
- `NSStringCharAccess` now conforms completely to `std::ranges::random_access_range`
- `BoxUtil.h`: boxing now detects comparability and enables `compare:` not just via presence of operator `<=>` but also when only operators `<`, `==`, `<=` etc. are present.
- `BoxUtil.h`: generated ObjectiveC box classes now have names unique to each shared library/main executable, preventing collisions if multiple modules use boxing.

### Added
- `NSStringUtil.h`: added `makeNSString`, `makeCFString` and `makeStdString` conversion functions between C++ character ranges and ObjectiveC strings.

## [2.3] - 2024-01-09

### Added
- `BoxUtil.h` header for generic boxing and unboxing of any C++ object to/from an ObjectiveC one
- `XCTestUtil.h` header for XCTest macros to compare C++ objects while producing useful descriptions in case of failure.

## [2.2] - 2023-12-22

### Changed
- Performance optimizations for CoDispatch

## [2.1] - 2023-12-16

### Added
- all variants of `resumeOn` and `resumeOnMainQueue` now accept `when` argument to request that the resumption happens no earlier than the specified time. When used with `co_await resumeOn(currentQueue, when)` this allows the caller to sleep asynchronously without blocking its queue.

## [2.0] - 2023-12-13

### Added
- CoDispatch.h: A collection of classes and functions that allows you to write **asynchronous** C++ coroutines and generators that execute on GCD dispatch queues. Detailed guide is available [here](https://github.com/gershnik/objc-helpers/blob/v2.0/doc/CoDispatch.md)
- Ability to subtract `NSStringCharAccess::iterator`s
- Ability to get starting index of `NSStringCharAccess::iterator` inside its string
- Extensive unit tests for all functionality

### Fixed
- Crash in `NSNumberEqual` when second argument is `nullptr`
- `NSStringCharAccess::empty()` returning opposite result
- Compiler warnings in pedantic mode

## [1.1] - 2022-08-20

### Fixed

- Suppressing compiler warning on an unused argument

## [1.0] - 2021-06-16

### Added
- Initial version

[1.0]: https://github.com/gershnik/objc-helpers/releases/tag/v1.0
[1.1]: https://github.com/gershnik/objc-helpers/releases/tag/v1.1
[2.0]: https://github.com/gershnik/objc-helpers/releases/v2.0
[2.1]: https://github.com/gershnik/objc-helpers/releases/v2.1
[2.2]: https://github.com/gershnik/objc-helpers/releases/v2.2
[2.3]: https://github.com/gershnik/objc-helpers/releases/v2.3
[3.0]: https://github.com/gershnik/objc-helpers/releases/v3.0
