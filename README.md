<!-- tizwildin-pricing:start -->
## 💎 MeterMaid — **free** on GitHub + TizWildin website

MeterMaid is released **free** as part of the TizWildin Plugin Ecosystem.

- [⬇️ Latest release](https://github.com/GareBear99/MeterMaid/releases/latest) — universal macOS VST3 + AU + Standalone
- [🌐 TizWildin website](https://garebear99.github.io/TizWildinEntertainmentHUB/)
- [🏛️ TizWildin Hub repo](https://github.com/GareBear99/TizWildinEntertainmentHUB)

Want to support the work directly? Any of these are appreciated:

- [GitHub Sponsors](https://github.com/sponsors/GareBear99)
- [Buy Me a Coffee](https://buymeacoffee.com/garebear99)
- [Ko-fi](https://ko-fi.com/luciferai)
<!-- tizwildin-pricing:end -->

> 🎛️ Part of the [TizWildin Plugin Ecosystem](https://garebear99.github.io/TizWildinEntertainmentHUB/) — 17 free audio plugins with a live update dashboard.
>
> [FreeEQ8](https://github.com/GareBear99/FreeEQ8) · [XyloCore](https://github.com/GareBear99/XyloCore) · [Instrudio](https://github.com/GareBear99/Instrudio) · [Therum](https://github.com/GareBear99/Therum_JUCE-Plugin) · [BassMaid](https://github.com/GareBear99/BassMaid) · [SpaceMaid](https://github.com/GareBear99/SpaceMaid) · [GlueMaid](https://github.com/GareBear99/GlueMaid) · [MixMaid](https://github.com/GareBear99/MixMaid) · [MultiMaid](https://github.com/GareBear99/MultiMaid) · [MeterMaid](https://github.com/GareBear99/MeterMaid) · [ChainMaid](https://github.com/GareBear99/ChainMaid) · [PaintMask](https://github.com/GareBear99/PaintMask_Free-JUCE-Plugin) · [WURP](https://github.com/GareBear99/WURP_Toxic-Motion-Engine_JUCE) · [AETHER](https://github.com/GareBear99/AETHER_Choir-Atmosphere-Designer) · [WhisperGate](https://github.com/GareBear99/WhisperGate_Free-JUCE-Plugin) · [RiftWave](https://github.com/GareBear99/RiftWaveSuite_RiftSynth_WaveForm_Lite) · [FreeSampler](https://github.com/GareBear99/FreeSampler_v0.3) · [VF-PlexLab](https://github.com/GareBear99/VF-PlexLab) · [PAP-Forge-Audio](https://github.com/GareBear99/PAP-Forge-Audio)
>
> 🎁 [Free Packs & Samples](#tizwildin-free-sample-packs) — jump to free packs & samples
>
> 🎵 [Awesome Audio](https://github.com/GareBear99/awesome-audio-plugins-dev) — (FREE) Awesome Audio Dev List

# MeterMaid

**A single-plugin mastering and metering suite built on rigorous loudness standards.**
ITU-R BS.1770-4 K-weighted loudness · EBU R128 gated integration · 4× oversampled true peak · real-time spectrum · spectrogram · chromagram · goniometer · phase correlation · octagonal target radar.

[![CI](https://github.com/GareBear99/MeterMaid/actions/workflows/build.yml/badge.svg)](https://github.com/GareBear99/MeterMaid/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platforms](https://img.shields.io/badge/platforms-macOS%20universal-lightgrey.svg)](#install)

## What MeterMaid measures

MeterMaid gives you every reading a modern mastering engineer or streaming-target engineer needs from a single plugin window. Every metric below is computed from the same audio block on the audio thread and published lock-free to the UI.

### Loudness (ITU-R BS.1770-4 / EBU R128)
- **Momentary LUFS** (400 ms sliding window)
- **Short-Term LUFS** (3 s sliding window)
- **Integrated LUFS** with absolute (-70 LUFS) and relative (-10 LU) gating
- **LRA** (Loudness Range, EBU R128 P95 − P10)

### Peaks and RMS
- **True Peak** (4× oversampled, IIR half-band): summed bus, and per-channel L / R
- **Sample Peak**: per-channel L / R
- **RMS**: per-channel L / R and summed mono

### Dynamics and ratios
- **Crest Factor** (true peak − RMS)
- **Dynamic Range** (short-term LUFS − RMS)
- **PSR** (Peak-to-Short-term Ratio, dB)
- **PLR** (Peak-to-Loudness Ratio, dB)

### Stereo image
- **Phase correlation** (−1..+1)
- **Stereo width** (side / total energy, %)
- **Goniometer** (L/R rotated 45°)
- **Vectorscope** (side / mid)

### Spectral
- **Spectrum analyzer** (512-bin Hann-windowed FFT, linear frequency axis)
- **Spectrogram** (log-frequency, viridis heatmap, 256-frame history)
- **Chromagram** (12 pitch classes over the piano range)

### Time-based
- **Integrated LUFS timeline** (512-point rolling history)
- **Loudness histogram** (−48..0 LUFS in 1 dB buckets)

### Decision layer
Rule-based target guidance that categorises the current material as one of:

| State | Meaning |
|---|---|
| `Streaming Ready` | Integrated −16..−13 LUFS and true peak ≤ −1 dBTP |
| `Too Loud` | Integrated > −13 LUFS |
| `Too Quiet` | Integrated < −16 LUFS |
| `True Peak Risk` | True peak > −1.0 dBTP |
| `Stereo Risk` | Correlation < −0.10 |
| `Dynamic Pressure` | Dynamic range < 6 dB |

### The octagonal radar
The centrepiece visualisation. Eight labelled spokes — Integrated, Short-Term, Momentary, RMS, Dynamics, Correlation, Headroom, True Peak — each normalised to 0..1 against its target range. A filled polygon connects the live values so an at-risk mix shows an obvious asymmetry before you read a single number.

## Install

1. Grab the latest release from [the releases page](https://github.com/GareBear99/MeterMaid/releases/latest).
2. Unzip and copy:
   - `MeterMaid.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
   - `MeterMaid.component` → `~/Library/Audio/Plug-Ins/Components/`
3. Re-scan plugins in your DAW.

Standalone: `MeterMaid.app` — double-click to run.

*Windows / Linux builds — planned. The CMake project is portable; CI currently targets macOS universal.*

## Standards compliance

MeterMaid implements loudness measurement against these published specifications:

- **ITU-R BS.1770-4** — pre-filter (high-shelf + high-pass K-weighting), mean square, gated block ring, logarithmic conversion.
- **EBU R128 / Tech 3342** — momentary (400 ms), short-term (3 s), integrated with absolute (−70 LUFS) and relative (−10 LU) gates; LRA as P95 − P10 of gated short-term values.
- **ITU-R BS.1770-4 Annex 2 (true peak)** — 4× polyphase IIR half-band oversampling before the peak detector.

The K-weighting filter coefficients are the specification values (1681.97 Hz high-shelf at +4 dB, 38.14 Hz high-pass Q=0.5003) implemented as `juce::dsp::IIR::Filter` pairs, one per channel.

## Validation status

This is a beta build. Numerical validation against reference meters is in progress; see [`MeterMaid/docs/TECHNICAL_AUDIT.md`](MeterMaid/docs/TECHNICAL_AUDIT.md) for the current audit and [`MeterMaid/docs/RELEASE_GATES.md`](MeterMaid/docs/RELEASE_GATES.md) for the full release gate checklist. The repository now ships an in-process calibration test (`MeterMaid/tests/ReferenceToneTest.cpp`) that synthesizes known-energy tones and asserts each reading is within ±0.5 LU / ±0.5 dBTP of the analytical expectation.

**Honest claim:** serious pre-production baseline with every MiniMeters-class readout implemented against the published standards, and an octagonal target radar that no other free metering plugin currently offers.

**Unsafe claim without proof:** "best in world" — that requires the validation matrix filled end-to-end and attached to a release.

## Build from source

Requirements:
- CMake 3.22+
- A C++20 compiler (Xcode 14+ / CLT 14+ on macOS, MSVC 2022 on Windows, GCC 11+ on Linux)
- [JUCE 8.0.0](https://github.com/juce-framework/JUCE/tree/8.0.0)

```bash path=null start=null
git clone https://github.com/GareBear99/MeterMaid.git
cd MeterMaid
git clone --depth 1 --branch 8.0.0 https://github.com/juce-framework/JUCE.git JUCE

cmake -S MeterMaid -B build \
  -DJUCE_PATH=$PWD/JUCE \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

cmake --build build --config Release --target MeterMaid_VST3 -j
cmake --build build --config Release --target MeterMaid_AU -j
cmake --build build --config Release --target MeterMaid_Standalone -j
```

Universal binaries land in `build/MeterMaid_artefacts/Release/{VST3,AU,Standalone}/`.

Run the calibration tests:

```bash path=null start=null
cmake -S MeterMaid -B build -DJUCE_PATH=$PWD/JUCE -DMETERMAID_BUILD_TESTS=ON
cmake --build build --target MeterMaid_ReferenceTest -j
./build/MeterMaid_ReferenceTest
```

## Repository layout

```
MeterMaid/
├── CMakeLists.txt
├── Source/
│   ├── PluginProcessor.{h,cpp}   — audio processor + framePipe owner
│   ├── PluginEditor.{h,cpp}      — octagonal radar + full dashboard
│   ├── MeterEngine.h             — K-weighting, gate, true peak, spectrum, chroma, LRA
│   ├── MeterFrame.h              — audio→UI payload
│   └── LockFreeFramePipe.h       — double-buffered SPSC latest-frame pipe
├── assets/                       — HTML UI reference demo
├── config/                       — UI contract
├── docs/                         — technical audit, release gates, audit plan
├── tests/                        — reference tones + validation matrix
└── scripts/                      — validation harness
.github/workflows/build.yml       — macos-14 CI, universal-binary build + release attach
```

## License

MIT — see [LICENSE](LICENSE). The K-weighting coefficients and gating procedure are from publicly published ITU / EBU specifications and are implemented from scratch.

## Support and ecosystem

- **Report issues**: [GitHub Issues](https://github.com/GareBear99/MeterMaid/issues)
- **Changelog**: [CHANGELOG.md](CHANGELOG.md)
- **Other TizWildin plugins**: [TizWildin Hub](https://garebear99.github.io/TizWildinEntertainmentHUB/)
- **Awesome Audio Dev list**: [awesome-audio-plugins-dev](https://github.com/GareBear99/awesome-audio-plugins-dev)
