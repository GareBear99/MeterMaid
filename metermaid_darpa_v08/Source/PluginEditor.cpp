#include "PluginEditor.h"

using namespace juce;
using namespace metermaid;

// ===================================================================
// Construction
// ===================================================================
MeterMaidAudioProcessorEditor::MeterMaidAudioProcessorEditor(MeterMaidAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    history.fill(-96.0f);
    setSize(1440, 920);
    startTimerHz(30);
}

void MeterMaidAudioProcessorEditor::resized() {}

void MeterMaidAudioProcessorEditor::timerCallback()
{
    if (processor.getFramePipe().pull(frame))
        pushHistory(frame.integratedLufs);

    // 1-pole smoothing on numeric readouts so they don't jitter.
    auto smooth = [](float& s, float target, float a) { s = s + a * (target - s); };
    smooth(smMomentary,   frame.momentaryLufs, 0.25f);
    smooth(smShort,       frame.shortTermLufs, 0.15f);
    smooth(smIntegrated,  frame.integratedLufs, 0.10f);
    smooth(smTruePeak,    frame.truePeakDbtp,  0.35f);
    smooth(smCorrelation, frame.correlation,   0.18f);
    smooth(smWidth,       frame.stereoWidthPct, 0.18f);
    repaint();
}

void MeterMaidAudioProcessorEditor::pushHistory(float integrated)
{
    history[(size_t) historyWrite] = integrated;
    historyWrite = (historyWrite + 1) % kTimelinePoints;
    if (historyWrite == 0) historyFilled = true;
}

// ===================================================================
// Decision helpers
// ===================================================================
String MeterMaidAudioProcessorEditor::decisionText(DecisionState state) const
{
    switch (state)
    {
        case DecisionState::Ready:           return "Streaming ready";
        case DecisionState::TooQuiet:        return "Too quiet \u2014 raise gain";
        case DecisionState::TooLoud:         return "Too loud \u2014 reduce loudness";
        case DecisionState::PeakRisk:        return "True peak risk \u2014 lower ceiling";
        case DecisionState::StereoRisk:      return "Stereo risk \u2014 mono compatibility warning";
        case DecisionState::DynamicPressure: return "Dynamics pressure \u2014 over-compressed feel";
        default:                             return "Monitoring";
    }
}

Colour MeterMaidAudioProcessorEditor::decisionColour(DecisionState state) const
{
    switch (state)
    {
        case DecisionState::Ready:           return Colour::fromRGB(120, 232, 140);
        case DecisionState::DynamicPressure: return Colour::fromRGB(255, 205,  90);
        case DecisionState::TooQuiet:        return Colour::fromRGB(120, 180, 255);
        case DecisionState::TooLoud:
        case DecisionState::PeakRisk:
        case DecisionState::StereoRisk:      return Colour::fromRGB(255, 112, 112);
        default:                             return Colour::fromRGB(215, 183, 110);
    }
}

Colour MeterMaidAudioProcessorEditor::heatmapColour(float n)
{
    // 5-stop viridis-ish ramp: dark blue -> teal -> green -> amber -> red
    n = jlimit(0.0f, 1.0f, n);
    if (n < 0.25f)      return Colour::fromRGB(  8,  16,  48).interpolatedWith(Colour::fromRGB( 30,  90, 160), n / 0.25f);
    else if (n < 0.50f) return Colour::fromRGB( 30,  90, 160).interpolatedWith(Colour::fromRGB( 60, 200, 190), (n - 0.25f) / 0.25f);
    else if (n < 0.75f) return Colour::fromRGB( 60, 200, 190).interpolatedWith(Colour::fromRGB(230, 220,  80), (n - 0.50f) / 0.25f);
    else                return Colour::fromRGB(230, 220,  80).interpolatedWith(Colour::fromRGB(255,  70,  70), (n - 0.75f) / 0.25f);
}

// ===================================================================
// Chrome
// ===================================================================
void MeterMaidAudioProcessorEditor::drawPanelChrome(Graphics& g, Rectangle<float> r, const String& title)
{
    g.setColour(Colour::fromRGBA(20, 26, 36, 236));
    g.fillRoundedRectangle(r, 16.0f);
    g.setColour(Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(r.reduced(0.5f), 16.0f, 1.0f);
    g.setColour(Colour::fromRGB(255, 232, 170));
    g.setFont(FontOptions(13.0f, Font::bold));
    g.drawText(title, r.reduced(12.0f).removeFromTop(16.0f).toNearestInt(), Justification::centredLeft);
}

void MeterMaidAudioProcessorEditor::drawHeader(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "MeterMaid \u2014 mastering + metering suite");
    auto inner = r.reduced(16.0f).withTrimmedTop(8.0f);
    g.setColour(Colour::fromRGB(214, 181, 106));
    g.setFont(FontOptions(28.0f, Font::bold));
    g.drawText("MeterMaid", inner.removeFromLeft(180.0f).toNearestInt(), Justification::centredLeft);
    g.setColour(Colours::white.withAlpha(0.75f));
    g.setFont(FontOptions(13.0f));
    g.drawText("Octagonal radar \u2022 LUFS suite \u2022 True peak \u2022 Loudness range \u2022 Spectrogram \u2022 Chroma \u2022 Gonio \u2022 Histogram",
               inner.toNearestInt(), Justification::centredLeft);
}

// ===================================================================
// Octagonal radar \u2014 the hero visualiser
// ===================================================================
void MeterMaidAudioProcessorEditor::drawOctagonRadar(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Octagonal Radar \u2014 normalised target matrix");

    auto area = r.reduced(18.0f).withTrimmedTop(20.0f);
    const float side = jmin(area.getWidth(), area.getHeight());
    Rectangle<float> box(area.getCentreX() - side * 0.5f, area.getY(), side, side);

    const float cx = box.getCentreX();
    const float cy = box.getCentreY();
    const float R  = side * 0.44f;

    auto octagonPath = [&](float radius) -> Path
    {
        Path p;
        for (int i = 0; i < 8; ++i)
        {
            // vertex at top (north), proceeding clockwise
            const float a = (float) i * MathConstants<float>::twoPi / 8.0f - MathConstants<float>::halfPi;
            const float x = cx + radius * std::cos(a);
            const float y = cy + radius * std::sin(a);
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        p.closeSubPath();
        return p;
    };

    // Outer ticks (24 around the perimeter) \u2014 decorative scale ring
    g.setColour(Colours::white.withAlpha(0.18f));
    for (int i = 0; i < 48; ++i)
    {
        const float a   = (float) i * MathConstants<float>::twoPi / 48.0f - MathConstants<float>::halfPi;
        const float r1  = R * 1.05f;
        const float r2  = (i % 6 == 0) ? R * 1.12f : R * 1.08f;
        g.drawLine(cx + r1 * std::cos(a), cy + r1 * std::sin(a),
                   cx + r2 * std::cos(a), cy + r2 * std::sin(a), 1.0f);
    }

    // Concentric octagon rings (6 tiers: 20/40/60/80/100% + inner 10%)
    for (int ring = 1; ring <= 6; ++ring)
    {
        const float t = (float) ring / 6.0f;
        g.setColour(Colours::white.withAlpha(ring == 6 ? 0.55f : 0.18f));
        g.strokePath(octagonPath(R * t), PathStrokeType(ring == 6 ? 1.4f : 0.8f));
    }

    // 8 radial spokes + labels
    struct Spoke { const char* label; float value; Colour hue; };
    auto norm = [](float v, float lo, float hi) { return jlimit(0.0f, 1.0f, (v - lo) / (hi - lo)); };
    const Spoke spokes[8] = {
        { "Integrated",  norm(smIntegrated, -60.0f, 0.0f),               Colour::fromRGB(214, 181, 106) },
        { "Short-Term",  norm(smShort,      -60.0f, 0.0f),               Colour::fromRGB(255, 232, 170) },
        { "Momentary",   norm(smMomentary,  -60.0f, 0.0f),               Colour::fromRGB(255, 200, 120) },
        { "RMS",         norm(frame.rmsDb,  -60.0f, 0.0f),               Colour::fromRGB(255, 150,  90) },
        { "Dynamics",    norm(frame.dynamicRangeDb, 0.0f, 20.0f),        Colour::fromRGB(120, 232, 140) },
        { "Correlation", 0.5f + 0.5f * jlimit(-1.0f, 1.0f, smCorrelation), Colour::fromRGB(100, 200, 255) },
        { "Headroom",    norm(-smTruePeak,   0.0f, 12.0f),               Colour::fromRGB(160, 200, 255) },
        { "True Peak",   norm(smTruePeak,   -60.0f, 0.0f),               Colour::fromRGB(255, 112, 112) },
    };

    // Spokes
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 8.0f - MathConstants<float>::halfPi;
        const float sx = cx + R * std::cos(a);
        const float sy = cy + R * std::sin(a);
        g.setColour(Colours::white.withAlpha(0.28f));
        g.drawLine(cx, cy, sx, sy, 0.8f);

        // Vertex-to-vertex diagonals (connect opposite vertices)
        if (i < 4)
        {
            const float ax = cx + R * std::cos(a + MathConstants<float>::pi);
            const float ay = cy + R * std::sin(a + MathConstants<float>::pi);
            g.setColour(Colours::white.withAlpha(0.10f));
            g.drawLine(sx, sy, ax, ay, 0.6f);
        }

        // Spoke label
        g.setColour(Colours::white.withAlpha(0.85f));
        g.setFont(FontOptions(11.0f, Font::bold));
        const float lr = R * 1.18f;
        Rectangle<float> lbl(cx + lr * std::cos(a) - 60.0f, cy + lr * std::sin(a) - 8.0f, 120.0f, 16.0f);
        g.drawText(spokes[(size_t) i].label, lbl.toNearestInt(), Justification::centred);
    }

    // Live metric polygon (filled + stroked)
    Path metricPoly;
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 8.0f - MathConstants<float>::halfPi;
        const float v = jlimit(0.02f, 1.0f, spokes[(size_t) i].value);
        const float x = cx + R * v * std::cos(a);
        const float y = cy + R * v * std::sin(a);
        if (i == 0) metricPoly.startNewSubPath(x, y); else metricPoly.lineTo(x, y);
    }
    metricPoly.closeSubPath();
    g.setColour(Colour::fromRGBA(255, 232, 170, 40));
    g.fillPath(metricPoly);
    g.setColour(Colour::fromRGB(255, 232, 170));
    g.strokePath(metricPoly, PathStrokeType(1.8f));

    // Vertex dots coloured per metric + numeric overlay
    g.setFont(FontOptions(10.0f));
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 8.0f - MathConstants<float>::halfPi;
        const float v = jlimit(0.02f, 1.0f, spokes[(size_t) i].value);
        const float x = cx + R * v * std::cos(a);
        const float y = cy + R * v * std::sin(a);
        g.setColour(spokes[(size_t) i].hue);
        g.fillEllipse(x - 4.0f, y - 4.0f, 8.0f, 8.0f);
    }

    // Inner crosshair / diamond
    g.setColour(Colours::white.withAlpha(0.35f));
    g.drawLine(cx - R * 0.1f, cy, cx + R * 0.1f, cy, 1.0f);
    g.drawLine(cx, cy - R * 0.1f, cx, cy + R * 0.1f, 1.0f);
    Path inner;
    inner.startNewSubPath(cx, cy - R * 0.06f);
    inner.lineTo(cx + R * 0.06f, cy);
    inner.lineTo(cx, cy + R * 0.06f);
    inner.lineTo(cx - R * 0.06f, cy);
    inner.closeSubPath();
    g.strokePath(inner, PathStrokeType(0.8f));
    g.setColour(Colour::fromRGB(255, 232, 170));
    g.fillEllipse(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);

    // Numeric stamp centre-bottom
    g.setColour(Colours::white.withAlpha(0.85f));
    g.setFont(FontOptions(12.0f, Font::bold));
    g.drawText(String("INT ") + String(smIntegrated, 1) + "  ST " + String(smShort, 1) +
               "  M " + String(smMomentary, 1) + " LUFS",
               box.withTrimmedTop(box.getHeight() - 20.0f).toNearestInt(), Justification::centred);
}

// ===================================================================
// Decision pill + numeric clusters
// ===================================================================
void MeterMaidAudioProcessorEditor::drawDecisionCluster(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Decision Layer");
    auto inner = r.reduced(14.0f).withTrimmedTop(20.0f);
    auto pill  = inner.removeFromTop(44.0f);
    const auto dc = decisionColour(frame.decision);
    g.setColour(dc.withAlpha(0.18f));  g.fillRoundedRectangle(pill, 14.0f);
    g.setColour(dc);                   g.drawRoundedRectangle(pill, 14.0f, 1.2f);
    g.setFont(FontOptions(18.0f, Font::bold));
    g.drawText(decisionText(frame.decision), pill.toNearestInt(), Justification::centred);

    g.setColour(Colours::white.withAlpha(0.75f));
    g.setFont(FontOptions(12.0f));
    g.drawText("Target: Streaming (-16..-13 LUFS, <= -1.0 dBTP)",
               inner.removeFromTop(20.0f).toNearestInt(), Justification::centredLeft);
    g.drawText("Readiness " + String(frame.readinessState) + " / 2",
               inner.removeFromTop(20.0f).toNearestInt(), Justification::centredLeft);
}

void MeterMaidAudioProcessorEditor::drawLoudnessReadouts(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Loudness (LUFS)");
    auto inner = r.reduced(14.0f).withTrimmedTop(22.0f);
    auto row = [&](Rectangle<float>& c, const char* label, float value, Colour colour)
    {
        auto line = c.removeFromTop(c.getHeight() / 4.0f);
        g.setColour(Colours::white.withAlpha(0.7f));
        g.setFont(FontOptions(12.0f, Font::bold));
        g.drawText(label, line.removeFromLeft(110.0f).toNearestInt(), Justification::centredLeft);
        g.setColour(colour);
        g.setFont(FontOptions(20.0f, Font::bold));
        g.drawText(String(value, 1) + " LUFS", line.toNearestInt(), Justification::centredLeft);
    };
    row(inner, "Momentary",  smMomentary,   Colour::fromRGB(120, 212, 255));
    row(inner, "Short-Term", smShort,       Colour::fromRGB(255, 232, 170));
    row(inner, "Integrated", smIntegrated,  Colour::fromRGB(214, 181, 106));
    row(inner, "LRA",        frame.loudnessRangeLu, Colour::fromRGB(200, 160, 255));
}

void MeterMaidAudioProcessorEditor::drawPeakRmsTable(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Peak / RMS (per channel)");
    auto inner = r.reduced(14.0f).withTrimmedTop(24.0f);
    g.setColour(Colours::white.withAlpha(0.6f));
    g.setFont(FontOptions(12.0f, Font::bold));
    auto hdr = inner.removeFromTop(18.0f);
    const int colW = (int) (inner.getWidth() / 3);
    g.drawText("Metric", hdr.removeFromLeft((float) colW).toNearestInt(), Justification::centredLeft);
    g.drawText("L",      hdr.removeFromLeft((float) colW).toNearestInt(), Justification::centredLeft);
    g.drawText("R",      hdr.toNearestInt(),                              Justification::centredLeft);

    auto row = [&](const char* label, float l, float r, Colour col)
    {
        auto line = inner.removeFromTop(26.0f);
        g.setColour(Colours::white.withAlpha(0.75f));
        g.setFont(FontOptions(13.0f));
        g.drawText(label, line.removeFromLeft((float) colW).toNearestInt(), Justification::centredLeft);
        g.setColour(col);
        g.setFont(FontOptions(15.0f, Font::bold));
        g.drawText(String(l, 2) + " dB", line.removeFromLeft((float) colW).toNearestInt(), Justification::centredLeft);
        g.drawText(String(r, 2) + " dB", line.toNearestInt(),                              Justification::centredLeft);
    };
    row("True Peak",    frame.truePeakLDbtp, frame.truePeakRDbtp, Colour::fromRGB(255, 112, 112));
    row("Sample Peak",  frame.samplePeakLDb, frame.samplePeakRDb, Colour::fromRGB(255, 180,  90));
    row("RMS",          frame.rmsLDb,        frame.rmsRDb,        Colour::fromRGB(255, 232, 170));
}

void MeterMaidAudioProcessorEditor::drawRatiosPanel(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Dynamics / ratios");
    auto inner = r.reduced(14.0f).withTrimmedTop(22.0f);
    auto row = [&](const char* label, float value, const char* unit, Colour col)
    {
        auto line = inner.removeFromTop(inner.getHeight() / 5.0f);
        g.setColour(Colours::white.withAlpha(0.7f));
        g.setFont(FontOptions(12.0f));
        g.drawText(label, line.removeFromLeft(110.0f).toNearestInt(), Justification::centredLeft);
        g.setColour(col);
        g.setFont(FontOptions(17.0f, Font::bold));
        g.drawText(String(value, 1) + " " + unit, line.toNearestInt(), Justification::centredLeft);
    };
    row("Crest",    frame.crestFactorDb,  "dB", Colour::fromRGB(214, 181, 106));
    row("DR",       frame.dynamicRangeDb, "dB", Colour::fromRGB(120, 232, 140));
    row("PSR",      frame.psrDb,          "dB", Colour::fromRGB(255, 180,  90));
    row("PLR",      frame.plrDb,          "dB", Colour::fromRGB(255, 150,  90));
    row("Width",    smWidth,              "%",  Colour::fromRGB(100, 200, 255));
}

void MeterMaidAudioProcessorEditor::drawCorrelationBar(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Phase correlation");
    auto inner = r.reduced(14.0f).withTrimmedTop(22.0f);
    auto bar = inner.removeFromTop(18.0f).reduced(4.0f, 0.0f);

    // Gradient background
    g.setColour(Colour::fromRGB(40,  50,  70));  g.fillRoundedRectangle(bar, 6.0f);
    g.setColour(Colour::fromRGB(255, 112, 112).withAlpha(0.25f)); g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * 0.5f), 6.0f);
    g.setColour(Colour::fromRGB(120, 232, 140).withAlpha(0.25f)); g.fillRoundedRectangle(bar.withTrimmedLeft(bar.getWidth() * 0.5f), 6.0f);

    const float nx = jlimit(-1.0f, 1.0f, smCorrelation);
    const float dotX = jmap(nx, -1.0f, 1.0f, bar.getX(), bar.getRight());
    g.setColour(Colours::white);
    g.fillEllipse(dotX - 7.0f, bar.getCentreY() - 7.0f, 14.0f, 14.0f);
    g.setColour(Colour::fromRGB(255, 232, 170));
    g.drawEllipse(dotX - 7.0f, bar.getCentreY() - 7.0f, 14.0f, 14.0f, 1.5f);

    g.setColour(Colours::white.withAlpha(0.7f));
    g.setFont(FontOptions(11.0f));
    g.drawText("-1",        Rectangle<int>((int) bar.getX(),                    (int) bar.getBottom() + 4, 30, 14), Justification::centredLeft);
    g.drawText("mono",      Rectangle<int>((int) bar.getCentreX() - 20,         (int) bar.getBottom() + 4, 40, 14), Justification::centred);
    g.drawText("+1",        Rectangle<int>((int) bar.getRight() - 30,           (int) bar.getBottom() + 4, 30, 14), Justification::centredRight);

    g.setColour(Colour::fromRGB(255, 232, 170));
    g.setFont(FontOptions(20.0f, Font::bold));
    g.drawText(String(smCorrelation, 2), inner.removeFromTop(36.0f).toNearestInt(), Justification::centred);
}

// ===================================================================
// Stereo visualisers
// ===================================================================
void MeterMaidAudioProcessorEditor::drawGoniometer(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Goniometer (L/R rotated 45\u00b0)");
    auto inner = r.reduced(14.0f).withTrimmedTop(22.0f);
    const float side = jmin(inner.getWidth(), inner.getHeight());
    Rectangle<float> box(inner.getCentreX() - side * 0.5f, inner.getCentreY() - side * 0.5f, side, side);

    const float cx = box.getCentreX();
    const float cy = box.getCentreY();
    const float rad = side * 0.46f;

    g.setColour(Colours::white.withAlpha(0.10f));
    g.drawEllipse(box.reduced(side * 0.04f), 1.0f);
    for (int i = 0; i < 4; ++i)
    {
        const float a = (float) i * MathConstants<float>::halfPi;
        g.drawLine(cx, cy, cx + rad * std::cos(a), cy + rad * std::sin(a), 1.0f);
    }
    // L and R diagonals at 45°
    g.setColour(Colour::fromRGB(120, 212, 255).withAlpha(0.6f));
    g.drawLine(cx - rad * 0.7f, cy - rad * 0.7f, cx + rad * 0.7f, cy + rad * 0.7f, 1.2f);
    g.setColour(Colour::fromRGB(255, 150, 120).withAlpha(0.6f));
    g.drawLine(cx + rad * 0.7f, cy - rad * 0.7f, cx - rad * 0.7f, cy + rad * 0.7f, 1.2f);

    g.setColour(Colours::white.withAlpha(0.75f));
    g.setFont(FontOptions(11.0f, Font::bold));
    g.drawText("L", Rectangle<int>((int)(cx - rad * 0.75f) - 12, (int)(cy - rad * 0.75f) - 6, 24, 14), Justification::centred);
    g.drawText("R", Rectangle<int>((int)(cx + rad * 0.75f) - 12, (int)(cy - rad * 0.75f) - 6, 24, 14), Justification::centred);
    g.drawText("M", Rectangle<int>((int) cx - 12,                (int)(cy - rad) - 6,         24, 14), Justification::centred);
    g.drawText("S", Rectangle<int>((int)(cx + rad) - 12,         (int) cy - 6,                24, 14), Justification::centred);

    g.setColour(Colour::fromRGB(120, 232, 140).withAlpha(0.85f));
    for (const auto& p : frame.gonio)
    {
        const float x = cx + p.x * rad;
        const float y = cy - p.y * rad;
        g.fillEllipse(x - 1.2f, y - 1.2f, 2.4f, 2.4f);
    }
}

void MeterMaidAudioProcessorEditor::drawVectorscope(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Vectorscope (Side / Mid)");
    auto inner = r.reduced(14.0f).withTrimmedTop(22.0f);
    const float side = jmin(inner.getWidth(), inner.getHeight());
    Rectangle<float> box(inner.getCentreX() - side * 0.5f, inner.getCentreY() - side * 0.5f, side, side);

    g.setColour(Colours::white.withAlpha(0.08f));
    g.drawEllipse(box, 1.0f);
    g.drawLine(box.getCentreX(), box.getY(), box.getCentreX(), box.getBottom(), 1.0f);
    g.drawLine(box.getX(), box.getCentreY(), box.getRight(), box.getCentreY(), 1.0f);

    Path vp; bool started = false;
    for (const auto& p : frame.scope)
    {
        const float x = box.getCentreX() + p.x * side * 0.44f;
        const float y = box.getCentreY() - p.y * side * 0.44f;
        if (!started) { vp.startNewSubPath(x, y); started = true; }
        else          vp.lineTo(x, y);
    }
    g.setColour(Colour::fromRGB(112, 212, 255).withAlpha(0.9f));
    g.strokePath(vp, PathStrokeType(1.3f));
}

// ===================================================================
// Spectrum / spectrogram / timeline / histogram / chroma
// ===================================================================
void MeterMaidAudioProcessorEditor::drawSpectrum(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Spectrum");
    auto content = r.reduced(14.0f).withTrimmedTop(22.0f);
    for (int i = 0; i <= 4; ++i)
    {
        const float y = jmap((float) i / 4.0f, content.getY(), content.getBottom());
        g.setColour(Colours::white.withAlpha(0.06f));
        g.drawHorizontalLine((int) y, content.getX(), content.getRight());
        g.setColour(Colours::white.withAlpha(0.5f));
        g.setFont(FontOptions(10.0f));
        const float db = jmap((float) i / 4.0f, 6.0f, -96.0f);
        g.drawText(String((int) std::round(db)) + " dB",
                   Rectangle<int>((int) content.getX() + 4, (int) y - 7, 50, 14), Justification::centredLeft);
    }
    Path sp;
    sp.startNewSubPath(content.getX(), jmap(frame.spectrumDb[0], -96.0f, 6.0f, content.getBottom(), content.getY()));
    for (int i = 1; i < kSpectrumBins; ++i)
    {
        const float x = content.getX() + ((float) i / (float) (kSpectrumBins - 1)) * content.getWidth();
        const float y = jmap(frame.spectrumDb[(size_t) i], -96.0f, 6.0f, content.getBottom(), content.getY());
        sp.lineTo(x, y);
    }
    g.setColour(Colour::fromRGB(255, 232, 170));
    g.strokePath(sp, PathStrokeType(1.6f));
}

void MeterMaidAudioProcessorEditor::drawSpectrogram(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Spectrogram (log frequency \u00b7 time \u2192)");
    auto content = r.reduced(14.0f).withTrimmedTop(22.0f);
    const int W = (int) content.getWidth();
    const int H = (int) content.getHeight();
    if (W <= 0 || H <= 0) return;
    const float colW = (float) W / (float) kSpectrogramFrames;
    const float rowH = (float) H / (float) kSpectrogramBins;
    for (int c = 0; c < kSpectrogramFrames; ++c)
    {
        // Map oldest frame -> left, newest -> right.
        const int src = (frame.spectrogramWrite + c) % kSpectrogramFrames;
        const auto& col = frame.spectrogram[(size_t) src];
        const float x = content.getX() + (float) c * colW;
        for (int b = 0; b < kSpectrogramBins; ++b)
        {
            const float db = col[(size_t) b];
            const float n  = jlimit(0.0f, 1.0f, (db - (-96.0f)) / (6.0f - (-96.0f)));
            if (n < 0.05f) continue;
            const float y = content.getBottom() - (float) (b + 1) * rowH;
            g.setColour(heatmapColour(n));
            g.fillRect(x, y, colW + 0.5f, rowH + 0.5f);
        }
    }
}

void MeterMaidAudioProcessorEditor::drawTimeline(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Integrated LUFS timeline");
    auto tl = r.reduced(14.0f).withTrimmedTop(22.0f);
    g.setColour(Colours::white.withAlpha(0.5f));
    g.setFont(FontOptions(10.0f));
    g.drawText("-6 LUFS",  Rectangle<int>((int) tl.getX() + 4, (int) tl.getY() + 2,       80, 14), Justification::centredLeft);
    g.drawText("-24 LUFS", Rectangle<int>((int) tl.getX() + 4, (int) tl.getBottom() - 16, 80, 14), Justification::centredLeft);

    const int count = historyFilled ? kTimelinePoints : historyWrite;
    if (count <= 1) return;
    Path hp;
    for (int i = 0; i < count; ++i)
    {
        const int idx = historyFilled ? ((historyWrite + i) % kTimelinePoints) : i;
        const float x = tl.getX() + ((float) i / (float) (count - 1)) * tl.getWidth();
        const float y = jmap(history[(size_t) idx], -24.0f, -6.0f, tl.getBottom(), tl.getY());
        if (i == 0) hp.startNewSubPath(x, y); else hp.lineTo(x, y);
    }
    g.setColour(Colour::fromRGB(112, 212, 255));
    g.strokePath(hp, PathStrokeType(1.6f));
}

void MeterMaidAudioProcessorEditor::drawHistogram(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Loudness histogram (-48..0 LUFS)");
    auto content = r.reduced(14.0f).withTrimmedTop(22.0f);
    const float binW = content.getWidth() / (float) kHistogramBins;
    for (int i = 0; i < kHistogramBins; ++i)
    {
        const float h = frame.lufsHistogram[(size_t) i] * content.getHeight();
        const float x = content.getX() + (float) i * binW;
        g.setColour(heatmapColour(jlimit(0.0f, 1.0f, (float) i / (float) (kHistogramBins - 1))));
        g.fillRect(x + 0.5f, content.getBottom() - h, binW - 1.0f, h);
    }
    // Axis labels at -48 / -24 / 0
    g.setColour(Colours::white.withAlpha(0.55f));
    g.setFont(FontOptions(10.0f));
    g.drawText("-48", Rectangle<int>((int) content.getX(),                   (int) content.getBottom() + 2, 40, 14), Justification::centredLeft);
    g.drawText("-24", Rectangle<int>((int) content.getCentreX() - 12,        (int) content.getBottom() + 2, 28, 14), Justification::centred);
    g.drawText("0",   Rectangle<int>((int) content.getRight() - 20,          (int) content.getBottom() + 2, 24, 14), Justification::centredRight);
}

void MeterMaidAudioProcessorEditor::drawChromagram(Graphics& g, Rectangle<float> r)
{
    drawPanelChrome(g, r, "Chromagram (C..B)");
    auto content = r.reduced(14.0f).withTrimmedTop(22.0f);
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float binW = content.getWidth() / (float) kChromaBins;
    for (int i = 0; i < kChromaBins; ++i)
    {
        const float v = jlimit(0.0f, 1.0f, frame.chroma[(size_t) i]);
        const float h = v * content.getHeight();
        const float x = content.getX() + (float) i * binW;
        const Rectangle<float> bar(x + 2.0f, content.getBottom() - h, binW - 4.0f, h);
        g.setColour(heatmapColour(v));
        g.fillRoundedRectangle(bar, 3.0f);
        g.setColour(Colours::white.withAlpha(0.75f));
        g.setFont(FontOptions(10.0f, Font::bold));
        g.drawText(names[i], Rectangle<int>((int) x, (int) content.getBottom() + 2, (int) binW, 14), Justification::centred);
    }
}

// ===================================================================
// Top-level paint
// ===================================================================
void MeterMaidAudioProcessorEditor::paint(Graphics& g)
{
    g.fillAll(Colour::fromRGB(8, 11, 17));
    auto bounds = getLocalBounds().toFloat().reduced(14.0f);

    // Header
    auto header = bounds.removeFromTop(62.0f);
    drawHeader(g, header);
    bounds.removeFromTop(6.0f);

    // Main: three columns \u2014 left (radar), middle (numerics + scopes), right (correlation + ratios)
    auto main = bounds.removeFromTop(520.0f);
    auto radarZone = main.removeFromLeft(main.getWidth() * 0.46f);
    drawOctagonRadar(g, radarZone);

    main.removeFromLeft(8.0f);
    auto midZone = main.removeFromLeft(main.getWidth() * 0.5f);
    {
        auto decision = midZone.removeFromTop(120.0f);
        drawDecisionCluster(g, decision);
        midZone.removeFromTop(8.0f);

        auto loud = midZone.removeFromTop(180.0f);
        drawLoudnessReadouts(g, loud);
        midZone.removeFromTop(8.0f);

        drawPeakRmsTable(g, midZone);
    }
    main.removeFromLeft(8.0f);
    {
        auto gonio = main.removeFromTop(main.getHeight() * 0.58f);
        drawGoniometer(g, gonio);
        main.removeFromTop(8.0f);

        auto corrBar = main.removeFromTop(main.getHeight() * 0.45f);
        drawCorrelationBar(g, corrBar);
        main.removeFromTop(8.0f);

        drawRatiosPanel(g, main);
    }

    bounds.removeFromTop(6.0f);

    // Bottom row: spectrum + spectrogram
    auto row1 = bounds.removeFromTop(180.0f);
    auto spec = row1.removeFromLeft(row1.getWidth() * 0.50f);
    drawSpectrum(g, spec);
    row1.removeFromLeft(8.0f);
    drawSpectrogram(g, row1);

    bounds.removeFromTop(6.0f);

    // Bottom row: timeline + histogram + chroma + vectorscope
    auto row2 = bounds;
    auto tl     = row2.removeFromLeft(row2.getWidth() * 0.30f);
    drawTimeline(g, tl);
    row2.removeFromLeft(8.0f);
    auto hist   = row2.removeFromLeft(row2.getWidth() * 0.30f);
    drawHistogram(g, hist);
    row2.removeFromLeft(8.0f);
    auto chroma = row2.removeFromLeft(row2.getWidth() * 0.45f);
    drawChromagram(g, chroma);
    row2.removeFromLeft(8.0f);
    drawVectorscope(g, row2);
}
