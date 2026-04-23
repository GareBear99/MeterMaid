// MeterMaid — Reference-tone calibration test
// ---------------------------------------------
// A self-contained console executable that drives the MeterMaid engine
// with synthesised known-energy signals and asserts every loudness /
// true-peak / correlation reading is within a published tolerance of
// the analytical expectation.
//
// This is the canonical "Gate 1 — numerical trust" harness from
// docs/RELEASE_GATES.md: it reports a deviation matrix and exits with
// a non-zero code if any signal fails.
//
// Build (enabled by default when -DMETERMAID_BUILD_TESTS=ON is passed):
//   cmake --build build --target MeterMaid_ReferenceTest -j
//   ./build/MeterMaid_ReferenceTest
//
// References: ITU-R BS.1770-4, EBU R128 / Tech 3342, Tech 3341
// (calibration tone: 1 kHz sine at -23 LUFS reads -23.0 LUFS ± 0.1 LU).

#include <JuceHeader.h>
#include "../Source/MeterEngine.h"
#include "../Source/MeterFrame.h"
#include "../Source/LockFreeFramePipe.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

namespace
{
constexpr double kSampleRate   = 48000.0;
constexpr int    kBlockSize    = 1024;
constexpr double kSecondsRamp  = 3.5;   // > 3 s so short-term window fills
constexpr double kLufsTolerance = 0.5;  // LU
constexpr double kTpTolerance   = 0.5;  // dB

struct Case
{
    const char* name;
    double      freqHz;          // 0 => pink noise (not used here; sine only)
    double      targetLufs;      // analytically-expected integrated LUFS
    double      expectedTruePeakDbtp; // analytically-expected true peak in dBTP
    double      expectedCorrelation;  // -1..+1 (mono = +1, out-of-phase = -1)
};

struct Result
{
    std::string name;
    float integrated = -96.0f;
    float momentary  = -96.0f;
    float shortTerm  = -96.0f;
    float truePeak   = -96.0f;
    float correlation = 0.0f;
    float rmsDb      = -96.0f;
    bool  passed = false;
};

// Convert LUFS target to the peak amplitude of a 1 kHz sine. For a full-
// bandwidth sine, BS.1770-4 says the K-weighted integrated loudness of a
// 1 kHz sine at peak amplitude A is:
//
//   Lk = -0.691 + 10 * log10(A^2 / 2)
//
// (The K-weighting filter has unity gain at 1 kHz.)
static inline float sineAmpForLufs(double targetLufs)
{
    // 10 * log10(A^2/2) = targetLufs + 0.691
    const double rhs = (targetLufs + 0.691) / 10.0;
    const double msq = std::pow(10.0, rhs);   // A^2 / 2
    return (float) std::sqrt(2.0 * msq);
}

static void fillSineStereoInPlace(juce::AudioBuffer<float>& buf, double sr, double freq, float amp, double& phase)
{
    const int n = buf.getNumSamples();
    const double inc = 2.0 * juce::MathConstants<double>::pi * freq / sr;
    for (int i = 0; i < n; ++i)
    {
        const float s = amp * (float) std::sin(phase);
        buf.setSample(0, i, s);
        if (buf.getNumChannels() > 1) buf.setSample(1, i, s); // mono on both channels
        phase += inc;
        if (phase > 2.0 * juce::MathConstants<double>::pi) phase -= 2.0 * juce::MathConstants<double>::pi;
    }
}

static Result runCase(const Case& c)
{
    Result r; r.name = c.name;

    metermaid::MeterEngine engine;
    LockFreeFramePipe pipe; // class lives in the global namespace by design
    engine.prepare(kSampleRate, kBlockSize, 2);

    const int  totalBlocks = (int) std::ceil(kSampleRate * kSecondsRamp / (double) kBlockSize);
    const float amp = sineAmpForLufs(c.targetLufs);
    juce::AudioBuffer<float> buf(2, kBlockSize);
    double phase = 0.0;

    for (int b = 0; b < totalBlocks; ++b)
    {
        fillSineStereoInPlace(buf, kSampleRate, c.freqHz, amp, phase);
        engine.process(buf, pipe);
    }

    metermaid::MeterFrame f {};
    pipe.pull(f);
    r.integrated = f.integratedLufs;
    r.momentary  = f.momentaryLufs;
    r.shortTerm  = f.shortTermLufs;
    r.truePeak   = f.truePeakDbtp;
    r.correlation = f.correlation;
    r.rmsDb      = f.rmsDb;

    const bool integratedOK  = std::abs(r.integrated - c.targetLufs)            <= kLufsTolerance;
    const bool shortOK       = std::abs(r.shortTerm  - c.targetLufs)            <= kLufsTolerance;
    const bool momOK         = std::abs(r.momentary  - c.targetLufs)            <= kLufsTolerance;
    const bool tpOK          = std::abs(r.truePeak   - c.expectedTruePeakDbtp)  <= kTpTolerance;
    const bool corrOK        = std::abs(r.correlation - c.expectedCorrelation)  <= 0.05f;
    r.passed = integratedOK && shortOK && momOK && tpOK && corrOK;
    return r;
}

static void printHeader()
{
    std::printf("%-30s  %10s  %10s  %10s  %10s  %10s  %10s  %6s\n",
                "case", "integ LUFS", "short LUFS", "mom LUFS", "TP dBTP", "corr", "RMS dB", "pass");
    std::printf("%-30s  %10s  %10s  %10s  %10s  %10s  %10s  %6s\n",
                "----", "----------", "----------", "--------", "-------", "----", "------", "----");
}

static void printRow(const Result& r, const Case& c)
{
    std::printf("%-30s  %+10.2f  %+10.2f  %+10.2f  %+10.2f  %+10.2f  %+10.2f  %6s\n",
                r.name.c_str(),
                r.integrated, r.shortTerm, r.momentary, r.truePeak, r.correlation, r.rmsDb,
                r.passed ? "PASS" : "FAIL");
    (void) c;
}
}

int main()
{
    // Peak amplitude of the test sine in dBFS == 20 * log10(A).
    // For a full-bandwidth unclipped sine the true peak equals the sample
    // peak, and equals amp in linear terms.
    auto expectedTp = [](double targetLufs)
    {
        const float amp = sineAmpForLufs(targetLufs);
        return (double) (amp > 0.0f ? 20.0f * std::log10(amp) : -96.0f);
    };

    // The analytical LUFS formula below assumes K-weighting has unity
    // gain at the test frequency. That is exactly true at 1 kHz (the
    // standard EBU Tech 3341 calibration frequency) and approximately
    // true within a few tenths of a dB near 400 Hz. We restrict the test
    // matrix to these two frequencies so the tolerance budget doesn't
    // get consumed by the K-weighting curve itself.
    const Case cases[] = {
        { "1 kHz sine @ -23 LUFS (EBU Tech 3341)",  1000.0, -23.0, expectedTp(-23.0), +1.0 },
        { "1 kHz sine @ -18 LUFS (broadcast hot)",  1000.0, -18.0, expectedTp(-18.0), +1.0 },
        { "1 kHz sine @ -14 LUFS (streaming-loud)", 1000.0, -14.0, expectedTp(-14.0), +1.0 },
        { "400 Hz sine @ -20 LUFS (low-mid)",        400.0, -20.0, expectedTp(-20.0), +1.0 },
    };

    printHeader();
    int failed = 0;
    for (const auto& c : cases)
    {
        const auto r = runCase(c);
        printRow(r, c);
        if (!r.passed) ++failed;
    }
    std::printf("\n");
    if (failed == 0)
    {
        std::printf("All %zu reference cases passed within \u00b10.5 LU / \u00b10.5 dBTP.\n", sizeof(cases) / sizeof(Case));
        return 0;
    }
    std::printf("%d / %zu reference cases FAILED. See deviations above.\n", failed, sizeof(cases) / sizeof(Case));
    return 1;
}
