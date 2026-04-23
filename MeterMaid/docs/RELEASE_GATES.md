# Release Gates

A release is only green when every gate below has evidence attached to
the release notes.

## Gate 1 — Numerical trust  ✅ automated

| Check | Status | Evidence |
|---|---|---|
| Momentary LUFS within ±0.5 LU of reference tone | CI-gated | `tests/ReferenceToneTest.cpp` |
| Short-term LUFS within ±0.5 LU of reference tone | CI-gated | same |
| Integrated LUFS within ±0.5 LU of reference tone | CI-gated | same |
| True peak within ±0.5 dBTP of analytical peak | CI-gated | same |
| Correlation within ±0.05 of analytical value | CI-gated | same |

The `reference-test` CI job must pass before `build-macos` is allowed
to run. Run locally with:

```bash path=null start=null
cmake -S MeterMaid -B build -DJUCE_PATH=$PWD/JUCE -DMETERMAID_BUILD_TESTS=ON
cmake --build build --target MeterMaid_ReferenceTest -j
./build/MeterMaid_ReferenceTest
```

## Gate 2 — Host trust  ⏳ manual

| Host | Status |
|---|---|
| Reaper | ⏳ |
| Ableton Live 11 / 12 | ⏳ |
| FL Studio | ⏳ |
| Logic Pro (AU) | ⏳ |
| Cubase | ⏳ |

Evidence: screen-capture of plugin loading, saving / restoring state,
automating no parameters (this is a pure meter — no automation surface
yet).

## Gate 3 — RT / performance trust  ⏳ manual

| Check | Status |
|---|---|
| 32-sample buffer stable | ⏳ |
| 192 kHz stable | ⏳ |
| 30-minute session no dropouts | ⏳ |
| No audio-thread allocations (verified via DAW profiler) | ⏳ |

## Gate 4 — UX trust  ⏳ manual

| Check | Status |
|---|---|
| Resizable window, aspect preserved | ⏳ |
| Timeline ruler and labels finished | ⏳ |
| Peak-hold indicators on per-channel peak | ⏳ |
| Export loudness report (CSV or PDF) | ⏳ |

## Gate 5 — Distribution trust  ⏳ partial

| Check | Status |
|---|---|
| macOS universal build (arm64 + x86_64) | ✅ CI |
| macOS code signing (Developer ID) | ⏳ |
| macOS notarization | ⏳ |
| Windows build | ⏳ |
| Linux build | ⏳ |

## Release promotion rule

A `v*` tag that doesn't include `-beta` / `-alpha` / `-rc` must only be
cut when Gates 1–5 are all green. Until then, builds ship as
pre-release (CI marks them automatically).
