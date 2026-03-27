#include "PluginEditor.h"

using namespace juce;

MeterMaidAudioProcessorEditor::MeterMaidAudioProcessorEditor(MeterMaidAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    history.fill(-96.0f);
    setSize(1280, 800);
    startTimerHz(30);
}

void MeterMaidAudioProcessorEditor::resized() {} // custom-painted layout; keep deterministic until component layer is introduced

void MeterMaidAudioProcessorEditor::timerCallback()
{
    if (processor.getFramePipe().pull(frame))
        pushHistory(frame.integratedLufs);
    repaint();
}

void MeterMaidAudioProcessorEditor::pushHistory(float integrated)
{
    history[(size_t) historyWrite] = integrated;
    historyWrite = (historyWrite + 1) % metermaid::kTimelinePoints;
    if (historyWrite == 0)
        historyFilled = true;
}

String MeterMaidAudioProcessorEditor::decisionText(metermaid::DecisionState state) const
{
    using DS = metermaid::DecisionState;
    switch (state)
    {
        case DS::Ready: return "Streaming ready";
        case DS::TooQuiet: return "Too quiet — raise gain";
        case DS::TooLoud: return "Too loud — reduce loudness";
        case DS::PeakRisk: return "True peak risk — lower ceiling";
        case DS::StereoRisk: return "Stereo risk — mono compatibility warning";
        case DS::DynamicPressure: return "Dynamics pressure — over-compressed feel";
        default: return "Monitoring";
    }
}

Colour MeterMaidAudioProcessorEditor::decisionColour(metermaid::DecisionState state) const
{
    using DS = metermaid::DecisionState;
    switch (state)
    {
        case DS::Ready: return Colour::fromRGB(120, 232, 140);
        case DS::DynamicPressure: return Colour::fromRGB(255, 205, 90);
        case DS::TooQuiet: return Colour::fromRGB(120, 180, 255);
        case DS::TooLoud:
        case DS::PeakRisk:
        case DS::StereoRisk: return Colour::fromRGB(255, 112, 112);
        default: return Colour::fromRGB(215, 183, 110);
    }
}

void MeterMaidAudioProcessorEditor::drawPanel(Graphics& g, Rectangle<float> r, String title)
{
    g.setColour(Colour::fromRGBA(20, 26, 36, 236));
    g.fillRoundedRectangle(r, 22.0f);
    g.setColour(Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(r.reduced(0.5f), 22.0f, 1.0f);
    g.setColour(Colour::fromRGB(255, 232, 170));
    g.setFont(FontOptions(16.0f, Font::bold));
    g.drawText(title, r.reduced(16.0f).removeFromTop(24.0f).toNearestInt(), Justification::centredLeft);
}

void MeterMaidAudioProcessorEditor::paint(Graphics& g)
{
    g.fillAll(Colour::fromRGB(8, 11, 17));

    auto bounds = getLocalBounds().toFloat().reduced(16.0f);
    auto header = bounds.removeFromTop(72.0f);
    drawPanel(g, header, "MeterMaid — single-plugin mastering and metering suite");
    g.setColour(Colour::fromRGB(214, 181, 106));
    g.setFont(FontOptions(30.0f, Font::bold));
    g.drawText("MeterMaid", header.reduced(16.0f).removeFromTop(34.0f).toNearestInt(), Justification::centredLeft);

    auto top = bounds.removeFromTop(200.0f);
    auto left = top.removeFromLeft(top.getWidth() * 0.46f);
    auto right = top;

    drawPanel(g, left, "Primary Loudness");
    auto lr = left.reduced(16.0f).withTrimmedTop(28.0f);
    auto a = lr.removeFromTop(64.0f);
    auto b = lr.removeFromTop(64.0f);
    auto c = lr.removeFromTop(64.0f);
    g.setColour(Colours::lightgrey); g.setFont(FontOptions(14.0f, Font::bold));
    g.drawText("Momentary", a.removeFromLeft(150.0f).toNearestInt(), Justification::centredLeft);
    g.drawText("Short-Term", b.removeFromLeft(150.0f).toNearestInt(), Justification::centredLeft);
    g.drawText("Integrated", c.removeFromLeft(150.0f).toNearestInt(), Justification::centredLeft);
    g.setColour(Colour::fromRGB(112, 212, 255)); g.setFont(FontOptions(28.0f, Font::bold));
    g.drawText(String(frame.momentaryLufs, 1) + " LUFS", a.toNearestInt(), Justification::centredLeft);
    g.setColour(Colour::fromRGB(255, 232, 170));
    g.drawText(String(frame.shortTermLufs, 1) + " LUFS", b.toNearestInt(), Justification::centredLeft);
    g.setColour(Colour::fromRGB(214, 181, 106));
    g.drawText(String(frame.integratedLufs, 1) + " LUFS", c.toNearestInt(), Justification::centredLeft);

    auto status = right.removeFromTop(96.0f);
    drawPanel(g, status, "Decision Layer");
    auto statusInner = status.reduced(16.0f).withTrimmedTop(28.0f);
    auto pill = statusInner.removeFromTop(42.0f);
    const auto dc = decisionColour(frame.decision);
    g.setColour(dc.withAlpha(0.18f)); g.fillRoundedRectangle(pill, 16.0f);
    g.setColour(dc); g.drawRoundedRectangle(pill, 16.0f, 1.2f);
    g.setFont(FontOptions(22.0f, Font::bold)); g.drawText(decisionText(frame.decision), pill.toNearestInt(), Justification::centred);
    g.setColour(Colours::lightgrey); g.setFont(FontOptions(14.0f));
    g.drawText("Target: Streaming (-16 to -13 LUFS, <= -1.0 dBTP)", statusInner.removeFromTop(22.0f).toNearestInt(), Justification::centredLeft);
    g.drawText("True Peak: " + String(frame.truePeakDbtp, 2) + " dBTP    Correlation: " + String(frame.correlation, 2), statusInner.removeFromTop(22.0f).toNearestInt(), Justification::centredLeft);

    auto vectorscope = right;
    drawPanel(g, vectorscope, "Vectorscope");
    auto scope = vectorscope.reduced(20.0f).withTrimmedTop(34.0f);
    g.setColour(Colours::white.withAlpha(0.05f));
    g.drawEllipse(scope, 1.0f);
    g.drawLine(scope.getCentreX(), scope.getY(), scope.getCentreX(), scope.getBottom(), 1.0f);
    g.drawLine(scope.getX(), scope.getCentreY(), scope.getRight(), scope.getCentreY(), 1.0f);
    Path vp;
    bool started = false;
    for (const auto& p : frame.scope)
    {
        const float x = scope.getCentreX() + p.x * scope.getWidth() * 0.42f;
        const float y = scope.getCentreY() - p.y * scope.getHeight() * 0.42f;
        if (!started) { vp.startNewSubPath(x, y); started = true; }
        else vp.lineTo(x, y);
    }
    g.setColour(Colour::fromRGB(112, 212, 255).withAlpha(0.9f));
    g.strokePath(vp, PathStrokeType(1.3f));

    auto bottom = bounds;
    auto spectrum = bottom.removeFromLeft(bottom.getWidth() * 0.62f);
    drawPanel(g, spectrum, "Spectrum + LUFS Timeline");
    auto content = spectrum.reduced(18.0f).withTrimmedTop(30.0f);
    auto spec = content.removeFromTop(content.getHeight() * 0.58f);
    for (int i = 0; i < 6; ++i)
    {
        const float y = jmap((float) i / 5.0f, spec.getY(), spec.getBottom());
        g.setColour(Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine((int) y, spec.getX(), spec.getRight());
        g.setColour(Colours::lightgrey.withAlpha(0.7f));
        g.setFont(FontOptions(12.0f));
        const float dbLabel = jmap((float) i / 5.0f, 6.0f, -96.0f);
        g.drawText(String((int) std::round(dbLabel)) + " dB", Rectangle<int>((int) spec.getX() + 6, (int) y - 8, 64, 16), Justification::centredLeft);
    }
    Path sp;
    sp.startNewSubPath(spec.getX(), jmap(frame.spectrumDb[0], -96.0f, 6.0f, spec.getBottom(), spec.getY()));
    for (int i = 1; i < metermaid::kSpectrumBins; ++i)
    {
        const float x = spec.getX() + ((float) i / (float) (metermaid::kSpectrumBins - 1)) * spec.getWidth();
        const float y = jmap(frame.spectrumDb[(size_t) i], -96.0f, 6.0f, spec.getBottom(), spec.getY());
        sp.lineTo(x, y);
    }
    g.setColour(Colour::fromRGB(255, 232, 170));
    g.strokePath(sp, PathStrokeType(2.0f));

    auto tl = content.reduced(0.0f, 8.0f);
    g.setColour(Colours::lightgrey.withAlpha(0.7f));
    g.setFont(FontOptions(12.0f));
    g.drawText("Timeline", Rectangle<int>((int) tl.getX(), (int) tl.getY() - 18, 80, 16), Justification::centredLeft);
    g.drawText("-24 LUFS", Rectangle<int>((int) tl.getX() + 6, (int) tl.getBottom() - 18, 80, 16), Justification::centredLeft);
    g.drawText("-6 LUFS", Rectangle<int>((int) tl.getX() + 6, (int) tl.getY() + 2, 80, 16), Justification::centredLeft);
    g.drawText("Recent →", Rectangle<int>((int) tl.getRight() - 90, (int) tl.getBottom() - 18, 84, 16), Justification::centredRight);
    Path hp;
    const int count = historyFilled ? metermaid::kTimelinePoints : historyWrite;
    if (count > 0)
    {
        for (int i = 0; i < count; ++i)
        {
            const int idx = historyFilled ? ((historyWrite + i) % metermaid::kTimelinePoints) : i;
            const float x = tl.getX() + ((float) i / (float) juce::jmax(1, count - 1)) * tl.getWidth();
            const float y = jmap(history[(size_t) idx], -24.0f, -6.0f, tl.getBottom(), tl.getY());
            if (i == 0) hp.startNewSubPath(x, y); else hp.lineTo(x, y);
        }
    }
    g.setColour(Colour::fromRGB(112, 212, 255));
    g.strokePath(hp, PathStrokeType(1.8f));

    auto aux = bottom;
    drawPanel(g, aux, "Auxiliary Metrics");
    auto ar = aux.reduced(16.0f).withTrimmedTop(32.0f);
    g.setColour(Colours::lightgrey); g.setFont(FontOptions(14.0f, Font::bold));
    g.drawText("RMS", ar.removeFromTop(24.0f).toNearestInt(), Justification::centredLeft);
    g.setColour(Colour::fromRGB(255, 232, 170)); g.setFont(FontOptions(22.0f, Font::bold));
    g.drawText(String(frame.rmsDb, 2) + " dB", ar.removeFromTop(36.0f).toNearestInt(), Justification::centredLeft);
    g.setColour(Colours::lightgrey); g.setFont(FontOptions(14.0f, Font::bold));
    g.drawText("Dynamics", ar.removeFromTop(24.0f).toNearestInt(), Justification::centredLeft);
    g.setColour(Colour::fromRGB(214, 181, 106)); g.setFont(FontOptions(22.0f, Font::bold));
    g.drawText(String(frame.dynamicRangeDb, 1) + " dB", ar.removeFromTop(36.0f).toNearestInt(), Justification::centredLeft);
}
