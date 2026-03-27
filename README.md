# MeterMaid — v0.8

MeterMaid is a **single JUCE plugin mastering and metering suite** built around three pillars:

1. **Trust layer** — BS.1770-style loudness architecture, K-weighting, integrated gating, oversampled true-peak path.
2. **Decision layer** — rule-based guidance so the plugin says what is wrong, not just what the numbers are.
3. **Time layer** — a live LUFS timeline so the user can see how the track behaved over time.

## What changed in v0.7

- removed obvious audio-thread allocations from the main processing path
- replaced dynamic integrated-gate storage with a fixed-capacity gated block ring
- preallocated mono and weighted scratch buffers
- kept oversampling and FFT objects persistent after `prepareToPlay`
- upgraded the editor to include a real decision layer, vectorscope, spectrum, and LUFS timeline
- aligned repo messaging around **credible production baseline**, not fake-complete claims

## Included

- single-plugin JUCE repo scaffold
- K-weighted loudness engine
- momentary / short-term / integrated LUFS path
- oversampled true peak baseline
- RMS, stereo correlation, vectorscope, spectrum analyzer
- decision layer and streaming readiness logic
- LUFS timeline history in the editor
- source-of-truth HTML UI demo
- audit, validation, and release-gate docs

## Honest status

This is a **much stronger engineering baseline** than the earlier passes, but it is still **not honestly proven “best in world” yet**.

It still needs:

- numerical validation against trusted reference meters
- real DAW build/load testing on AU/VST3 targets
- hostile-case RT/perf verification
- export/report implementation
- host matrix verification

## Repo layout

- `Source/` — processor, editor, engine, frame pipe
- `assets/` — HTML demo
- `config/` — UI contract
- `docs/` — audit plan, gap analysis, release gates
- `tests/` — reference comparison notes
- `scripts/` — validation harness starter

## Current claim boundary

Safe claim:

> MeterMaid v0.7 is a serious single-plugin metering suite architecture with a better decision/time UX than most early metering projects.

Unsafe claim until validation is done:

> “best in world”

That claim requires proof, not intention.


## Honest status
MeterMaid v0.8 is a serious pre-production baseline. It is **not** yet truthfully release-ready until validation, host proof, and stress-gate evidence are attached.
