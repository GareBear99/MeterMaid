# MeterMaid v0.7 — Audit and Execution Plan

## Files reviewed in this pass

- `CMakeLists.txt`
- `Source/MeterFrame.h`
- `Source/LockFreeFramePipe.h`
- `Source/MeterEngine.h`
- `Source/PluginProcessor.h`
- `Source/PluginProcessor.cpp`
- `Source/PluginEditor.h`
- `Source/PluginEditor.cpp`
- `assets/MeterMaid_UI_Demo.html`
- `config/ui_contract.json`
- `tests/reference_notes.md`
- `scripts/validate_plan.py`

## What was corrected from earlier passes

1. **Audio-thread allocation drift**
   - Earlier versions still had clearly avoidable dynamic behavior in the processing path.
   - v0.7 preallocates the main scratch buffers and keeps FFT/oversampling objects persistent.

2. **Integrated loudness storage policy**
   - Earlier versions used dynamic containers for gated block storage.
   - v0.7 uses a fixed-capacity ring for gated mean-square blocks.

3. **Editor completeness**
   - Earlier versions described a timeline and decision layer more strongly than the editor actually expressed.
   - v0.7 now includes a visible decision panel and LUFS history path.

## Remaining blockers to release-grade trust

### 1. Numerical validation
Required before launch:
- compare momentary / short-term / integrated LUFS against trusted references
- compare true peak against trusted references
- document deviation envelopes

### 2. Host testing
Required before launch:
- Reaper
- Ableton Live
- FL Studio
- Logic Pro (AU)

### 3. RT/perf validation
Required before launch:
- 44.1 / 48 / 96 / 192 kHz
- 32 / 64 / 128 / 512 / 2048 buffers
- 30+ minute session stability

### 4. UX finish layer
Required before launch:
- resize handling
- timeline labels / ruler
- peak hold markings
- export/report panel

## Release gate

Do not call MeterMaid “best in world” until all four are complete:
- validated numbers
- host stability
- perf stability
- finished UI/UX layer
