# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## Unreleased

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
[1.1]: https://github.com/gershnik/objc-helpers/releases/tag/v1.1[2.0]: https://github.com/gershnik/objc-helpers/releases/v2.0
