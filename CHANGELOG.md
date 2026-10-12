# Changelog

## 0.1.0 — kernel preview, 2026-10-12

- Extracted vector distance, conversion, PQ/FastScan and rotation kernels.
- Introduced independent `turbo` namespace, headers and CMake package.
- Added raw-pointer dispatch API, compiled/usable target inspection, and strict
  explicit-ISA selection; rejected invalid PQ dtype combinations.
- Added x86 OSXSAVE/XCR0 gating and per-translation-unit ISA feature bundles.
- Added portable nearest-even binary16 conversion and an owning FHT helper.
- Added static/shared installation, relocatable CMake/pkg-config metadata,
  external-consumer tests, CI definition and deterministic source packaging.
- Retained the original kernel provenance and Apache-2.0 notices.

Known scope limits: complete quantizer training/model persistence and zvec
integration remain in the roadmap. Hardware coverage is listed separately in
`docs/VALIDATION.md`; compiled AVX512 code does not imply AVX512 execution tests.
