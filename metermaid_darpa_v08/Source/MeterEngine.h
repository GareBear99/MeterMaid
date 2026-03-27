#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include "MeterFrame.h"
#include "LockFreeFramePipe.h"

namespace metermaid
{
static constexpr float kMinDb = -96.0f;
static constexpr int kFftOrder = 11;
static constexpr int kFftSize = 1 << kFftOrder;
static constexpr int kMaxGatedBlocks = 16384;

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
            if (e > absGateMs)
            {
                absSum += e;
                ++absCount;
            }
        });

        if (absCount == 0)
            return kMinDb;

        const double ungatedMean = absSum / (double) absCount;
        const double ungatedLufs = -0.691 + 10.0 * std::log10(juce::jmax(ungatedMean, 1.0e-18));
        const double relGateLufs = ungatedLufs - 10.0;
        const double relGateMs = std::pow(10.0, (relGateLufs + 0.691) / 10.0);

        double relSum = 0.0;
        int relCount = 0;
        forEach([&](double e)
        {
            if (e > absGateMs && e > relGateMs)
            {
                relSum += e;
                ++relCount;
            }
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
        sampleRate = sr;
        maxBlockSize = juce::jmax(32, maxBlock);
        numChannels = juce::jmax(1, channels);

        kWeight.prepare(sampleRate, numChannels);
        gate.reset();

        weightedScratch.setSize(numChannels, maxBlockSize, false, false, true);
        monoScratch.setSize(1, maxBlockSize, false, false, true);
        momentaryRing.setSize(numChannels, juce::jmax(1, (int) std::ceil(sampleRate * 0.4)), false, false, true);
        shortTermRing.setSize(numChannels, juce::jmax(1, (int) std::ceil(sampleRate * 3.0)), false, false, true);
        momentaryRing.clear();
        shortTermRing.clear();
        momentaryWrite = shortWrite = 0;
        momentaryFilled = shortFilled = 0;

        oversampler = std::make_unique<juce::dsp::Oversampling<float>>(1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampler->initProcessing((size_t) maxBlockSize);
        oversampler->reset();

        fft = std::make_unique<juce::dsp::FFT>(kFftOrder);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(kFftSize, juce::dsp::WindowingFunction<float>::hann, true);
        fftFifo.fill(0.0f);
        fftData.fill(0.0f);
        fftIndex = 0;
        scopeWrite = 0;
        frame = {};
    }

    void process(const juce::AudioBuffer<float>& input, LockFreeFramePipe& pipe)
    {
        updateRms(input);
        updateCorrelation(input);
        updateTruePeak(input);
        updateLoudness(input);
        updateSpectrum(input);
        updateScope(input);
        updateDecisionLayer();
        pipe.push(frame);
    }

private:
    void updateRms(const juce::AudioBuffer<float>& input) noexcept
    {
        double sum = 0.0;
        const int total = input.getNumChannels() * input.getNumSamples();
        for (int ch = 0; ch < input.getNumChannels(); ++ch)
        {
            const float* d = input.getReadPointer(ch);
            for (int i = 0; i < input.getNumSamples(); ++i)
                sum += (double) d[i] * d[i];
        }
        const float rms = total > 0 ? (float) std::sqrt(sum / (double) total) : 0.0f;
        frame.rmsDb = gainToDbSafe(rms);
    }

    void updateCorrelation(const juce::AudioBuffer<float>& input) noexcept
    {
        if (input.getNumChannels() < 2 || input.getNumSamples() <= 0)
        {
            frame.correlation = 0.0f;
            return;
        }
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
        monoScratch.clear();
        const int n = input.getNumSamples();
        for (int ch = 0; ch < input.getNumChannels(); ++ch)
            monoScratch.addFrom(0, 0, input, ch, 0, n, 1.0f / (float) juce::jmax(1, input.getNumChannels()));

        juce::dsp::AudioBlock<float> monoBlock(monoScratch);
        auto upBlock = oversampler->processSamplesUp(monoBlock);
        const float* up = upBlock.getChannelPointer(0);
        float maxAbs = 0.0f;
        for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
            maxAbs = juce::jmax(maxAbs, std::abs(up[i]));
        frame.truePeakDbtp = gainToDbSafe(maxAbs);
    }

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
        pushRing(shortTermRing, shortWrite, shortFilled, weightedScratch);

        frame.momentaryLufs = windowLufs(momentaryRing, momentaryFilled);
        frame.shortTermLufs = windowLufs(shortTermRing, shortFilled);
        frame.integratedLufs = gate.integratedLufs();
        frame.dynamicRangeDb = juce::jmax(0.0f, frame.shortTermLufs - frame.rmsDb);
    }

    static void pushRing(juce::AudioBuffer<float>& ring, int& write, int& filled, const juce::AudioBuffer<float>& src) noexcept
    {
        const int len = ring.getNumSamples();
        const int n = src.getNumSamples();
        const int c = juce::jmin(ring.getNumChannels(), src.getNumChannels());
        for (int ch = 0; ch < c; ++ch)
        {
            const float* s = src.getReadPointer(ch);
            for (int i = 0; i < n; ++i)
                ring.setSample(ch, (write + i) % len, s[i]);
        }
        write = (write + n) % len;
        filled = juce::jmin(len, filled + n);
    }

    static float windowLufs(const juce::AudioBuffer<float>& buf, int filled) noexcept
    {
        if (filled <= 0)
            return kMinDb;
        double ms = 0.0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer(ch);
            for (int i = 0; i < filled; ++i)
                ms += (double) d[i] * d[i];
        }
        ms /= (double) juce::jmax(1, filled * buf.getNumChannels());
        return (float) (-0.691 + 10.0 * std::log10(juce::jmax(ms, 1.0e-18)));
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
                for (int iBin = 0; iBin < kSpectrumBins; ++iBin)
                    frame.spectrumDb[(size_t) iBin] = juce::jlimit(-96.0f, 6.0f, gainToDbSafe(fftData[(size_t) iBin] / (float) kFftSize));
                fftIndex = 0;
            }
        }
    }

    void updateScope(const juce::AudioBuffer<float>& input) noexcept
    {
        if (input.getNumChannels() < 2)
            return;
        const float* l = input.getReadPointer(0);
        const float* r = input.getReadPointer(1);
        const int step = juce::jmax(1, input.getNumSamples() / 64);
        for (int i = 0; i < input.getNumSamples(); i += step)
        {
            const float mid = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]);
            frame.scope[(size_t) scopeWrite] = { side, mid };
            scopeWrite = (scopeWrite + 1) % kScopePoints;
        }
    }

    void updateDecisionLayer() noexcept
    {
        frame.readinessState = 0;
        frame.decision = DecisionState::Neutral;

        if (frame.truePeakDbtp > -1.0f)
        {
            frame.readinessState = 2;
            frame.decision = DecisionState::PeakRisk;
            return;
        }

        if (frame.integratedLufs > -13.0f)
        {
            frame.readinessState = 2;
            frame.decision = DecisionState::TooLoud;
            return;
        }

        if (frame.integratedLufs < -16.0f)
        {
            frame.readinessState = 0;
            frame.decision = DecisionState::TooQuiet;
            return;
        }

        if (frame.correlation < -0.10f)
        {
            frame.readinessState = 0;
            frame.decision = DecisionState::StereoRisk;
            return;
        }

        if (frame.dynamicRangeDb < 6.0f)
        {
            frame.readinessState = 1;
            frame.decision = DecisionState::DynamicPressure;
            return;
        }

        frame.readinessState = 1;
        frame.decision = DecisionState::Ready;
    }

    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    int numChannels = 2;

    KWeighting kWeight;
    IntegratedGate gate;

    juce::AudioBuffer<float> weightedScratch;
    juce::AudioBuffer<float> monoScratch;
    juce::AudioBuffer<float> momentaryRing;
    juce::AudioBuffer<float> shortTermRing;
    int momentaryWrite = 0, shortWrite = 0;
    int momentaryFilled = 0, shortFilled = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    std::array<float, kFftSize> fftFifo {};
    std::array<float, kFftSize * 2> fftData {};
    int fftIndex = 0;
    int scopeWrite = 0;

    MeterFrame frame {};
};
}
