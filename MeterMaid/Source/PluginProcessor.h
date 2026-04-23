#pragma once
#include <JuceHeader.h>
#include "MeterEngine.h"

class MeterMaidAudioProcessor : public juce::AudioProcessor
{
public:
    MeterMaidAudioProcessor();
    ~MeterMaidAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MeterMaid"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    LockFreeFramePipe& getFramePipe() noexcept { return framePipe; }

private:
    metermaid::MeterEngine engine;
    LockFreeFramePipe framePipe;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterMaidAudioProcessor)
};
