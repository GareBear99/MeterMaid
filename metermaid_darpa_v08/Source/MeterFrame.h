#pragma once
#include <array>
#include <cstdint>

namespace metermaid
{
static constexpr int kScopePoints = 256;
static constexpr int kSpectrumBins = 512;
static constexpr int kTimelinePoints = 512;

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
    float momentaryLufs = -96.0f;
    float shortTermLufs = -96.0f;
    float integratedLufs = -96.0f;
    float truePeakDbtp = -96.0f;
    float rmsDb = -96.0f;
    float correlation = 0.0f;
    float dynamicRangeDb = 0.0f;
    int readinessState = 0;
    DecisionState decision = DecisionState::Neutral;

    std::array<float, kSpectrumBins> spectrumDb {};
    std::array<ScopePoint, kScopePoints> scope {};
};
}
