cpp-http-server

A C++20 HTTP/1.1 server with an image-processing library and command-line tool.

Implements request parsing, routing, and response serialization directly on
Winsock and POSIX sockets, without an HTTP framework or parser dependency.

Supports grayscale conversion, thresholding, box blur, Gaussian blur, and
Sobel edge detection through an ordered operation pipeline.

```sh
media_demo input.jpg edges.png --grayscale --gaussian-blur 3 1.0 --sobel
```

## Features

- HTTP/1.1 request parsing and response serialization.
- Method/path routing with registered handler functions.
- Host validation, Content-Length framing, and bounded request sizes.
- Partial socket writes and total receive/send deadlines.
- Move-only RAII socket ownership and per-request error handling.
- Windows Winsock and macOS POSIX networking.
- PNG/JPEG input and PNG output.
- Owned, tightly packed Gray8 and RGB8 pixel buffers.
- Image dimension, format, and storage validation.
- Sequential CPU image-processing operations.
- Configurable operation chains.
- Separate processing, file I/O, and CLI components.

## Requirements

- CMake 3.23 or newer.
- A compiler and standard library supporting C++20, including `std::format`.
- A build tool compatible with the selected CMake generator.

The HTTP server core uses the C++ standard library and operating-system
socket APIs. Windows builds link against `Ws2_32`. No third-party HTTP
dependency is required.

Image decoding and encoding use the vendored
[stb libraries](https://github.com/nothings/stb). No separate image-library
installation is required.

## Build

Clone the repository:

```sh
git clone https://github.com/kojiaquilino/cpp-http-server.git
cd cpp-http-server
```

Run build commands from the repository root. Use separate build directories
for different compilers and generators.

### Linux

Install CMake, GCC or Clang, and GNU Make.

```sh
cmake -S . -B build-linux -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --parallel 2
./build-linux/media_demo --help
```

### macOS

Install Apple's Command Line Tools:

```sh
xcode-select --install
```

Install CMake through Homebrew or [cmake.org](https://cmake.org/download/),
then build:

```sh
cmake -S . -B build-macos -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build-macos --parallel 2
./build-macos/media_demo --help
```

Alternatively, use the repository script for a Debug build:

```sh
bash build.sh
```

### Windows

The PowerShell script targets Visual Studio 2026 with CMake's
`Visual Studio 18 2026` generator.

Install Visual Studio's C++ desktop build tools and a CMake version supporting
that generator.

```powershell
.\build.ps1
.\build\Debug\media_demo.exe --help
```

For other Visual Studio versions, configure a separate build directory with
the matching generator.

## HTTP server

The build produces a separate `media_server` executable. Start it from the
repository root:

```sh
# macOS
./build-macos/media_server
```

```powershell
# Windows
.\build\Debug\media_server.exe
```

The server listens on `127.0.0.1:8080`. The address and port are currently
specified in `server/main.cpp`.

From a separate terminal, send a request:

```sh
curl -i http://127.0.0.1:8080/
```

Use `curl.exe` in Windows PowerShell. The current demonstration route returns
`200 OK` with a `text/plain` body of `Hello :)`. Unknown paths return `404`;
POST to `/` returns `405` with `Allow: GET`.

### Protocol support

- HTTP/1.1 over TCP, with GET and POST supported by the server.
- Case-insensitive header names and a required, validated Host header.
- Content-Length request bodies, including bytes split across socket reads.
- Method/path routing; query strings are excluded from route matching.
- `404` for unknown paths and `405` with `Allow` for disallowed route methods.
- Complete response writes with byte-based Content-Length and CRLF framing.
- One request per connection, followed by `Connection: close`.
- HTTP rejection responses and connection cleanup on transport failures.

| Limit | Value |
|---|---|
| Request line, excluding CRLF | 4,094 bytes |
| Header section, including request line and final CRLFCRLF | 20 KiB |
| Request body | 12 MiB |
| Total request receive budget | 10 seconds |
| Total response send budget | 10 seconds |

Requests without Content-Length are treated as having an empty body.
Transfer-Encoding and Expect headers are rejected. Persistent connections,
pipelining, chunked transfer encoding, HTTP/2, and TLS are outside the current
protocol subset. Connections are handled sequentially.

### Handler registration

Handlers receive a parsed `Request` and return a `Response`. Register them
before the server enters its accept loop:

```cpp
#include "http/router.hpp"

Router router;

router.register_handler("GET", "/hello", [](const Request&) {
    return Response{200, "OK", "text/plain", "Hello"};
});
```

The router dispatches complete requests to their registered handlers. Handlers
can call application libraries without adding application-specific logic to
the router. Duplicate routes, invalid registrations, and empty handlers are
rejected.

The current server demonstrates HTTP handling independently of the image
library. Image-processing API endpoints and the browser interface are planned.

## Command-line usage

```text
media_demo <input-image> <output.png> [operations...]
media_demo --help
media_demo -h
```

Operations execute from left to right. Each operation processes the result
of the previous operation. Without operations, the application loads the
image and saves its decoded pixels as PNG.

The examples below use the Linux executable path. Substitute
`./build-macos/media_demo` on macOS or `.\build\Debug\media_demo.exe` on Windows.

### Examples

Copy an image to PNG:

```sh
./build-linux/media_demo input.jpg output.png
```

Convert to grayscale:

```sh
./build-linux/media_demo input.jpg gray.png --grayscale
```

Apply thresholding:

```sh
./build-linux/media_demo input.jpg binary.png --grayscale --threshold 128
```

Apply box blur:

```sh
./build-linux/media_demo input.jpg blurred.png --grayscale --box-blur
```

Apply Gaussian blur:

```sh
./build-linux/media_demo input.jpg gaussian.png --grayscale --gaussian-blur 3 1.0
```

Smooth an image and detect edges:

```sh
./build-linux/media_demo input.jpg edges.png --grayscale --gaussian-blur 3 1.0 --sobel
```

### Operations

| Option | Required input | Parameters |
|---|---|---|
| `--grayscale` | RGB8 | None |
| `--threshold <cutoff>` | Gray8 | Integer cutoff from `0` to `255` |
| `--box-blur` | Gray8 | None |
| `--gaussian-blur <radius> <sigma>` | Gray8 | Nonnegative integer radius; positive, finite sigma |
| `--sobel` | Gray8 | None |

For RGB input, use `--grayscale` before grayscale-only operations.
Gray8 input can be processed directly. Applying `--grayscale` to Gray8 input
returns the image unmodified.

Operations may be repeated when their format requirements remain satisfied.
An incompatible operation stops the pipeline without saving an intermediate
result.

### Paths and exit codes

- The output directory must already exist.
- Relative paths are resolved from the terminal's current working directory.
- Paths containing spaces must be quoted.
- Output is always PNG; use a `.png` extension.
- Successful processing and help return `0`.
- Invalid arguments, incompatible operations, and I/O failures return a
  nonzero exit code.
- Errors are written to standard error.

## Processing details

### Image representation

| Format | Channels | Storage |
|---|---|---|
| Gray8 | One grayscale channel | One byte per pixel |
| RGB8 | Red, green, blue | Three bytes per pixel |

Pixels use unsigned 8-bit channel values from `0` to `255`, stored in
row-major order with tightly packed rows. Grayscale input remains Gray8;
color input is loaded as RGB8. Alpha-channel input is rejected.

### Algorithms

| Operation | Behavior |
|---|---|
| Grayscale | Uses `(299R + 587G + 114B + 500) / 1000` for integer rounding. |
| Threshold | Values below the cutoff become `0`; values at or above it become `255`. |
| Box blur | Computes a rounded uniform average over a 3×3 neighborhood. |
| Gaussian blur | Applies a normalized one-dimensional kernel in horizontal and vertical passes, retaining floating-point intermediates. |
| Sobel | Computes horizontal and vertical gradients, then rounds and clamps their Euclidean magnitude to `0–255`. |

Blur and Sobel operations replicate the nearest edge pixel for
out-of-bounds samples.

Gaussian radius controls the neighborhood size; sigma controls the spread of
the weights. A radius around three times sigma is a useful starting point.
Radius `0` leaves pixels unchanged.

Sobel produces dark output in constant regions and brighter output at
stronger edges. Large magnitudes saturate at white. Gaussian blur can be
applied before Sobel to reduce responses to noise and fine texture.

PNG encoding is lossless relative to the processed pixel data. Converting
a JPEG does not restore information lost through JPEG compression.

## C++ API

Individual operations are declared in `media/operations.hpp`. Ordered chains
are declared in `media/pipeline.hpp`.

```cpp
#include "media/image_io.hpp"
#include "media/pipeline.hpp"

#include <exception>
#include <iostream>
#include <vector>

int main() {
    try {
        Image input = load_image("input.jpg");

        std::vector<PipelineOperation> operations{
            GrayscaleOperation{},
            GaussianBlurOperation{3, 1.0},
            SobelOperation{}
        };

        Image output = apply_pipeline(input, operations);
        save_png(output, "edges.png");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

`apply_pipeline` preserves the original image and returns a new image.
An empty chain returns an independent copy. Invalid image data, parameters,
or operation ordering raise exceptions.

Within the project's CMake build, link callers against `media_core` for
processing or `media_io` for processing with image loading and saving.

## Project structure

```text
include/media/    Public library headers
src/             Library implementations and CLI
server/http/     HTTP parsing, responses, routing, and error types
server/net/      Socket ownership, platform networking, and byte I/O
server/main.cpp  HTTP server application
third_party/stb/ Image decoding and encoding dependencies
docs/            Technical documentation and development roadmap
```

| CMake target | Responsibility |
|---|---|
| `media_core` | Image representation, validation, operations, and pipelines |
| `media_io` | Image loading and PNG saving; depends on `media_core` |
| `media_demo` | Command-line application; links to `media_io` |
| `server_net` | HTTP and socket implementation; independent of media processing |
| `media_server` | HTTP server executable; links to `server_net` |

## Current scope

The HTTP server implements a bounded HTTP/1.1 subset with reusable routing and
Windows/macOS socket implementations. Its current executable serves a
demonstration handler; image uploads and processing over HTTP are not yet
connected.

Processing currently uses the sequential CPU backend and supports Gray8 and
RGB8 images. Filtering operations require Gray8 input.

Interactive frontend, parallel CPU execution, CUDA, resizing, restoration, and video processing are
planned extensions.
