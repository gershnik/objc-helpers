#! /usr/bin/env -S python3 -u

#
# Build ObjCHelpers.xcframework for a header-only library.
#
# Because the library is header-only, each platform slice is just an (almost)
# empty static library; the value of the XCFramework is the bundled headers
# (include/objc-helpers/*.h) plus the module map (include/module.modulemap),
# which `-headers` carries along.
#
# This script is part of the internal release tooling (it lives under tools/
# and is invoked by CI); the produced .xcframework is published as a release
# artifact.
#
# Usage:
#   tools/make-xcframework.py [DEST]
#
# DEST is the directory the .xcframework is written into (default: repo root).
#

import argparse
import shutil
import subprocess
import tempfile

from pathlib import Path

MYPATH = Path(__file__).parent
ROOT = MYPATH.parent

NAME = "ObjCHelpers"

SRC = ROOT / "stub" / "empty.cpp"
HEADERS = ROOT / "include"
VERSION_FILE = ROOT / "VERSION"
CXX_STD = "gnu++20"

# (slice name, sdk, min-version flag, [archs])
SLICES = [
    ("macos",       "macosx",           "-mmacosx-version-min=10.13",           ["arm64", "x86_64"]),
    ("ios",         "iphoneos",         "-miphoneos-version-min=10.3",          ["arm64", "armv7", "armv7s"]),
    ("iossim",      "iphonesimulator",  "-mios-simulator-version-min=10.3",     ["arm64", "x86_64"]),
    ("tvos",        "appletvos",        "-mtvos-version-min=10.2",              ["arm64"]),
    ("tvossim",     "appletvsimulator", "-mtvos-simulator-version-min=10.2",    ["arm64", "x86_64"]),
    ("watchos",     "watchos",          "-mwatchos-version-min=3.2",            ["arm64", "arm64_32", "armv7k"]),
    ("watchossim",  "watchsimulator",   "-mwatchos-simulator-version-min=3.2",  ["arm64", "x86_64"]),
]


def run(args):
    print("+", " ".join(str(a) for a in args))
    subprocess.run(args, check=True)


def capture(args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout.strip()


def build_slice(build_dir, slice_name, sdk, min_flag, archs):
    sdk_path = capture(["xcrun", "--sdk", sdk, "--show-sdk-path"])

    arch_libs = []
    for arch in archs:
        obj = build_dir / f"{slice_name}-{arch}.o"
        lib = build_dir / f"{slice_name}-{arch}.a"
        run([
            "xcrun", "--sdk", sdk, "clang++",
            f"-std={CXX_STD}",
            "-arch", arch,
            "-isysroot", sdk_path,
            min_flag,
            "-Os",
            "-c", SRC,
            "-o", obj,
        ])
        run(["xcrun", "--sdk", sdk, "ar", "-crs", lib, obj])
        arch_libs.append(lib)

    slice_lib = build_dir / f"lib{NAME}-{slice_name}.a"
    if len(arch_libs) > 1:
        run(["xcrun", "lipo", "-create", *arch_libs, "-output", slice_lib])
    else:
        shutil.copy(arch_libs[0], slice_lib)
    return slice_lib

def dedup_headers(output):
    # One shared copy of the headers at the bundle root; each slice's Headers/
    # becomes a relative symlink to it. The per-slice HeadersPath in Info.plist
    # stays "Headers" and resolves through the link, so the plist is untouched.
    shared = output / "Headers"
    for slice_dir in sorted(p for p in output.iterdir() if p.is_dir()):
        hdr = slice_dir / "Headers"
        if not hdr.exists():
            continue
        if not shared.exists():
            shutil.move(str(hdr), str(shared))   # first slice donates the real copy
        else:
            shutil.rmtree(hdr)
        hdr.symlink_to(Path("..") / "Headers")    # relative, so the bundle stays movable

def parse_args():
    parser = argparse.ArgumentParser(
        description=f"Build {NAME}.xcframework from the header-only library."
    )
    parser.add_argument(
        "dest",
        nargs="?",
        type=Path,
        default=ROOT,
        help="directory the .xcframework is written into (default: repository root)",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    dest = args.dest.resolve()
    dest.mkdir(parents=True, exist_ok=True)
    output = dest / f"{NAME}.xcframework"

    # create-xcframework refuses to overwrite an existing output.
    if output.exists():
        shutil.rmtree(output)

    version = VERSION_FILE.read_text().strip()
    if not version:
        raise SystemExit(f"VERSION file is empty: {VERSION_FILE}")

    # Intermediate objects / per-slice archives live in a temp dir that is
    # removed automatically on exit; only the final .xcframework persists.
    with tempfile.TemporaryDirectory(prefix=f"{NAME}-xcframework-") as tmp:
        build_dir = Path(tmp)

        slice_libs = [build_slice(build_dir, *slice) for slice in SLICES]

        create_args = ["xcrun", "xcodebuild", "-create-xcframework"]
        for slice_lib in slice_libs:
            create_args += ["-library", slice_lib, "-headers", HEADERS]
        create_args += ["-output", output]
        run(create_args)

    # create-xcframework has no version flag, so stamp the version into the
    # bundle's Info.plist after the fact. The bundle is freshly created each
    # run, so these keys never pre-exist and -insert is safe.
    info_plist = output / "Info.plist"
    run(["plutil", "-insert", "CFBundleShortVersionString", "-string", version, info_plist])
    run(["plutil", "-insert", "CFBundleVersion", "-string", version, info_plist])
    
    dedup_headers(output)

    print(f"\nCreated {output}")
    print(f"Bundled headers from '{HEADERS}' (including module.modulemap).")


if __name__ == "__main__":
    main()
