#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour bg0       { 0xff111317 };
    const juce::Colour bg1       { 0xff181b20 };
    const juce::Colour card      { 0xff1c2026 };
    const juce::Colour cardEdge  { 0x14ffffff };
    const juce::Colour text      { 0xffe9ebee };
    const juce::Colour dim       { 0xff8a919b };
    const juce::Colour faint     { 0xff4a5059 };
    const juce::Colour accent    { 0xff4f9dff };
    const juce::Colour warm      { 0xffffb547 };
    const juce::Colour hot       { 0xffff5a4f };
    const juce::Colour display   { 0xff0b0d10 };
}

static juce::Font uiFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

static void drawCard (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Pal::card.brighter (0.06f), r.getX(), r.getY(),
                                       Pal::card, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (Pal::cardEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

    if (title.isNotEmpty())
    {
        g.setColour (Pal::dim);
        g.setFont (uiFont (10.5f, true));
        g.drawText (title, r.reduced (16.0f, 12.0f).removeFromTop (14.0f), Justification::centredLeft);
    }
}

//==============================================================================
VBCLookAndFeel::VBCLookAndFeel()
{
    setColour (juce::Label::textColourId, Pal::text);
    setColour (juce::ToggleButton::textColourId, Pal::text);
}

void VBCLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& s)
{
    using namespace juce;
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    const int ticks = (int) s.getProperties().getWithDefault ("ticks", 11);

    // tick dots
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (float) i / (float) jmax (1, ticks - 1) * (endAngle - startAngle);
        const auto p = c.getPointOnCircumference (r * 0.98f, a);
        const bool lit = a <= angle + 1.0e-3f;
        g.setColour (lit ? accent.withAlpha (0.9f) : Pal::faint);
        g.fillEllipse (p.x - 1.5f, p.y - 1.5f, 3.0f, 3.0f);
    }

    // track + value arc
    const float ar = r * 0.84f;
    Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        Path val;
        val.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, angle, true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (val, PathStrokeType (8.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (val, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // knob body: soft shadow, machined edge, flat cap
    const float kr = r * 0.66f;
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - kr, c.y - kr + 3.0f, kr * 2.0f, kr * 2.0f);
    g.setGradientFill (ColourGradient (Colour (0xff3b4048), c.x, c.y - kr, Colour (0xff1e2126), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    const float cr = kr * 0.84f;
    g.setGradientFill (ColourGradient (Colour (0xff2c3037), c.x, c.y - cr, Colour (0xff23262c), c.x, c.y + cr, false));
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawEllipse (c.x - kr + 0.5f, c.y - kr + 0.5f, kr * 2.0f - 1.0f, kr * 2.0f - 1.0f, 1.0f);

    // pointer
    const auto p1 = c.getPointOnCircumference (cr * 0.35f, angle);
    const auto p2 = c.getPointOnCircumference (cr * 0.88f, angle);
    g.setColour (Pal::text);
    g.drawLine ({ p1, p2 }, 3.0f);
}

void VBCLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    const bool bigIn = b.getProperties().getWithDefault ("style", "") == var ("in");

    if (bigIn)
    {
        // illuminated square "IN" switch
        if (on)
        {
            for (int i = 3; i > 0; --i)
            {
                g.setColour (Pal::warm.withAlpha (0.07f * (float) i));
                g.fillRoundedRectangle (r.expanded (0.0f), 12.0f);
            }
            g.setGradientFill (ColourGradient (Pal::warm.brighter (0.35f), r.getCentreX(), r.getY(),
                                               Pal::warm.darker (0.25f), r.getCentreX(), r.getBottom(), false));
        }
        else
        {
            g.setGradientFill (ColourGradient (Colour (0xff2b2f36), r.getCentreX(), r.getY(),
                                               Colour (0xff1d2025), r.getCentreX(), r.getBottom(), false));
        }
        g.fillRoundedRectangle (r.reduced (6.0f), 9.0f);
        g.setColour (on ? Colours::white.withAlpha (0.35f) : Colours::white.withAlpha (highlighted ? 0.18f : 0.08f));
        g.drawRoundedRectangle (r.reduced (6.5f), 9.0f, 1.0f);
        g.setColour (on ? Colour (0xff2a1a00) : Pal::dim);
        g.setFont (uiFont (17.0f, true));
        g.drawText (b.getButtonText(), r.translated (0.0f, down ? 1.0f : 0.0f), Justification::centred);
        return;
    }

    // LED pill button
    g.setColour (on ? Pal::accent.withAlpha (0.14f) : Colour (0xff22262c));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (on ? Pal::accent.withAlpha (0.55f) : Colours::white.withAlpha (highlighted ? 0.16f : 0.07f));
    g.drawRoundedRectangle (r, 9.0f, 1.0f);

    const auto led = Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 15.0f, r.getCentreY() });
    if (on)
    {
        g.setColour (Pal::accent.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.5f));
    }
    g.setColour (on ? Pal::accent.brighter (0.3f) : Pal::faint);
    g.fillEllipse (led);

    g.setColour (on ? Pal::text : Pal::dim);
    g.setFont (uiFont (11.5f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (24.0f).withTrimmedRight (6.0f), Justification::centred);
}

//==============================================================================
void GainReductionMeter::setGainReduction (float db)
{
    const float target = juce::jlimit (0.0f, 20.0f, db);
    needleDb += (target - needleDb) * (target > needleDb ? 0.5f : 0.2f); // VU-like ballistics at ~30 Hz

    if (target >= peakDb) { peakDb = target; peakHoldTicks = 45; }       // ~1.5 s hold
    else if (--peakHoldTicks < 0) peakDb = juce::jmax (target, peakDb - 0.35f);

    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    using namespace juce;
    auto bounds = getLocalBounds().toFloat();

    // display glass
    g.setColour (Pal::display);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setGradientFill (ColourGradient (Pal::accent.withAlpha (0.10f), bounds.getCentreX(), bounds.getBottom(),
                                       Colours::transparentBlack, bounds.getCentreX(), bounds.getY(), true));
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    const float a0 = -1.25f, a1 = 1.25f;                               // 0 dB on the LEFT
    const float R = jmin (bounds.getWidth() * 0.5f - 52.0f, bounds.getHeight() * 0.45f); // leave room for edge labels
    // centre the whole figure (labels above the arc down to the readout caption) vertically
    const float figureH = R + 36.0f + (-0.3f * R + 46.0f);
    const auto pivot = Point<float> (bounds.getCentreX(), bounds.getY() + (bounds.getHeight() - figureH) * 0.5f + 36.0f + R);
    auto angleFor = [&] (float db) { return a0 + (db / 20.0f) * (a1 - a0); };
    auto zoneColour = [] (float db) { return db < 6.0f ? Pal::accent : (db < 12.0f ? Pal::warm : Pal::hot); };

    // zone band + track
    auto arc = [&] (float fromDb, float toDb, float radius, float thickness, Colour col)
    {
        Path p;
        p.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (fromDb), angleFor (toDb), true);
        g.setColour (col);
        g.strokePath (p, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::butt));
    };
    arc (0.0f, 20.0f, R, 6.0f, Colours::white.withAlpha (0.06f));
    arc (0.0f, 6.0f,   R + 9.0f, 2.0f, Pal::accent.withAlpha (0.45f));
    arc (6.0f, 12.0f,  R + 9.0f, 2.0f, Pal::warm.withAlpha (0.45f));
    arc (12.0f, 20.0f, R + 9.0f, 2.0f, Pal::hot.withAlpha (0.45f));

    // fill up to the current GR
    if (needleDb > 0.05f)
    {
        arc (0.0f, needleDb, R, 14.0f, zoneColour (needleDb).withAlpha (0.18f));
        arc (0.0f, needleDb, R, 6.0f, zoneColour (needleDb));
    }

    // ticks + labels
    g.setFont (uiFont (11.0f, true));
    for (int db = 0; db <= 20; db += 2)
    {
        const float a = angleFor ((float) db);
        const bool major = db % 4 == 0;
        g.setColour (major ? Pal::text.withAlpha (0.85f) : Pal::faint);
        g.drawLine ({ pivot.getPointOnCircumference (R + 14.0f, a), pivot.getPointOnCircumference (R + (major ? 24.0f : 19.0f), a) },
                    major ? 1.5f : 1.0f);
        if (major)
        {
            g.setColour (Pal::dim);
            g.drawText (String (db), Rectangle<float> (26.0f, 14.0f).withCentre (pivot.getPointOnCircumference (R + 36.0f, a)),
                        Justification::centred);
        }
    }

    // peak-hold marker
    if (peakDb > 0.2f)
    {
        const float a = angleFor (peakDb);
        g.setColour (Pal::warm);
        g.drawLine ({ pivot.getPointOnCircumference (R - 7.0f, a), pivot.getPointOnCircumference (R + 7.0f, a) }, 2.0f);
    }

    // short needle near the scale (keeps the centre free for the readout)
    {
        const float a = angleFor (needleDb);
        const Line<float> needle (pivot.getPointOnCircumference (R * 0.62f, a), pivot.getPointOnCircumference (R + 10.0f, a));
        g.setColour (Colours::white.withAlpha (0.15f));
        g.drawLine (needle, 6.0f);
        g.setColour (Colours::white);
        g.drawLine (needle, 2.0f);
    }

    // numeric readout
    const auto readout = Rectangle<float> (R * 1.2f, 40.0f).withCentre ({ pivot.x, pivot.y - R * 0.30f });
    g.setColour (needleDb > 0.05f ? Pal::text : Pal::dim);
    g.setFont (uiFont (34.0f, true));
    g.drawText (needleDb > 0.05f ? "-" + String (needleDb, 1) : "0.0", readout, Justification::centred);
    g.setColour (Pal::dim);
    g.setFont (uiFont (10.5f, true));
    g.drawText ("dB  GAIN REDUCTION", readout.translated (0.0f, 30.0f), Justification::centred);
}

//==============================================================================
ParamBinding::ParamBinding (juce::RangedAudioParameter& p, juce::Slider& s) : param (p), slider (&s)
{
    using namespace juce;
    // Mirror the parameter's (possibly skewed / stepped) range on the slider.
    const auto range = param.getNormalisableRange();
    NormalisableRange<double> r ((double) range.start, (double) range.end,
        [range] (double, double, double v) { return (double) range.convertFrom0to1 ((float) v); },
        [range] (double, double, double v) { return (double) range.convertTo0to1 ((float) v); },
        [range] (double, double, double v) { return (double) range.snapToLegalValue ((float) v); });
    r.interval = range.interval;
    r.skew = range.skew;
    r.symmetricSkew = range.symmetricSkew;
    s.setNormalisableRange (r);

    s.textFromValueFunction = [this] (double v) { return param.getText (param.convertTo0to1 ((float) v), 0); };
    s.valueFromTextFunction = [this] (const String& t) { return (double) param.convertFrom0to1 (param.getValueForText (t)); };
    s.setDoubleClickReturnValue (true, (double) param.convertFrom0to1 (param.getDefaultValue()));

    s.onDragStart   = [this] { dragging = true; param.beginChangeGesture(); };
    s.onDragEnd     = [this] { param.endChangeGesture(); dragging = false; };
    s.onValueChange = [this] { sendSliderValue(); };
    poll();
    s.updateText();
}

ParamBinding::ParamBinding (juce::RangedAudioParameter& p, juce::Button& b, bool inv)
    : param (p), button (&b), invert (inv)
{
    b.setClickingTogglesState (true);
    b.onClick = [this]
    {
        const bool state = button->getToggleState() != invert;
        param.beginChangeGesture();
        param.setValueNotifyingHost (state ? 1.0f : 0.0f);
        param.endChangeGesture();
    };
    poll();
}

void ParamBinding::sendSliderValue()
{
    const float norm = param.convertTo0to1 ((float) slider->getValue());
    if (juce::approximatelyEqual (norm, param.getValue()))
        return;

    if (dragging)
        param.setValueNotifyingHost (norm);
    else
    {   // mouse wheel / double-click / text entry: a one-shot gesture
        param.beginChangeGesture();
        param.setValueNotifyingHost (norm);
        param.endChangeGesture();
    }
}

void ParamBinding::poll()
{
    const float v = param.getValue(); // atomic read
    if (juce::approximatelyEqual (v, lastPolled))
        return;
    lastPolled = v;

    if (slider != nullptr && ! dragging)
        slider->setValue ((double) param.convertFrom0to1 (v), juce::dontSendNotification);
    if (button != nullptr)
        button->setToggleState ((v >= 0.5f) != invert, juce::dontSendNotification);
}

//==============================================================================
static constexpr float kKnobStart = juce::MathConstants<float>::pi * 1.25f;
static constexpr float kKnobEnd   = juce::MathConstants<float>::pi * 2.75f;

KnobCell::KnobCell (const juce::String& n, juce::StringArray scale, bool isStepped, juce::Colour acc, const juce::String& unit)
    : name (n), suffix (unit), labels (std::move (scale)), stepped (isStepped), accent (acc)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (kKnobStart, kKnobEnd, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    slider.getProperties().set ("ticks", stepped ? labels.size() : 11);
    slider.addListener (this);
    addAndMakeVisible (slider);
}

KnobCell::~KnobCell() { slider.removeListener (this); }

void KnobCell::resized()
{
    auto b = getLocalBounds();
    knobArea = b.removeFromTop (b.getHeight() - 44).reduced (4);
    const int d = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) - 34;
    slider.setBounds (juce::Rectangle<int> (d, d).withCentre (knobArea.getCentre()));
}

void KnobCell::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto sb = slider.getBounds().toFloat();
    const auto c = sb.getCentre();
    const float r = sb.getWidth() * 0.5f;

    // hardware-style scale labels around the knob
    const double norm = slider.valueToProportionOfLength (slider.getValue());
    const int selected = stepped ? roundToInt (norm * (labels.size() - 1)) : -1;
    g.setFont (uiFont (10.5f, true));
    for (int i = 0; i < labels.size(); ++i)
    {
        const float p = labels.size() > 1 ? (float) i / (float) (labels.size() - 1) : 0.0f;
        const float a = kKnobStart + p * (kKnobEnd - kKnobStart);
        const auto pt = c.getPointOnCircumference (r + 13.0f, a);
        g.setColour (i == selected ? Pal::text : Pal::dim.withAlpha (0.8f));
        g.drawText (labels[i], Rectangle<float> (34.0f, 14.0f).withCentre (pt), Justification::centred);
    }

    // name + value
    auto text = getLocalBounds().toFloat().removeFromBottom (44.0f);
    g.setColour (Pal::text.withAlpha (0.9f));
    g.setFont (uiFont (11.5f, true));
    g.drawText (name, text.removeFromTop (18.0f), Justification::centred);

    auto value = slider.getTextFromValue (slider.getValue());
    if (suffix.isNotEmpty() && value.containsOnly ("0123456789.-+"))
        value << suffix;
    g.setColour (accent);
    g.setFont (uiFont (13.0f, true));
    g.drawText (value, text.removeFromTop (18.0f), Justification::centred);
}

//==============================================================================
void VBCAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Pal::bg1, 0.0f, 0.0f, Pal::bg0, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setColour (Pal::text);
    g.setFont (uiFont (24.0f, true));
    g.drawText ("VBC-1", 24, 18, 90, 30, Justification::centredLeft);
    g.setColour (Pal::dim);
    g.setFont (uiFont (11.0f, true));
    g.drawText ("VCA  BUS  COMPRESSOR", 106, 20, 220, 28, Justification::centredLeft);

    const auto pill = Rectangle<float> (566.0f, 22.0f, 130.0f, 24.0f);
    g.setColour (Pal::accent.withAlpha (0.12f));
    g.fillRoundedRectangle (pill, 12.0f);
    g.setColour (Pal::accent.withAlpha (0.6f));
    g.drawRoundedRectangle (pill, 12.0f, 1.0f);
    g.setColour (Pal::accent.brighter (0.3f));
    g.setFont (uiFont (10.5f, true));
    g.drawText ("ZERO LATENCY", pill, Justification::centred);

    // cards (left control column, right meter / character / side-chain)
    drawCard (g, { 20.0f, 68.0f, 330.0f, 712.0f });
    drawCard (g, { 366.0f, 68.0f, 334.0f, 252.0f });
    drawCard (g, { 366.0f, 336.0f, 334.0f, 262.0f }, "CHARACTER");
    drawCard (g, { 366.0f, 614.0f, 334.0f, 166.0f }, "SIDECHAIN");

    // divider lines between left-column rows
    g.setColour (Colours::white.withAlpha (0.04f));
    for (int row = 1; row < 4; ++row)
        g.drawHorizontalLine (68 + 12 + row * 172, 40.0f, 330.0f);

    g.setColour (Pal::faint);
    g.setFont (uiFont (10.0f));
    g.drawText ("VCA + output-stage saturation", Rectangle<float> (366.0f, 566.0f, 334.0f, 18.0f), Justification::centred);
}

VBCAudioProcessorEditor::VBCAudioProcessorEditor (VBCAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      threshold ("THRESHOLD", { "-30", "+6" }, false, Pal::accent),
      makeup    ("MAKE-UP",   { "-10", "+20" }, false, Pal::accent),
      attack    ("ATTACK",    { ".1", ".3", "1", "3", "10", "30" }, true, Pal::accent, " ms"),
      release   ("RELEASE",   { ".1", ".3", ".6", "1.2", "AUTO" }, true, Pal::accent, " s"),
      ratio     ("RATIO",     { "2", "4", "10" }, true, Pal::accent, " : 1"),
      hpfFreq   ("S/C HPF",   { "20", "500" }, false, Pal::accent),
      mix       ("MIX",       { "DRY", "WET" }, false, Pal::accent),
      analog    ("ANALOG",    { "CLEAN", "COLOR" }, false, Pal::warm)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (auto* k : { &threshold, &makeup, &attack, &release, &ratio, &hpfFreq, &mix, &analog })
        content.addAndMakeVisible (k);

    bindKnob (threshold, ParamIDs::threshold);
    bindKnob (makeup,    ParamIDs::makeup);
    bindKnob (attack,    ParamIDs::attack);
    bindKnob (release,   ParamIDs::release);
    bindKnob (ratio,     ParamIDs::ratio);
    bindKnob (hpfFreq,   ParamIDs::hpfFreq);
    bindKnob (mix,       ParamIDs::mix);
    bindKnob (analog,    ParamIDs::analog);

    inButton.getProperties().set ("style", "in");
    bindButton (inButton, ParamIDs::bypass,   "IN", true); // lit = compressor engaged (bypass off)
    bindButton (hpfOn,    ParamIDs::hpfOn,    "HPF");
    bindButton (extSc,    ParamIDs::extSc,    "EXT S/C");
    bindButton (scListen, ParamIDs::scListen, "LISTEN");

    inName.setText ("ENGAGE", juce::dontSendNotification);
    inName.setFont (uiFont (11.5f, true));
    inName.setColour (juce::Label::textColourId, Pal::text.withAlpha (0.9f));
    for (auto* l : { &inName, &inState })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setInterceptsMouseClicks (false, false);
        content.addAndMakeVisible (l);
    }
    inState.setFont (uiFont (13.0f, true));

    content.addAndMakeVisible (meter);
    scStatus.setJustificationType (juce::Justification::centred);
    scStatus.setFont (uiFont (11.0f, true));
    content.addAndMakeVisible (scStatus);

    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 3 / 2, baseHeight * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

VBCAudioProcessorEditor::~VBCAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VBCAudioProcessorEditor::bindKnob (KnobCell& k, const char* paramId)
{
    if (auto* p = processor.getParameterByID (paramId))
        bindings.push_back (std::make_unique<ParamBinding> (*p, k.slider));
}

void VBCAudioProcessorEditor::bindButton (juce::ToggleButton& b, const char* paramId, const juce::String& text, bool invert)
{
    b.setButtonText (text);
    content.addAndMakeVisible (b);
    if (auto* p = processor.getParameterByID (paramId))
        bindings.push_back (std::make_unique<ParamBinding> (*p, b, invert));
}

void VBCAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Pal::bg0);
}

void VBCAudioProcessorEditor::resized()
{
    content.setBounds (0, 0, baseWidth, baseHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));

    // left column: 2 x 4 grid (Threshold/Make-up, Attack/Release, Ratio/IN, S/C HPF/Mix)
    const int colX[] { 28, 186 }, cellW = 156, cellH = 172, top = 80;
    auto cell = [&] (int col, int row) { return juce::Rectangle<int> (colX[col], top + row * cellH, cellW, cellH); };
    threshold.setBounds (cell (0, 0));
    makeup   .setBounds (cell (1, 0));
    attack   .setBounds (cell (0, 1));
    release  .setBounds (cell (1, 1));
    ratio    .setBounds (cell (0, 2));
    inButton .setBounds (juce::Rectangle<int> (88, 88).withCentre (cell (1, 2).getCentre().translated (0, -22)));
    inName   .setBounds (cell (1, 2).removeFromBottom (44).removeFromTop (18));
    inState  .setBounds (cell (1, 2).removeFromBottom (26).removeFromTop (18));
    hpfFreq  .setBounds (cell (0, 3));
    mix      .setBounds (cell (1, 3));

    // right column
    meter.setBounds (380, 82, 306, 224);
    analog.setBounds (juce::Rectangle<int> (200, 196).withCentre ({ 533, 460 }));
    hpfOn   .setBounds (382, 666, 96, 38);
    extSc   .setBounds (485, 666, 96, 38);
    scListen.setBounds (588, 666, 96, 38);
    scStatus.setBounds (382, 724, 302, 20);
}

void VBCAudioProcessorEditor::timerCallback()
{
    for (auto& b : bindings)
        b->poll();

    meter.setGainReduction (processor.gainReductionDb.load());

    const bool engaged = inButton.getToggleState();
    inState.setText (engaged ? "ACTIVE" : "BYPASSED", juce::dontSendNotification);
    inState.setColour (juce::Label::textColourId, engaged ? Pal::warm : Pal::dim);

    const bool connected = processor.sidechainConnected.load();
    const bool wantExt   = extSc.getToggleState();
    juce::String status = wantExt ? (connected ? "KEY: EXTERNAL INPUT" : "KEY: EXTERNAL - NOT ROUTED") : "KEY: INTERNAL";
    status << "   |   HPF " << (hpfOn.getToggleState() ? hpfFreq.slider.getTextFromValue (hpfFreq.slider.getValue()) : juce::String ("OFF"));
    scStatus.setText (status, juce::dontSendNotification);
    scStatus.setColour (juce::Label::textColourId, (wantExt && ! connected) ? Pal::hot : Pal::dim);
}
