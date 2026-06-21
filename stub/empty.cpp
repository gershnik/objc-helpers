//
// objc-helpers is a header-only library, but both SwiftPM targets and the
// static-library slices of an XCFramework require at least one compiled
// translation unit. This file exists solely to satisfy that requirement.
//
// It deliberately defines one tiny, inert symbol. A truly empty archive is
// rejected by some versions of `xcodebuild -create-xcframework`; a single
// unused byte of data avoids that edge case while linking cleanly into any
// consumer.
//

namespace objc_helpers_detail {
    extern const char build_marker;
    const char build_marker = 0;
}
