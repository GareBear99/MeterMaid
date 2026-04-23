# Changelog

All notable changes to MeterMaid are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.7.0-beta.3] — 2026-04-23

### Added
- **Octagonal target radar** centre-piece visualiser: 6 concentric octagons,
  8 labelled spokes (Integrated, Short-Term, Momentary, RMS, Dynamics,
  Correlation, Headroom, True Peak), vertex-to-vertex diagonals, 48-tick
  perimeter ring, inner crosshair/diamond, and a filled live-metric
  polygon.
- **Loudness range (LRA)** per EBU R128 (P95 − P10 over short-term history
  gated to the absolute −70 LUFS threshold).
- **Per-channel true peak** via independent 4× oversampling chains for L,
  R, and summed bus.
- **Per-channel sample peak** and **per-channel RMS**.
- **PSR, PLR, and crest factor** derived ratios.
- **Stereo width %** from mid/side energy.
- **Goniometer** (raw L/R rotated 45°) alongside the existing side/mid
  vectorscope.
- **Spectrogram** (256-frame log-frequency heatmap).
- **Chromagram** (12 pitch classes across the piano range).
- **Loudness histogram** (−48..0 LUFS, 1 dB buckets).
- **`MeterMaid_ReferenceTest`** calibration target: synthesised
  known-energy tones (−18 and −23 LUFS sines) asserted against engine
  output within ±0.5 LU / ±0.5 dBTP of the analytical expectation.
- `.github/workflows/build.yml` — macos-14 CI running universal
  (arm64 + x86_64) VST3 + AU + Standalone build with `lipo -info`
  verification and release-asset attach on tag push.
- MIT `LICENSE`, `CHANGELOG.md`, and a polished public-facing `README.md`.

### Changed
- Project subdirectory renamed from `metermaid_darpa_v08` to `MeterMaid`
  so the repository layout reads cleanly in search results, GitHub
  browsers, and download URLs.
- Internal `FILE_BY_FILE_DARPA_AUDIT.md` renamed to `TECHNICAL_AUDIT.md`
  and refreshed for the v0.7 surface.
- `CMakeLists.txt` now accepts `-DJUCE_PATH=...` or `-DJUCE_DIR=...` for
  JUCE discovery and adds `JUCE_VST3_CAN_REPLACE_VST2=0` so the VST3
  module no longer requires the VST2 SDK headers.
- `juce_generate_juce_header(MeterMaid)` is now called so sources that
  `#include <JuceHeader.h>` resolve.

## [0.7.0] — 2025
- Initial public pre-production baseline. K-weighted loudness engine,
  momentary / short-term / integrated LUFS, oversampled true peak, RMS,
  stereo correlation, vectorscope, spectrum analyser, decision layer,
  LUFS timeline, HTML UI reference demo, audit / validation / release-gate
  documentation.
