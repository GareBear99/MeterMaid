#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <algorithm>
#include <vector>
#include "MeterFrame.h"
#include "LockFreeFramePipe.h"

namespace metermaid
{
static constexpr float kMinDb = -96.0f;
static constexpr int   kFftOrder = 11;
static constexpr int   kFftSize  = 1 << kFftOrder;
static constexpr int   kMaxGatedBlocks = 16384;

// Short-term loudness history used for LRA (10th / 95th percentile).
// 60 s at ~10 Hz update ≈ 600 samples; round up for headroom.
static constexpr int   kLraHistoryLen = 1024;

inline float gainToDbSafe(float g) noexcept
{
    return juce::Decibels::gainToDecibels(juce::jmax(g, 1.0e-12f), kMinDb);
}

inline float dbToGain(float db) noexcept
{
    return juce::Decibels::decibelsToGain(db);
}

class KWeighting
{
public:
    void prepare(double sampleRate, int channels)
    {
        juce::dsp::ProcessSpec spec { sampleRate, 4096, (juce::uint32) juce::jmax(1, channels) };
        highShelf.reset();
        highPass.reset();
        *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1681.974450955533f, 0.7071752369554196f, dbToGain(4.0f));
        *highPass.state  = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 38.13547087602444f, 0.5003270373238773f);
        highShelf.prepare(spec);
        highPass.prepare(spec);
    }

    void process(juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        highShelf.process(ctx);
        highPass.process(ctx);
    }

private:
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highShelf, highPass;
};

class IntegratedGate
{
public:
    void reset() noexcept
    {
        writeIndex = 0;
        count = 0;
        energies.fill(0.0);
    }

    void push(double meanSquare) noexcept
    {
        energies[(size_t) writeIndex] = meanSquare;
        writeIndex = (writeIndex + 1) % kMaxGatedBlocks;
        count = juce::jmin(count + 1, kMaxGatedBlocks);
    }

    float integratedLufs() const noexcept
    {
        if (count <= 0)
            return kMinDb;

        constexpr double absGateLufs = -70.0;
        const double absGateMs = std::pow(10.0, (absGateLufs + 0.691) / 10.0);

        double absSum = 0.0;
        int absCount = 0;
        forEach([&](double e)
        {
            if (e > absGateMs) { absSum += e; ++absCount; }
        });
        if (absCount == 0) return kMinDb;

        const double ungatedMean = absSum / (double) absCount;
        const double ungatedLufs = -0.691 + 10.0 * std::log10(juce::jmax(ungatedMean, 1.0e-18));
        const double relGateLufs = ungatedLufs - 10.0;
        const double relGateMs   = std::pow(10.0, (relGateLufs + 0.691) / 10.0);

        double relSum = 0.0;
        int relCount = 0;
        forEach([&](double e)
        {
            if (e > absGateMs && e > relGateMs) { relSum += e; ++relCount; }
        });

        const double finalMean = relCount > 0 ? (relSum / (double) relCount) : ungatedMean;
        return (float) (-0.691 + 10.0 * std::log10(juce::jmax(finalMean, 1.0e-18)));
    }

private:
    template <typename Fn>
    void forEach(Fn&& fn) const noexcept
    {
        const int start = count == kMaxGatedBlocks ? writeIndex : 0;
        for (int i = 0; i < count; ++i)
            fn(energies[(size_t) ((start + i) % kMaxGatedBlocks)]);
    }

    std::array<double, kMaxGatedBlocks> energies {};
    int writeIndex = 0;
    int count = 0;
};

class MeterEngine
{
public:
    void prepare(double sr, int maxBlock, int channels)
    {
        sampleRate  = sr;
        maxBlockSize = juce::jmax(32, maxBlock);
        numChannels  = juce::jmax(1, channels);

        kWeight.prepare(sampleRate, numChannels);
        gate.reset();

        weightedScratch.setSize(numChannels, maxBlockSize, false, false, true);
        monoScratch.setSize   (1, maxBlockSize, false, false, true);
        chanScratchL.setSize  (1, maxBlockSize, false, false, true);
        chanScratchR.setSize  (1, maxBlockSize, false, false, true);
        momentaryRing.setSize (numChannels, juce::jmax(1, (int) std::ceil(sampleRate * 0.4)), false, false, true);
        shortTermRing.setSize (numChannels, juce::jmax(1, (int) std::ceil(sampleRate * 3.0)), false, false, true);
        momentaryRing.clear();
        shortTermRing.clear();
        momentaryWrite = shortWrite = 0;
        momentaryFilled = shortFilled = 0;

        oversampler  = std::make_unique<juce::dsp::Oversampling<float>>(1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampler->initProcessing ((size_t) maxBlockSize);
        oversampler->reset();
        oversamplerL = std::make_unique<juce::dsp::Oversampling<float>>(1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversamplerL->initProcessing((size_t) maxBlockSize);
        oversamplerL->reset();
        oversamplerR = std::make_unique<juce::dsp::Oversampling<float>>(1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversamplerR->initProcessing((size_t) maxBlockSize);
        oversamplerR->reset();

        fft    = std::make_unique<juce::dsp::FFT>(kFftOrder);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(kFftSize, juce::dsp::WindowingFunction<float>::hann, true);
        fftFifo.fill(0.0f);
        fftData.fill(0.0f);
        fftIndex   = 0;
        scopeWrite = 0;
        gonioWrite = 0;

        lraHistory.assign(kLraHistoryLen, kMinDb);
        lraWrite  = 0;
        lraFilled = 0;

        histCounts.fill(0);
        histTotal = 0;

        buildLogBinMap();

        frame = {};
        frame.spectrogramWrite = 0;
    }

    void process(const juce::AudioBuffer<float>& input, LockFreeFramePipe& pipe)
    {
        updateSamplePeakAndRms(input);
        updateCorrelation(input);
        updateTruePeak(input);
        updateLoudness(input);
        updateStereoWidth(input);
        updateSpectrum(input);      // also feeds spectrogram + chroma
        updateScope(input);
        updateGoniometer(input);
        updateDynamicsAndRatios();
        updateLra();
        updateHistogram();
        updateDecisionLayer();
        pipe.push(frame);
    }

private:
    // ------------------------------------------------------------------
    // Peaks & RMS
    // ------------------------------------------------------------------
    void updateSamplePeakAndRms(const juce::AudioBuffer<float>& input) noexcept
    {
        const int n = input.getNumSamples();
        double sumSq = 0.0, sumSqL = 0.0, sumSqR = 0.0;
        float  peakL = 0.0f, peakR = 0.0f;

        if (n > 0)
        {
            const float* l = input.getReadPointer(0);
            const float* r = input.getNumChannels() > 1 ? input.getReadPointer(1) : l;
            for (int i = 0; i < n; ++i)
            {
                const float xl = l[i], xr = r[i];
                sumSqL += (double) xl * xl;
                sumSqR += (double) xr * xr;
                sumSq  += (double) xl * xl + (double) xr * xr;
                peakL   = juce::jmax(peakL, std::abs(xl));
                peakR   = juce::jmax(peakR, std::abs(xr));
            }
        }
        const int ch = juce::jmax(1, input.getNumChannels());
        frame.rmsLDb       = gainToDbSafe(n > 0 ? (float) std::sqrt(sumSqL / (double) n) : 0.0f);
        frame.rmsRDb       = gainToDbSafe(n > 0 ? (float) std::sqrt(sumSqR / (double) n) : 0.0f);
        frame.rmsDb        = gainToDbSafe(n > 0 ? (float) std::sqrt(sumSq  / (double) (n * ch)) : 0.0f);
        frame.samplePeakLDb = gainToDbSafe(peakL);
        frame.samplePeakRDb = gainToDbSafe(peakR);
    }

    void updateCorrelation(const juce::AudioBuffer<float>& input) noexcept
    {
        if (input.getNumChannels() < 2 || input.getNumSamples() <= 0) { frame.correlation = 0.0f; return; }
        double ll = 0.0, rr = 0.0, lr = 0.0;
        const float* l = input.getReadPointer(0);
        const float* r = input.getReadPointer(1);
        for (int i = 0; i < input.getNumSamples(); ++i)
        {
            ll += (double) l[i] * l[i];
            rr += (double) r[i] * r[i];
            lr += (double) l[i] * r[i];
        }
        const double denom = std::sqrt(juce::jmax(1.0e-18, ll * rr));
        frame.correlation = (float) juce::jlimit(-1.0, 1.0, lr / denom);
    }

    void updateTruePeak(const juce::AudioBuffer<float>& input) noexcept
    {
        // Summed (mono-mixed) true peak
        monoScratch.clear();
        const int n = input.getNumSamples();
        for (int ch = 0; ch < input.getNumChannels(); ++ch)
            monoScratch.addFrom(0, 0, input, ch, 0, n, 1.0f / (float) juce::jmax(1, input.getNumChannels()));
        frame.truePeakDbtp = oversampledPeakDb(*oversampler, monoScratch);

        // Per-channel true peak
        chanScratchL.clear();
        chanScratchL.copyFrom(0, 0, input, 0, 0, n);
        frame.truePeakLDbtp = oversampledPeakDb(*oversamplerL, chanScratchL);

        if (input.getNumChannels() > 1)
        {
            chanScratchR.clear();
            chanScratchR.copyFrom(0, 0, input, 1, 0, n);
            frame.truePeakRDbtp = oversampledPeakDb(*oversamplerR, chanScratchR);
        }
        else
        {
            frame.truePeakRDbtp = frame.truePeakLDbtp;
        }
    }

    static float oversampledPeakDb(juce::dsp::Oversampling<float>& os, juce::AudioBuffer<float>& buf) noexcept
    {
        juce::dsp::AudioBlock<float> block(buf);
        auto up = os.processSamplesUp(block);
        const float* d = up.getChannelPointer(0);
        float m = 0.0f;
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            m = juce::jmax(m, std::abs(d[i]));
        return gainToDbSafe(m);
    }

    // ------------------------------------------------------------------
    // Loudness + LRA
    // ------------------------------------------------------------------
    void updateLoudness(const juce::AudioBuffer<float>& input)
    {
        weightedScratch.makeCopyOf(input, true);
        kWeight.process(weightedScratch);

        double ms = 0.0;
        const int total = weightedScratch.getNumChannels() * weightedScratch.getNumSamples();
        for (int ch = 0; ch < weightedScratch.getNumChannels(); ++ch)
        {
            const float* d = weightedScratch.getReadPointer(ch);
            for (int i = 0; i < weightedScratch.getNumSamples(); ++i)
                ms += (double) d[i] * d[i];
        }
        ms /= (double) juce::jmax(1, total);
        gate.push(ms);

        pushRing(momentaryRing, momentaryWrite, momentaryFilled, weightedScratch);
        pushRing(shortTermRing, shortWrite,     shortFilled,     weightedScratch);

        frame.momentaryLufs  = windowLufs(momentaryRing, momentaryFilled);
        frame.shortTermLufs  = windowLufs(shortTermRing, shortFilled);
        frame.integratedLufs = gate.integratedLufs();
    }

    void updateLra() noexcept
    {
        // Keep a rolling history of short-term LUFS values gated to the
        // absolute -70 LUFS threshold. Report LRA = P95 - P10.
        if (frame.shortTermLufs > -70.0f)
        {
            lraHistory[(size_t) lraWrite] = frame.shortTermLufs;
            lraWrite  = (lraWrite + 1) % kLraHistoryLen;
            lraFilled = juce::jmin(kLraHistoryLen, lraFilled + 1);
        }
        if (lraFilled < 10) { frame.loudnessRangeLu = 0.0f; return; }
        std::vector<float> tmp;
        tmp.reserve(lraFilled);
        for (int i = 0; i < lraFilled; ++i) tmp.push_back(lraHistory[(size_t) i]);
        std::sort(tmp.begin(), tmp.end());
        const float p10 = tmp[(size_t) (lraFilled * 10 / 100)];
        const float p95 = tmp[(size_t) juce::jmin(lraFilled - 1, lraFilled * 95 / 100)];
        frame.loudnessRangeLu = juce::jmax(0.0f, p95 - p10);
    }

    static void pushRing(juce::AudioBuffer<float>& ring, int& write, int& filled, const juce::AudioBuffer<float>& src) noexcept
    {
        const int len = ring.getNumSamples();
        const int n   = src.getNumSamples();
        const int c   = juce::jmin(ring.getNumChannels(), src.getNumChannels());
        for (int ch = 0; ch < c; ++ch)
        {
            const float* s = src.getReadPointer(ch);
            for (int i = 0; i < n; ++i)
                ring.setSample(ch, (write + i) % len, s[i]);
        }
        write  = (write + n) % len;
        filled = juce::jmin(len, filled + n);
    }

    static float windowLufs(const juce::AudioBuffer<float>& buf, int filled) noexcept
    {
        if (filled <= 0) return kMinDb;
        double ms = 0.0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer(ch);
            for (int i = 0; i < filled; ++i) ms += (double) d[i] * d[i];
        }
        ms /= (double) juce::jmax(1, filled * buf.getNumChannels());
        return (float) (-0.691 + 10.0 * std::log10(juce::jmax(ms, 1.0e-18)));
    }

    // ------------------------------------------------------------------
    // Stereo width / scope / goniometer
    // ------------------------------------------------------------------
    void updateStereoWidth(const juce::AudioBuffer<float>& input) noexcept
    {
        if (input.getNumChannels() < 2 || input.getNumSamples() <= 0) { frame.stereoWidthPct = 0.0f; return; }
        const float* l = input.getReadPointer(0);
        const float* r = input.getReadPointer(1);
        double midSq = 0.0, sideSq = 0.0;
        for (int i = 0; i < input.getNumSamples(); ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            const float s = 0.5f * (l[i] - r[i]);
            midSq  += (double) m * m;
            sideSq += (double) s * s;
        }
        const double total = midSq + sideSq;
        frame.stereoWidthPct = total > 1.0e-18 ? (float) (100.0 * sideSq / total) : 0.0f;
    }

    void updateScope(const juce::AudioBuffer<float>& input) noexcept
    {
        if (input.getNumChannels() < 2) return;
        const float* l = input.getReadPointer(0);
        const float* r = input.getReadPointer(1);
        const int step = juce::jmax(1, input.getNumSamples() / 64);
        for (int i = 0; i < input.getNumSamples(); i += step)
        {
            const float mid  = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]);
            frame.scope[(size_t) scopeWrite] = { side, mid };
            scopeWrite = (scopeWrite + 1) % kScopePoints;
        }
    }

    void updateGoniometer(const juce::AudioBuffer<float>& input) noexcept
    {
        if (input.getNumChannels() < 2) return;
        const float* l = input.getReadPointer(0);
        const float* r = input.getReadPointer(1);
        const int step = juce::jmax(1, input.getNumSamples() / 128);
        const float inv = 1.0f / std::sqrt(2.0f);
        for (int i = 0; i < input.getNumSamples(); i += step)
        {
            // Rotate L/R 45° into goniometer axes.
            const float x = (l[i] - r[i]) * inv;
            const float y = (l[i] + r[i]) * inv;
            frame.gonio[(size_t) gonioWrite] = { x, y };
            gonioWrite = (gonioWrite + 1) % kGonioPoints;
        }
    }

    // ------------------------------------------------------------------
    // Spectrum / spectrogram / chroma
    // ------------------------------------------------------------------
    void buildLogBinMap()
    {
        // Precompute log-spaced slices from linear FFT bins into the
        // spectrogram's fixed-size frequency axis.
        logBinStart.assign(kSpectrogramBins, 0);
        logBinEnd.assign  (kSpectrogramBins, 0);
        const int totalBins = kFftSize / 2;
        const float minLog = std::log10(20.0f);
        const float maxLog = std::log10(juce::jmax(21.0f, (float) (sampleRate * 0.5)));
        for (int i = 0; i < kSpectrogramBins; ++i)
        {
            const float lo = std::pow(10.0f, juce::jmap((float) i       / kSpectrogramBins, minLog, maxLog));
            const float hi = std::pow(10.0f, juce::jmap((float) (i + 1) / kSpectrogramBins, minLog, maxLog));
            logBinStart[(size_t) i] = juce::jlimit(0, totalBins - 1, (int) (lo / (sampleRate * 0.5) * totalBins));
            logBinEnd  [(size_t) i] = juce::jlimit(logBinStart[(size_t) i] + 1, totalBins, (int) (hi / (sampleRate * 0.5) * totalBins));
        }
    }

    void updateSpectrum(const juce::AudioBuffer<float>& input)
    {
        const float* l = input.getReadPointer(0);
        const float* r = input.getNumChannels() > 1 ? input.getReadPointer(1) : nullptr;
        for (int i = 0; i < input.getNumSamples(); ++i)
        {
            fftFifo[(size_t) fftIndex++] = r ? 0.5f * (l[i] + r[i]) : l[i];
            if (fftIndex == kFftSize)
            {
                std::copy(fftFifo.begin(), fftFifo.end(), fftData.begin());
                window->multiplyWithWindowingTable(fftData.data(), kFftSize);
                std::fill(fftData.begin() + kFftSize, fftData.end(), 0.0f);
                fft->performFrequencyOnlyForwardTransform(fftData.data());

                // Live spectrum (linear bins)
                for (int iBin = 0; iBin < kSpectrumBins; ++iBin)
                    frame.spectrumDb[(size_t) iBin] = juce::jlimit(-96.0f, 6.0f, gainToDbSafe(fftData[(size_t) iBin] / (float) kFftSize));

                // Spectrogram (log-binned column)
                auto& col = frame.spectrogram[(size_t) frame.spectrogramWrite];
                for (int b = 0; b < kSpectrogramBins; ++b)
                {
                    float m = 0.0f;
                    const int s = logBinStart[(size_t) b];
                    const int e = logBinEnd  [(size_t) b];
                    for (int k = s; k < e; ++k)
                        m = juce::jmax(m, fftData[(size_t) k]);
                    col[(size_t) b] = juce::jlimit(-96.0f, 6.0f, gainToDbSafe(m / (float) kFftSize));
                }
                frame.spectrogramWrite = (frame.spectrogramWrite + 1) % kSpectrogramFrames;

                // Chromagram (sum magnitude by pitch class)
                std::array<float, kChromaBins> chromaAcc {};
                for (int k = 1; k < kFftSize / 2; ++k)
                {
                    const float hz = (float) k * (float) sampleRate / (float) kFftSize;
                    if (hz < 27.5f || hz > 5000.0f) continue; // piano range
                    const float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
                    const int pc = ((int) std::round(midi)) % 12;
                    chromaAcc[(size_t) ((pc + 12) % 12)] += fftData[(size_t) k];
                }
                float chromaMax = 1.0e-9f;
                for (auto v : chromaAcc) chromaMax = juce::jmax(chromaMax, v);
                for (int i = 0; i < kChromaBins; ++i) frame.chroma[(size_t) i] = chromaAcc[(size_t) i] / chromaMax;

                fftIndex = 0;
            }
        }
    }

    // ------------------------------------------------------------------
    // Dynamics / histogram
    // ------------------------------------------------------------------
    void updateDynamicsAndRatios() noexcept
    {
        frame.crestFactorDb  = juce::jmax(0.0f, frame.truePeakDbtp - frame.rmsDb);
        frame.dynamicRangeDb = juce::jmax(0.0f, frame.shortTermLufs - frame.rmsDb);
        frame.psrDb          = frame.truePeakDbtp - frame.shortTermLufs;
        frame.plrDb          = frame.truePeakDbtp - frame.integratedLufs;
    }

    void updateHistogram() noexcept
    {
        // Bucket the current momentary LUFS value into a -48..0 dB histogram.
        if (frame.momentaryLufs <= -48.0f) return;
        const int bin = juce::jlimit(0, kHistogramBins - 1,
                                     (int) (((frame.momentaryLufs + 48.0f) / 48.0f) * kHistogramBins));
        histCounts[(size_t) bin]++;
        ++histTotal;
        const float maxCount = (float) *std::max_element(histCounts.begin(), histCounts.end());
        for (int i = 0; i < kHistogramBins; ++i)
            frame.lufsHistogram[(size_t) i] = maxCount > 0.0f ? (float) histCounts[(size_t) i] / maxCount : 0.0f;
    }

    // ------------------------------------------------------------------
    // Decision layer
    // ------------------------------------------------------------------
    void updateDecisionLayer() noexcept
    {
        frame.readinessState = 0;
        frame.decision = DecisionState::Neutral;

        if (frame.truePeakDbtp > -1.0f)   { frame.readinessState = 2; frame.decision = DecisionState::PeakRisk;       return; }
        if (frame.integratedLufs > -13.0f){ frame.readinessState = 2; frame.decision = DecisionState::TooLoud;        return; }
        if (frame.integratedLufs < -16.0f){ frame.readinessState = 0; frame.decision = DecisionState::TooQuiet;       return; }
        if (frame.correlation < -0.10f)   { frame.readinessState = 0; frame.decision = DecisionState::StereoRisk;     return; }
        if (frame.dynamicRangeDb < 6.0f)  { frame.readinessState = 1; frame.decision = DecisionState::DynamicPressure;return; }
        frame.readinessState = 1;
        frame.decision = DecisionState::Ready;
    }

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------
    double sampleRate = 44100.0;
    int maxBlockSize  = 512;
    int numChannels   = 2;

    KWeighting kWeight;
    IntegratedGate gate;

    juce::AudioBuffer<float> weightedScratch, monoScratch, chanScratchL, chanScratchR;
    juce::AudioBuffer<float> momentaryRing, shortTermRing;
    int momentaryWrite = 0, shortWrite = 0;
    int momentaryFilled = 0, shortFilled = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler, oversamplerL, oversamplerR;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::array<float, kFftSize>     fftFifo {};
    std::array<float, kFftSize * 2> fftData {};
    int fftIndex    = 0;
    int scopeWrite  = 0;
    int gonioWrite  = 0;

    std::vector<int> logBinStart, logBinEnd;

    std::vector<float> lraHistory;
    int lraWrite = 0, lraFilled = 0;

    std::array<int, kHistogramBins> histCounts {};
    int histTotal = 0;

    MeterFrame frame {};
};
}
