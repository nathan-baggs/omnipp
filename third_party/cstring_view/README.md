# beman.cstring_view: cstring_view, a null-terminated string view

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<!-- markdownlint-disable line-length -->
[![Library Status](https://raw.githubusercontent.com/bemanproject/beman/refs/heads/main/images/badges/beman_badge-beman_library_under_development.svg)](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#the-beman-library-maturity-model)
[![Continuous Integration Tests](https://github.com/bemanproject/cstring_view/actions/workflows/ci_tests.yml/badge.svg)](https://github.com/bemanproject/cstring_view/actions/workflows/ci_tests.yml)
[![Lint Check (pre-commit)](https://github.com/bemanproject/cstring_view/actions/workflows/pre-commit-check.yml/badge.svg)](https://github.com/bemanproject/cstring_view/actions/workflows/pre-commit-check.yml)
[![Coverage](https://coveralls.io/repos/github/bemanproject/cstring_view/badge.svg?branch=main)](https://coveralls.io/github/bemanproject/cstring_view?branch=main)
![Standard Target](https://github.com/bemanproject/beman/blob/main/images/badges/cpp29.svg)
[![Compiler Explorer Example](https://img.shields.io/badge/Try%20it%20on%20Compiler%20Explorer-grey?logo=compilerexplorer&logoColor=67c52a)](https://godbolt.org/z/s5KG3ozav)
<!-- markdownlint-restore -->

`beman.cstring_view` is a header-only `cstring_view` library.

**Implements**: `std::cstring_view` proposed in [cstring\_view (P3655R2)](https://wg21.link/P3655R2).

**Status**: [Under development and not yet ready for production use.](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#under-development-and-not-yet-ready-for-production-use)

## License

`beman.cstring_view` is licensed under the Apache License v2.0 with LLVM Exceptions.

## Usage

`std::cstring_view` exposes a string\_view like type that is intended for being able to propagate prior knowledge that
a string is null-terminated throughout the type system, while fulfilling the same role as string\_view.

The following code snippet illustrates how we can use `cstring_view` to make a beginner-friendly `main`:

```cpp
#include <beman/cstring_view/cstring_view.hpp>
#include <vector>

int main(int argc, const char** argv) {
  std::vector<cstring_view> args(argv, argv+argc);
}
```

Full runnable examples can be found in [`examples/`](examples/).

## Dependencies

### Build Environment

This project requires at least the following to build:

* A C++ compiler that conforms to the C++20 standard or greater
* CMake 3.30 or later
* (Test Only) GoogleTest

You can disable building tests by setting CMake option `BEMAN_CSTRING_VIEW_BUILD_TESTS` to
`OFF` when configuring the project.

### Supported Platforms

| Compiler   | Version | C++ Standards | Standard Library  |
|------------|---------|---------------|-------------------|
| GCC        | 16-13   | C++26-C++17   | libstdc++         |
| GCC        | 12-11   | C++23-C++17   | libstdc++         |
| Clang      | 22-19   | C++26-C++17   | libstdc++, libc++ |
| Clang      | 18      | C++26-C++17   | libc++            |
| Clang      | 18      | C++23-C++17   | libstdc++         |
| Clang      | 17      | C++26-C++17   | libc++            |
| Clang      | 17      | C++20, C++17  | libstdc++         |
| AppleClang | latest  | C++26-C++17   | libc++            |
| MSVC       | latest  | C++23         | MSVC STL          |

## Development

See the [Contributing Guidelines](CONTRIBUTING.md).

## Integrate beman.cstring_view into your project

### Build

You can build cstring_view using a CMake workflow preset:

```bash
cmake --workflow --preset gcc-release
```

To list available workflow presets, you can invoke:

```bash
cmake --list-presets=workflow
```

For details on building beman.cstring_view without using a CMake preset, refer to the
[Contributing Guidelines](CONTRIBUTING.md).

### Installation

#### Vcpkg

The preferred way to install cstring_view is via vcpkg. To do so, after installing vcpkg
itself, you need to add support for the Beman project's [vcpkg
registry](https://github.com/bemanproject/vcpkg-registry) by configuring a
`vcpkg-configuration.json` file (which cstring_view [provides](vcpkg-configuration.json)).

Then, simply run `vcpkg install beman-cstring-view`.

#### Manual

To install beman.cstring_view globally after building with the `gcc-release` preset, you can
run:

```bash
sudo cmake --install build/gcc-release
```

Alternatively, to install to a prefix, for example `/opt/beman`, you can run:

```bash
sudo cmake --install build/gcc-release --prefix /opt/beman
```

This will generate the following directory structure:

```txt
/opt/beman
├── include
│   └── beman
│       └── cstring_view
│           ├── cstring_view.hpp
│           └── ...
└── lib
    └── cmake
        └── beman.cstring_view
            ├── beman.cstring_view-config-version.cmake
            ├── beman.cstring_view-config.cmake
            └── beman.cstring_view-targets.cmake
```

### CMake Configuration

If you installed beman.cstring_view to a prefix, you can specify that prefix to your CMake
project using `CMAKE_PREFIX_PATH`; for example, `-DCMAKE_PREFIX_PATH=/opt/beman`.

You need to bring in the `beman.cstring_view` package to define the `beman::cstring_view` CMake
target:

```cmake
find_package(beman.cstring_view REQUIRED)
```

You will then need to add `beman::cstring_view` to the link libraries of any libraries or
executables that include `beman.cstring_view` headers.

```cmake
target_link_libraries(yourlib PUBLIC beman::cstring_view)
```

### Using beman.cstring_view

To use `beman.cstring_view` in your C++ project,
include an appropriate `beman.cstring_view` header from your source code.

```c++
#include <beman/cstring_view/cstring_view.hpp>
```
