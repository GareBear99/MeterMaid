#include "PluginProcessor.h"
#include "PluginEditor.h"

MeterMaidAudioProcessor::MeterMaidAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MeterMaidAudioProcessor::prepareToPlay(double sr, int spb)
{
    engine.prepare(sr, spb, getTotalNumInputChannels());
}

void MeterMaidAudioProcessor::releaseResources() {}

bool MeterMaidAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MeterMaidAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
    engine.process(buffer, framePipe);
}

juce::AudioProcessorEditor* MeterMaidAudioProcessor::createEditor()
{
    return new MeterMaidAudioProcessorEditor(*this);
}

void MeterMaidAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::MemoryOutputStream stream(dest, false);
    stream.writeInt(1);
}

void MeterMaidAudioProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MeterMaidAudioProcessor();
}
