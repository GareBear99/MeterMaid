#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MeterMaidAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit MeterMaidAudioProcessorEditor(MeterMaidAudioProcessor&);
    ~MeterMaidAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::String decisionText(metermaid::DecisionState) const;
    juce::Colour decisionColour(metermaid::DecisionState) const;
    void pushHistory(float integrated);
    void drawPanel(juce::Graphics&, juce::Rectangle<float>, juce::String title);

    MeterMaidAudioProcessor& processor;
    metermaid::MeterFrame frame;
    std::array<float, metermaid::kTimelinePoints> history {};
    int historyWrite = 0;
    bool historyFilled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterMaidAudioProcessorEditor)
};
