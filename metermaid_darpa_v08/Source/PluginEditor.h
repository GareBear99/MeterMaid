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
    void pushHistory(float integrated);

    // Zone drawers
    void drawHeader            (juce::Graphics&, juce::Rectangle<float>);
    void drawOctagonRadar      (juce::Graphics&, juce::Rectangle<float>);
    void drawDecisionCluster   (juce::Graphics&, juce::Rectangle<float>);
    void drawLoudnessReadouts  (juce::Graphics&, juce::Rectangle<float>);
    void drawPeakRmsTable      (juce::Graphics&, juce::Rectangle<float>);
    void drawRatiosPanel       (juce::Graphics&, juce::Rectangle<float>);
    void drawCorrelationBar    (juce::Graphics&, juce::Rectangle<float>);
    void drawGoniometer        (juce::Graphics&, juce::Rectangle<float>);
    void drawVectorscope       (juce::Graphics&, juce::Rectangle<float>);
    void drawSpectrum          (juce::Graphics&, juce::Rectangle<float>);
    void drawSpectrogram       (juce::Graphics&, juce::Rectangle<float>);
    void drawTimeline          (juce::Graphics&, juce::Rectangle<float>);
    void drawHistogram         (juce::Graphics&, juce::Rectangle<float>);
    void drawChromagram        (juce::Graphics&, juce::Rectangle<float>);
    void drawPanelChrome       (juce::Graphics&, juce::Rectangle<float>, const juce::String& title);

    juce::String decisionText(metermaid::DecisionState) const;
    juce::Colour decisionColour(metermaid::DecisionState) const;
    static juce::Colour heatmapColour(float normalised);

    MeterMaidAudioProcessor& processor;
    metermaid::MeterFrame frame;
    std::array<float, metermaid::kTimelinePoints> history {};
    int  historyWrite = 0;
    bool historyFilled = false;

    // Smoothed versions of scalar metrics for UI animation (audio-thread-independent).
    float smMomentary = -96.0f, smShort = -96.0f, smIntegrated = -96.0f;
    float smTruePeak = -96.0f,  smCorrelation = 0.0f, smWidth = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterMaidAudioProcessorEditor)
};
