# Turbo 0.1.0 — standalone kernel preview

Turbo is a C++17 vector-search computation library extracted from zvec. It
provides distance kernels, batch evaluation, PQ ADC/SDC, 4-bit FastScan, FHT
rotation, and application of an existing OPQ matrix. Runtime dispatch selects
a compiled implementation that the current CPU and operating system can use.

This is a **kernel preview**, not the complete zvec quantizer SDK. Training,
IndexMeta/IndexHolder adapters, model persistence, and a stable binary ABI are
outside this release. It builds offline with CMake and a C++17 compiler; there
is no zvec, Highway, Eigen, or GoogleTest dependency.

中文规划：[独立发行规划](docs/PLAN.zh-CN.md)。
接口契约：[API and layouts](docs/API.md)。
验证记录：[Validation](docs/VALIDATION.md)。

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
./build/turbo_example
./build/turbo_targets
```

Multi-configuration generators put executables under `build/Release/`.
`turbo_targets` reports **compiled** and **usable** variants separately. A
compiler accepting an ISA flag does not establish hardware test coverage.

## Install and consume

```sh
cmake --install build --config Release --prefix /your/prefix
cmake -S tests/consumer -B consumer-build -DCMAKE_PREFIX_PATH=/your/prefix
cmake --build consumer-build --config Release
ctest --test-dir consumer-build -C Release --output-on-failure
```

In another project:

```cmake
find_package(Turbo 0.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE turbo::turbo)
```

Or vendor this source tree and use `add_subdirectory(path/to/turbo)` followed
by the same target. Tests, examples, and installation default to OFF when
Turbo is a subproject. Its public include path is `include/`; internal ISA
headers are not installed. `pkg-config --cflags --libs turbo` is also supported.
On Windows a shared consumer needs the installed DLL directory on `PATH`.

```cpp
#include <turbo/turbo.h>
float a[] = {1, 2, 3}, b[] = {3, 2, 1}, distance = 0;
auto k = turbo::get_distance_kernels(
    turbo::MetricType::kSquaredEuclidean,
    turbo::DataType::kFp32, turbo::QuantizeType::kFp32);
if (k.dist) k.dist(a, b, 3, &distance);  // squared L2 = 8
```

## Options

| CMake option | Default | Meaning |
|---|---|---|
| `TURBO_BUILD_SHARED` | OFF | Shared library instead of static library |
| `TURBO_ENABLE_SIMD` | ON | Allow runtime SIMD selection; OFF forces scalar dispatch |
| `TURBO_ENABLE_AVX512` | ON | Allow compiler-supported AVX512 variants on GCC/Clang |
| `TURBO_BUILD_TESTS` | top-level only | Dependency-free numerical/dispatch tests |
| `TURBO_BUILD_EXAMPLES` | top-level only | Distance example and target inspector |
| `TURBO_ENABLE_INSTALL` | top-level only | Headers, library, CMake config, pkg-config |
| `TURBO_ENABLE_SANITIZERS` | OFF | Sanitizers for GCC/Clang development builds |
| `TURBO_SANITIZERS` | `address,undefined` | Override with `undefined` for a UBSan-only run |

Supported build targets in this preview are 64-bit x86 and AArch64. Other
64-bit targets may use scalar code but are not validated. 32-bit targets and
single-build Apple universal binaries are explicitly rejected. Build one
Apple architecture per directory. Do not inject external `-march=native`,
ISA-enabling `-m` flags, `/arch`, or global LTO into a portable release build.

| Target/toolchain | Available backends (subject to CPU/OS and compiler checks) |
|---|---|
| x86-64 GCC / Clang | Scalar, SSE2, AVX2+FMA+F16C, AVX512 F/BW/DQ/VL, VNNI, FP16 |
| x86-64 MSVC | Scalar, SSE2, AVX2; AVX512 intentionally disabled pending validation |
| AArch64 GCC / Clang | Scalar, NEON; native FP16 where detected and compiled |
| AArch64 MSVC | Scalar/NEON build path; not yet validated for this release |

Not every backend implements every operation. `AVX`, `SVE`, `SVE2`, and
`kRabit` are reserved selectors, not implemented capabilities. Linux/Windows
CI definitions are supplied; consult the validation record for what actually
ran. Scalar dispatch can still contain compiler-generated baseline vector
instructions (SSE2 on x86-64, NEON on AArch64).

## Release checks and source package

```sh
python3 scripts/verify_release.py --jobs 4
python3 scripts/package.py
```

The verifier builds static/shared/scalar configurations, tests them, installs
each, moves its installation to another path, and builds an external consumer.
It also verifies `add_subdirectory`. It does not download dependencies.
Packaging writes `dist/turbo-0.1.0.tar.gz` and its SHA-256 file. The archive has
an explicit source allowlist and normalized metadata; set `SOURCE_DATE_EPOCH`
to control timestamps. Build outputs and neighboring zvec files are excluded.
No script publishes to an external package registry.

## Compatibility and provenance

Public C++ source API and encoded layouts are experimental in 0.x. CMake
version matching is restricted to the same minor version. Shared libraries
have SOVERSION `0.1`; binary compatibility across compilers/STLs or future
minor versions is not promised. The source package is the portable deliverable.

The kernel snapshot and original hashes are recorded in `UPSTREAM.json`.
This tree is independent: changes to the adjacent zvec checkout do not update
it automatically. All copied source retains its original notices. New and
adapted code is distributed under Apache-2.0; see `LICENSE` and `NOTICE`.

Highway references used for the distribution design:
[CMake packaging](https://github.com/google/highway/blob/master/CMakeLists.txt),
[runtime target detection](https://github.com/google/highway/blob/master/hwy/targets.cc).
Turbo exposes domain kernels rather than Highway's general SIMD intrinsic API.
