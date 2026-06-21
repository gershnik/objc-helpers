// swift-tools-version: 5.9
import PackageDescription

// objc-helpers is a header-only C++ / Objective-C++ library.
//
// The target is rooted at the package root (`path: "."`) 
// SwiftPM requires `publicHeadersPath` to
// live inside the target's directory, hence rooting at ".".
//
// A single (near-)empty translation unit is compiled because SwiftPM requires
// every target to have at least one compilable source, even when the library
// is header-only.

let package = Package(
    name: "ObjCHelpers",
    platforms: [
        .macOS("10.13"),
        .iOS("10.3"),
        .tvOS("10.2"),
        .watchOS("3.2"),
        .visionOS(.v1),
    ],
    products: [
        .library(
            name: "ObjCHelpers",
            targets: ["ObjCHelpers"]
        ),
    ],
    targets: [
        .target(
            name: "ObjCHelpers",
            path: ".",
            // Everything that is NOT the public headers or the dummy source.
            // Hidden entries (.git, .github, .vscode, .gitignore) are ignored
            // by SwiftPM automatically and need not be listed.
            exclude: [
                "sample",
                "test",
                "doc",
                "README.md",
                "CHANGELOG.md",
                "LICENSE",
                "VERSION",
                "CMakeLists.txt"
            ],
            // The only compiled translation unit.
            sources: [
                "stub/empty.cpp",
            ],
            // Existing top-level include dir; exposes <objc-helpers/Foo.h> to
            // consumers. A hand-written module.modulemap lives here too.
            publicHeadersPath: "include",
            linkerSettings: [
                // The NS* helpers call Foundation; ensure consumers link it.
                // (dispatch/blocks live in libSystem on Apple platforms, so no
                // extra linkage is needed for CoDispatch / BlockUtil.)
                .linkedFramework("Foundation"),
            ]
        ),
    ],
    // Matches the standard the headers require (they #error below C++20).
    // This sets the standard for THIS package's sources only; consumers must
    // still build their own targets in C++20 mode.
    cxxLanguageStandard: .gnucxx20
)
