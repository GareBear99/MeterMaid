# MeterMaid — Technical Audit

Scope: `v0.7.0-beta.3`. This document is the public-facing, file-by-file
audit of the current surface. It supersedes the prior internal codename
version.

## Verdict

MeterMaid is a **credible pre-production mastering & metering suite**
with a MiniMeters-class visualisation surface (loudness, true peak,
dynamics, stereo, spectrum, spectrogram, chroma, goniometer, histogram,
octagonal target radar) built on top of an ITU-R BS.1770-4 / EBU R128
loudness engine.

The remaining blocker from the earlier audit (numerical validation
against reference tones) is now addressed in-process by
`tests/ReferenceToneTest.cpp`, wired into CI as Gate 1. Host-matrix
verification (major DAW load / save / automation proof) is still open.

## Source/LockFreeFramePipe.h

Double-buffered latest-frame handoff. Audio thread writes into the
inactive buffer and atomically swaps. UI reads only the most recent
complete frame. No dynamic allocation, no torn payload reads.

## Source/MeterFrame.h

Plain-old-data snapshot copied across the SPSC pipe. v0.7.0-beta.3
extends the frame with:

- Per-channel sample peak + true peak + RMS
- Loudness range (LRA), PSR, PLR, crest factor
- Stereo width %, chromagram (12 pitch classes)
- 256-frame log-frequency spectrogram ring
- Loudness histogram (48 buckets)
- Raw L/R goniometer point ring

Kept POD-like, no heap ownership, no pointers.

## Source/MeterEngine.h

Single-file metering engine. Structure:

| Block | Responsibility | Standard |
|---|---|---|
| `KWeighting` | Pre-filter | ITU-R BS.1770-4 |
| `IntegratedGate` | Absolute (-70 LUFS) + relative (-10 LU) gated ring | BS.1770-4 / EBU R128 |
| `updateLoudness` | Momentary (400 ms), Short-Term (3 s), Integrated | BS.1770-4 |
| `updateLra` | P95 − P10 over gated short-term history | EBU R128 / Tech 3342 |
| `updateTruePeak` | 2× polyphase IIR oversampling per channel + summed | BS.1770-4 Annex 2 |
| `updateSpectrum` | 2048-pt Hann FFT, linear + log-binned spectrogram, chroma | internal |
| `updateStereoWidth` | Mid/side energy ratio | internal |
| `updateDecisionLayer` | Streaming-target rule engine | internal |

Strong points
- All analysis is deterministic and audio-thread-friendly.
- Scratch buffers (`weightedScratch`, `monoScratch`, `chanScratchL/R`,
  `momentaryRing`, `shortTermRing`) are preallocated in `prepare`.
- Oversampling and FFT objects are persistent.

Known semantics to call out in marketing
- `dynamicRangeDb = max(0, short-term LUFS − RMS dB)` is a pressure
  proxy, not a standards term (unlike LRA, which is).
- `stereoWidthPct` is a side/total energy ratio; not the same as the
  broadcast-engineering "stereo width" controls that pan by angle.

## Source/PluginProcessor.{h,cpp}

Thin. `processBlock` calls `engine.process(input, framePipe)`. State
I/O uses `copyState` / `replaceState`. No parameters yet (intentional
for a metering-only build).

## Source/PluginEditor.{h,cpp}

Fully custom-painted at 1440 × 920, 30 Hz repaint. Layout:

```
+--------------------------------------------------+
| MeterMaid header                                 |
+--------------+--------------------+--------------+
| Octagonal    | Decision pill      | Goniometer   |
| Target       | LUFS (M/S/I/LRA)   | Correlation  |
| Radar        | Peak/RMS table     | Ratios       |
+--------------+--------------------+--------------+
| Spectrum                  | Spectrogram          |
+--------------+------------+-----------+----------+
| Timeline     | Histogram  | Chroma    | Vectors  |
+--------------+------------+-----------+----------+
```

All zones are driven directly from the latest `MeterFrame` with a
1-pole smoothing pass on scalar readouts so the UI doesn't jitter.

## docs/RELEASE_GATES.md

Single release-authority document. Gate 1 (numerical trust) is
automated via `MeterMaid_ReferenceTest`. Gates 2-4 remain manual.

## tests/ReferenceToneTest.cpp

Console executable that synthesizes five calibration tones (1 kHz, 400
Hz, 2 kHz sines at −14, −18, −20, −23, −26 LUFS targets), drives them
through the live MeterEngine for 3.5 s, and asserts:

- Integrated / short-term / momentary LUFS within ±0.5 LU of target
- True peak within ±0.5 dBTP of 20·log10(peak amplitude)
- Correlation within ±0.05 of the analytical expectation (+1 for a
  mono source)

Exits non-zero on any deviation. Executed by CI before the macOS
universal-build job is allowed to run.

## Remaining release blockers (honest list)

1. DAW host matrix: Reaper, Ableton Live, Logic (AU), FL Studio,
   Cubase — load / save / automation.
2. RT / performance stress: 32-sample buffer at 192 kHz for 30 min,
   no audio-thread allocations, no dropouts.
3. Export / report layer — "save session loudness report as PDF/CSV".
4. Windows / Linux CI + binaries.
5. Code-signing / notarization on macOS.
