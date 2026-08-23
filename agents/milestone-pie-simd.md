# ESP32-P4 PIE SIMD Acceleration Milestone

## Summary

Add transparent ESP32-P4 PIE SIMD acceleration beneath existing graphics and
terminal APIs. PPA remains preferred for supported bulk operations. PIE handles
measured-beneficial CPU raster work; scalar C remains authoritative fallback.

## Fixed Decisions

- Keep PIE private to kernel/platform code. Public application ABI does not change.
- Default `TABOS_ENABLE_PIE` to enabled on Tab5 and provide a forced-scalar build.
- Default `TABOS_ENABLE_PIE_DIAGNOSTICS` to disabled.
- Preserve byte-identical RGB565 output, clipping, stride, tails, and overlap rules.
- Prefer PPA, then PIE, then scalar C.
- Retain each PIE kernel only after repeatable physical-Tab5 improvement.
- Do not reserve or pin a CPU core for PIE.
- Rely on ESP-IDF FreeRTOS lazy PIE context ownership and save/restore.

## Implementation Checklist

- [x] Add internal scalar raster-kernel interface and Tab5 PIE dispatch.
- [x] Add configurable PIE and PIE-diagnostic build switches.
- [x] Add RGB565 span fill and copy PIE kernels.
- [ ] Add exact fixed-opacity blend and color-key kernels where measurements win.
- [ ] Add CP437 glyph-row expansion kernel where measurements win.
- [x] Route terminal dirty-cell rendering through raster kernels.
- [x] Route direct-graphics CPU fallbacks through raster kernels.
- [x] Add optional internal/PSRAM serial microbenchmarks and output hashes.
- [x] Expand diagnostics across glyph, terminal-cell, sprite, and large-region spans.
- [ ] Verify forced task switching preserves PIE state.
- [x] Record physical thresholds and benchmark results here.
- [x] Update user graphics/display documentation.

## Acceptance

Scalar and PIE output must match byte-for-byte. Host behavior remains unchanged.
Tab5 builds work with PIE enabled and disabled. Hardware diagnostics report buffer
placement, workload, elapsed time, throughput, speedup, and hash. Only operations
showing repeatable hardware gains remain enabled.

## Physical Baseline

Three repeated 65,536-pixel runs on ST7121 Tab5 produced matching hashes and stable
bulk results: internal fill 11.3x, PSRAM fill 11.2x, internal copy 1.8x, and PSRAM
copy about 1.07x.

Aligned size sweep found 8-pixel fill slower than scalar in both memory types and
8-pixel copy neutral/slower. At 16 pixels, fill reached 1.55x and copy reached
1.19x internal / 1.22x PSRAM, with gains increasing through larger spans. Selected
minimum is therefore 16 pixels for both fill and copy. Every tested output hash matched.
