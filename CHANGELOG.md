# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## Unreleased

### Changed
- `BlockUtil.h`: `makeBlock` is deprecated in ObjectiveC++. Modern versions of Clang allow conversions from lambdas to block directly doing essentially what `makeBlock` was doing. Note that it is still available and necessary in C++.
- `BoxUtil.h`: boxing now detects comparability and enables `compare:` not just via presence of operator `<=>` but also when only operators `<`, `==`, `<=` etc. are present.
- `BoxUtil.h`: generated ObjectiveC box classes now have names unique to each shared library/main executable, preventing collisions if multiple modules use boxing.

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
