# API contract for 0.1

Include `<turbo/turbo.h>`. Functions use namespace `turbo`; kernels use raw
function pointers. The library has no global thread pool and kernel calls do
not allocate. Resolve a kernel once, then reuse it outside dispatch lookup.

## Dispatch

`kAuto` chooses the first usable implementation for the requested operation,
falling back to scalar **where a scalar implementation exists**. Explicit ISA
requests are strict: unsupported ISA/operation combinations return null fields.
Unsupported dtype/quantizer combinations also return empty results. Always
check the field you intend to call. Priority is fixed; it is not an autotuner.

`is_arch_compiled()` describes build capability; `is_arch_supported()` also
checks CPU and OS state. Neither promises every operation for that ISA.
`DistanceKernels::arch` reports the selected implementation when `dist` exists.
Use the aggregate `get_distance_kernels` to keep distance/batch/preprocess paired.
When `preprocess` is non-null, apply it **once to a writable copy of the query**
before batch evaluation. Retain the original query for the single-distance
path. The two representations must not be interchanged.

## Numeric conventions

| Selector | Result/input contract |
|---|---|
| FP32 + `kFp32`, squared Euclidean | Sum of squared differences, not its square root |
| FP32/FP16 inner product | Negative dot product (smaller is better) |
| FP32/FP16 cosine | `1 - dot`; caller supplies unit-normalized vectors |
| FP16 + `kRaw`, squared Euclidean | Binary16 inputs widened to FP32 for arithmetic |
| FP16 + `kFp16` | May select native FP16 arithmetic; use for bounded/normalized values |
| UINT8 + `kRaw`, squared Euclidean | Unsigned byte vectors, FP32 output |

SIMD reduction order and fused operations can change rounding. No bitwise
floating-point equivalence across ISAs is promised. Native half arithmetic
can overflow where FP32 arithmetic does not. General non-finite-input behavior
is not standardized in this preview. `fp32_to_fp16` performs nearest-even
conversion; `fp16_to_fp32` widens IEEE binary16. NaN payload preservation is
not promised. Do not assume `kRaw` supplies all dtype/metric combinations.

## Buffer requirements

Low-level pointers are unchecked. Supply sufficiently sized, correctly typed
buffers; query/output must not overlap input unless an API explicitly permits
in-place use. `dim` means element count for raw FP32/FP16/UINT8, but follows
the encoded format for Record/uniform kernels. Batch accepts a gather array
of datapoint pointers. Output holds `num` floats; unused `extra_values` can be
null, but format-specific kernels may require it. These kernels are trusted
in-process primitives, not parsers for untrusted blobs.

Record and uniform families retain upstream layouts and are advanced preview
APIs. They are **not** arbitrary raw signed/unsigned integer dot products.
For Record INT8 L2/IP, the data is `n` signed bytes plus four FP32 fields
`scale, bias, sum, square_sum` and one int32 code sum; `dim=n+20`.
The reconstruction is `scale*code+bias`. Record INT4 uses packed signed
nibbles followed by four FP32 fields; `dim=n+32` is measured in nibble units.
Metadata must be float-aligned: use aligned backing storage and n divisible
by 4 for INT8 or by 8 for INT4. Cosine layouts include additional norm data;
consult the corresponding `src/distance/*/record_quantized_*` kernels before
constructing them. Safe general encoders and a complete format specification
are planned for the quantizer SDK, not claimed by this preview.

Uniform distance formats also have norm/query metadata and may use
`extra_values`; their uint4/uint7/uint8 accessors currently require VNNI.
The uniform quantization accessors return null when the optimized kernel was
not compiled or cannot run. Do not treat null as an identity transform.
`get_convert_func` is likewise an optional optimized path; the public
`fp32_to_fp16` helper is the portable FP16 conversion path.

## PQ and FastScan

PQ INT8 stores one unsigned code per subquantizer, with 256 centroids. PQ INT4
packs two codes per byte, low nibble first, with 16 centroids. ADC uses an
FP32 lookup table `[subquantizers][centroids]`; SDC uses an FP32 table
`[subquantizers][centroids][centroids]`. These functions do not train codebooks
or construct float LUTs.

Include `<turbo/fast_scan.h>` for packing helpers. FastScan scans one block of
32 vectors using uint8 LUT entries and produces 32 int32 sums. The caller
applies any affine LUT dequantization. Allocate via
`fast_scan_packed_block_size(m)` / `fast_scan_packed_lut_size(m)`, then call
`fast_scan_pack_codes` and `fast_scan_pack_lut`. Odd subquantizer counts get a
zero-LUT padding group. For a partial block, ignore outputs beyond the real
vector count. Code stride is in bytes; input per-vector code length is
`ceil(m/2)`. Bound m so `m*255` fits int32 and all allocation sizes fit size_t.

## Rotation

Include `<turbo/rotation.h>` for `FhtRotation(dim, seed)`. It owns an aligned
context and exposes `apply` / `inverse`; identical seeds generate identical
sign bits. Non-power-of-two dimensions use the inherited Kac-walk algorithm.
Input and output may be identical for FHT. Invalid dimensions or unavailable
explicit backends throw `std::invalid_argument`. Context memory is not a
serialized format and must not be persisted as a model.

The low-level OPQ rotate/unrotate functions accept a row-major FP32 square
matrix via `ctx`, with equal input/output dimensions. Rotation computes R*x;
inverse computes transpose(R)*x, so the caller must supply an orthogonal R.
Use separate input/output buffers. OPQ training is not included.

## Versioning

The exposed enum IDs preserve upstream identifiers for traceability, not a
promise to read complete zvec models. Unimplemented IDs remain reserved.
The API/layouts are experimental in 0.x; no cross-toolchain binary ABI or
quantizer model compatibility is promised. Only documented headers are public.
