# cpp-media-library
A C++20 image library and command-line tool.
The current application loads an image and saves its pixels as PNG.

## Current capabilities

- Load PNG/JPEG images into owned, tightly packed RGB8 pixel buffers.
- Save Gray8 or RGB8 images as PNG.
- Convert files using input and output paths supplied on the command line.
- Report invalid arguments and file I/O errors with a nonzero exit code.

Alpha-channel input is rejected. Filters, video, and GPU processing are future
work. Grayscale input is expanded to RGB8 when loaded. Output is always PNG,
regardless of the output filename; use a `.png` extension.

## Prerequisites

- CMake 3.23 or newer for the project itself.
- A compiler with C++20 support.
- The build tool required by your CMake generator.

Image decoding and encoding use the vendored
[stb headers](https://github.com/nothings/stb) in `third_party/stb`; no separate
image-library installation is needed.

The following commands demonstrate how to set up your environment and compile this program.
Keep build directories separate for different platforms, compilers, and generators.

## Build on macOS

Install Apple's Xcode Command Line Tools for `clang++` and `make`:

```sh
xcode-select --install
```

Complete the installation dialog before continuing. Install CMake, for example
with an existing [Homebrew](https://brew.sh) installation:

```sh
brew install cmake
```

Alternatively, install it from [cmake.org](https://cmake.org/download/) and make
`cmake` available in your terminal.

Build using the repository script:

```sh
.\build.sh
```

The script configures a Debug build using `clang++`, terminating after any encountered failures.
Equivalent manual commands are:

```sh
cmake -S . -B build-macos -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build-macos --parallel 2
```

Run help for usage instructions, or upload a PNG/JPEG image to convert into PNG format:

```sh
./build-macos/media_demo --help
./build-macos/media_demo input_filepath.jpeg output_filepath.png
```

## Build on Windows

The current PowerShell script targets Visual Studio 2026's MSVC compiler, using
CMake's `Visual Studio 18 2026` generator. Install Visual Studio's C++ desktop
build tools and a CMake version that supports that generator.

From PowerShell:

```powershell
.\build.ps1
.\build\Debug\media_demo.exe --help
.\build\Debug\media_demo.exe test\IMG_5618.jpeg build\output.png
```

The script builds the Debug executable and stops on failure. If using another
Visual Studio version, configure a separate build directory with the matching
generator rather than reusing a directory configured for another toolchain.

## Build on Linux

Install CMake, a C++20 compiler such as GCC, and GNU Make using your system's
package manager. Then run:

```sh
cmake -S . -B build-linux -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux --parallel 2
./build-linux/media_demo --help
./build-linux/media_demo test/IMG_5618.jpeg build-linux/output.png
```

## Command-line usage

```text
media_demo <input-image> <output.png>
media_demo --help
media_demo -h
```

The output directory must already exist. A relative path is resolved from the
terminal's current working directory. Quote paths containing spaces:

```sh
./build-macos/media_demo "photos/my photo.jpg" "build-macos/my output.png"
```

A successful conversion prints the input dimensions, pixel byte count, and
output path. RGB8 uses three bytes per pixel, so the byte count is
`width × height × 3`. PNG compression is lossless relative to the decoded image.

Help and successful conversions exit with `0`. Invalid argument counts,
missing/corrupt input, unsupported alpha input, and output failures exit with a
nonzero status. Errors are written to standard error.

## Library layout

- `media_core`: the `Image` type, pixel formats, and representation validation.
- `media_io`: stb implementations, image loading, and PNG saving; depends on
  `media_core`.
- `media_demo`: the CLI executable; links to `media_io`.

Public headers live in `include/media/`; implementations live in `src/`.

## Future Plans
More work is to come in developing further features and tools available in this CLI image library tool. CUDA integration for multicore GPU processing is also planned in the future, as well as supporting video formats.