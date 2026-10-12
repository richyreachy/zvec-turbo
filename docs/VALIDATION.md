# Local validation — 2026-10-12

Environment: macOS ARM64, AppleClang 15.0.0, C++17. This record distinguishes
compilation from execution and does not claim that unrun CI jobs have passed.

| Check | Result |
|---|---|
| Native ARM64 Release, static library | Passed numerical/dispatch checks |
| Native ARM64 Release, shared library | Passed numerical/dispatch checks; see first-launch note below |
| Scalar-dispatch Release | Passed numerical/dispatch checks |
| Static install → move prefix → external `find_package` consumer | Passed |
| Shared install → move prefix → external consumer | Passed |
| Scalar install → move prefix → external consumer | Passed |
| `add_subdirectory` consumer | Passed |
| x86-64 cross build, SSE2/AVX2/AVX512/VNNI/FP16 compiler variants | Compiled successfully |
| x86-64 execution through Rosetta | Scalar/SSE2 passed; AVX2/AVX512 unavailable to this process |
| Native ARM64 UBSan Debug | Passed, no runtime-error diagnostics |
| ASan+UBSan on local AppleClang runtime | Not validated: runtime startup fails, including an empty probe executable |
| Source archive extraction and standalone rebuild | Passed; repeated packaging produced identical SHA-256 |
| Linux / Windows native execution | Not run locally; CI definitions supplied |

Final numerical check counts: ARM64 static/shared/UBSan **69,588**, Scalar-dispatch
**68,383**, x86-64 Rosetta **69,352**. Counts differ because unavailable ISA
implementations are not executed.

The tests exercise FP32 distance/batch and zero-length behavior; all 65,536
binary16 encodings and selected rounding boundaries; raw FP16 overflow-safe
L2; bounded native-half distance/batch; aligned Record INT4/INT8 L2/IP with
batch query preprocessing; raw UINT8 L2; PQ ADC/SDC/batch; odd/even FastScan
packing; FHT forward/inverse and OPQ matrix application; invalid selectors and
strict explicit ISA handling; synthetic x86 OSXSAVE/XCR0 feature masks.

Unavailable kernels are skipped without executing their instructions. This
does not validate their numerical behavior. Uniform/VNNI formats, AVX512
hardware execution, wide non-finite-input behavior and large-dimension
overflow limits remain release-hardening work. Test counts include the 65,536
binary16 checks and should not be interpreted as distinct integration tests.

## Reproduction

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/turbo_targets
python3 scripts/verify_release.py --jobs 4

cmake -S . -B build-ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DTURBO_ENABLE_SANITIZERS=ON -DTURBO_SANITIZERS=undefined
cmake --build build-ubsan --parallel 4
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-ubsan --output-on-failure

cmake -S . -B build-x86 -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build build-x86 --parallel 4
ctest --test-dir build-x86 --output-on-failure
```

## Local host caveats

Some newly linked shared-test processes were killed by macOS on first launch.
The host log reported AMFI signature/provenance checks for those executables;
`codesign --verify` accepted the dylib. Unchanged binaries passed on subsequent
execution, followed by successful shared installation/relocation/consumption.
The release verifier remains fail-fast and does not silently retry failed tests.
The initial all-in-one run therefore failed locally; individual static/shared/
scalar verification stages were completed separately. No OS security setting
was disabled to run the tests. This first-launch behavior should be checked
on a clean macOS CI runner before a stable binary release.

ASan separately failed at runtime initialization with
`sanitizer_malloc_mac.inc:189 (!asan_init_is_running)`. A minimal `int main()`
program built with the same sanitizer toolchain also failed to launch locally.
UBSan-only testing passed; ASan execution remains a CI requirement, not a
claimed local pass. The implementation and CMake retain ASan support.
