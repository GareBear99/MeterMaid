#pragma once
#include <array>
#include <cstdint>

namespace metermaid
{
// Visualisation dimensions — sized to match MiniMeters-class depth
// without exploding the frame size on the SPSC pipe.
static constexpr int kScopePoints        = 256;    // vectorscope (side/mid)
static constexpr int kGonioPoints        = 512;    // raw L/R goniometer
static constexpr int kSpectrumBins       = 512;    // live FFT magnitude
static constexpr int kSpectrogramFrames  = 256;    // spectrogram time axis
static constexpr int kSpectrogramBins    = 128;    // spectrogram frequency axis (log-binned)
static constexpr int kTimelinePoints     = 512;    // integrated LUFS history
static constexpr int kHistogramBins      = 48;     // LUFS histogram (-48..0 LUFS in 1 dB)
static constexpr int kChromaBins         = 12;     // pitch classes C..B

enum class DecisionState : uint8_t
{
    Neutral = 0,
    Ready,
    TooQuiet,
    TooLoud,
    PeakRisk,
    StereoRisk,
    DynamicPressure
};

struct ScopePoint
{
    float x = 0.0f;
    float y = 0.0f;
};

struct MeterFrame
{
    // --- Loudness ---
    float momentaryLufs    = -96.0f;
    float shortTermLufs    = -96.0f;
    float integratedLufs   = -96.0f;
    float loudnessRangeLu  = 0.0f;   // LRA (EBU R128)

    // --- Peaks & RMS (summed + per-channel) ---
    float truePeakDbtp     = -96.0f; // max of L/R over upsampled mono-sum
    float truePeakLDbtp    = -96.0f;
    float truePeakRDbtp    = -96.0f;
    float samplePeakLDb    = -96.0f;
    float samplePeakRDb    = -96.0f;
    float rmsDb            = -96.0f; // summed mono
    float rmsLDb           = -96.0f;
    float rmsRDb           = -96.0f;

    // --- Dynamics / ratios ---
    float crestFactorDb    = 0.0f;   // true peak - rms
    float dynamicRangeDb   = 0.0f;   // short-term - rms
    float psrDb            = 0.0f;   // true peak - short-term
    float plrDb            = 0.0f;   // true peak - integrated

    // --- Stereo image ---
    float correlation      = 0.0f;   // -1..+1
    float stereoWidthPct   = 0.0f;   // side / (side + mid) * 100

    // --- Decision layer ---
    int readinessState     = 0;
    DecisionState decision = DecisionState::Neutral;

    // --- Visualisation arrays ---
    std::array<float, kSpectrumBins>      spectrumDb   {};
    std::array<ScopePoint, kScopePoints>  scope        {}; // side / mid
    std::array<ScopePoint, kGonioPoints>  gonio        {}; // raw L / R
    std::array<float, kChromaBins>        chroma       {};
    std::array<float, kHistogramBins>     lufsHistogram{}; // -48..0 LUFS, normalised 0..1

    // Spectrogram ring buffer: frame (spectrogramWrite - 1) mod N is newest.
    std::array<std::array<float, kSpectrogramBins>, kSpectrogramFrames> spectrogram {};
    int spectrogramWrite = 0;
};
}
