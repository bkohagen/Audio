#include "Components.h"

namespace kit
{
using namespace theme;

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Label::textColourId, text);
    setColour (juce::ToggleButton::textColourId, text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    using namespace juce;
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto acc = s.findColour (Slider::rotarySliderFillColourId);
    const int ticks = (int) s.getProperties().getWithDefault ("ticks", 11);

    // tick dots
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (float) i / (float) jmax (1, ticks - 1) * (endAngle - startAngle);
        const auto p = c.getPointOnCircumference (r * 0.98f, a);
        g.setColour (a <= angle + 1.0e-3f ? acc.withAlpha (0.9f) : faint);
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
        g.setColour (acc.withAlpha (0.25f));
        g.strokePath (val, PathStrokeType (8.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (acc);
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
    g.setColour (text);
    g.drawLine ({ c.getPointOnCircumference (cr * 0.35f, angle), c.getPointOnCircumference (cr * 0.88f, angle) }, 3.0f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();

    if (b.getProperties().getWithDefault ("style", "") == var ("in"))
    {
        // illuminated square switch
        if (on)
        {
            for (int i = 3; i > 0; --i)
            {
                g.setColour (warm.withAlpha (0.07f * (float) i));
                g.fillRoundedRectangle (r, 12.0f);
            }
            g.setGradientFill (ColourGradient (warm.brighter (0.35f), r.getCentreX(), r.getY(),
                                               warm.darker (0.25f), r.getCentreX(), r.getBottom(), false));
        }
        else
        {
            g.setGradientFill (ColourGradient (Colour (0xff2b2f36), r.getCentreX(), r.getY(),
                                               Colour (0xff1d2025), r.getCentreX(), r.getBottom(), false));
        }
        g.fillRoundedRectangle (r.reduced (6.0f), 9.0f);
        g.setColour (Colours::white.withAlpha (on ? 0.35f : (highlighted ? 0.18f : 0.08f)));
        g.drawRoundedRectangle (r.reduced (6.5f), 9.0f, 1.0f);
        g.setColour (on ? Colour (0xff2a1a00) : dim);
        g.setFont (font (17.0f, true));
        g.drawText (b.getButtonText(), r.translated (0.0f, down ? 1.0f : 0.0f), Justification::centred);
        return;
    }

    // LED pill button
    g.setColour (on ? accent.withAlpha (0.14f) : Colour (0xff22262c));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (on ? accent.withAlpha (0.55f) : Colours::white.withAlpha (highlighted ? 0.16f : 0.07f));
    g.drawRoundedRectangle (r, 9.0f, 1.0f);

    const auto led = Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 15.0f, r.getCentreY() });
    if (on)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.5f));
    }
    g.setColour (on ? accent.brighter (0.3f) : faint);
    g.fillEllipse (led);

    g.setColour (on ? text : dim);
    g.setFont (font (11.5f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (24.0f).withTrimmedRight (6.0f), Justification::centred);
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
    s.updateText(); // value may equal the slider's initial 0, so force the formatted text once
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
    slider.getProperties().set ("ticks", stepped ? labels.size() : 11);
    slider.addListener (this);
    addAndMakeVisible (slider);
}

KnobCell::~KnobCell() { slider.removeListener (this); }

void KnobCell::resized()
{
    auto b = getLocalBounds();
    auto knobArea = b.removeFromTop (b.getHeight() - 44).reduced (4);
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
    g.setFont (font (10.5f, true));
    for (int i = 0; i < labels.size(); ++i)
    {
        const float p = labels.size() > 1 ? (float) i / (float) (labels.size() - 1) : 0.0f;
        const float a = kKnobStart + p * (kKnobEnd - kKnobStart);
        g.setColour (i == selected ? text : dim.withAlpha (0.8f));
        g.drawText (labels[i], Rectangle<float> (34.0f, 14.0f).withCentre (c.getPointOnCircumference (r + 13.0f, a)),
                    Justification::centred);
    }

    // name + value
    auto area = getLocalBounds().toFloat().removeFromBottom (44.0f);
    g.setColour (text.withAlpha (0.9f));
    g.setFont (font (11.5f, true));
    g.drawText (name, area.removeFromTop (18.0f), Justification::centred);

    auto value = slider.getTextFromValue (slider.getValue());
    if (suffix.isNotEmpty() && value.containsOnly ("0123456789.-+"))
        value << suffix;
    g.setColour (accent);
    g.setFont (font (13.0f, true));
    g.drawText (value, area.removeFromTop (18.0f), Justification::centred);
}

//==============================================================================
void GainReductionMeter::setValue (float db)
{
    const float target = juce::jlimit (0.0f, maxDb, db);
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
    g.setColour (display);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setGradientFill (ColourGradient (accent.withAlpha (0.10f), bounds.getCentreX(), bounds.getBottom(),
                                       Colours::transparentBlack, bounds.getCentreX(), bounds.getY(), true));
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    const float a0 = -1.25f, a1 = 1.25f;                                // 0 dB on the LEFT
    const float R = jmin (bounds.getWidth() * 0.5f - 52.0f, bounds.getHeight() * 0.45f);
    const float figureH = R + 36.0f + (-0.3f * R + 46.0f);              // labels .. caption
    const auto pivot = Point<float> (bounds.getCentreX(), bounds.getY() + (bounds.getHeight() - figureH) * 0.5f + 36.0f + R);
    auto angleFor = [&] (float db) { return a0 + (db / maxDb) * (a1 - a0); };
    const float z1 = maxDb * 0.3f, z2 = maxDb * 0.6f;
    auto zoneColour = [&] (float db) { return db < z1 ? accent : (db < z2 ? warm : hot); };

    auto arc = [&] (float fromDb, float toDb, float radius, float thickness, Colour col)
    {
        Path p;
        p.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (fromDb), angleFor (toDb), true);
        g.setColour (col);
        g.strokePath (p, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::butt));
    };
    arc (0.0f, maxDb, R, 6.0f, Colours::white.withAlpha (0.06f));
    arc (0.0f, z1,    R + 9.0f, 2.0f, accent.withAlpha (0.45f));
    arc (z1, z2,      R + 9.0f, 2.0f, warm.withAlpha (0.45f));
    arc (z2, maxDb,   R + 9.0f, 2.0f, hot.withAlpha (0.45f));

    if (needleDb > 0.05f)
    {
        arc (0.0f, needleDb, R, 14.0f, zoneColour (needleDb).withAlpha (0.18f));
        arc (0.0f, needleDb, R, 6.0f, zoneColour (needleDb));
    }

    // ticks + labels: minor every maxDb/10, labels every maxDb/5
    g.setFont (font (11.0f, true));
    for (int i = 0; i <= 10; ++i)
    {
        const float db = maxDb * (float) i / 10.0f;
        const float a = angleFor (db);
        const bool major = i % 2 == 0;
        g.setColour (major ? text.withAlpha (0.85f) : faint);
        g.drawLine ({ pivot.getPointOnCircumference (R + 14.0f, a), pivot.getPointOnCircumference (R + (major ? 24.0f : 19.0f), a) },
                    major ? 1.5f : 1.0f);
        if (major)
        {
            g.setColour (dim);
            g.drawText (String (roundToInt (db)), Rectangle<float> (26.0f, 14.0f).withCentre (pivot.getPointOnCircumference (R + 36.0f, a)),
                        Justification::centred);
        }
    }

    if (peakDb > 0.2f)
    {
        const float a = angleFor (peakDb);
        g.setColour (warm);
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

    const auto readout = Rectangle<float> (R * 1.2f, 40.0f).withCentre ({ pivot.x, pivot.y - R * 0.30f });
    g.setColour (needleDb > 0.05f ? text : dim);
    g.setFont (font (34.0f, true));
    g.drawText (needleDb > 0.05f ? "-" + String (needleDb, 1) : "0.0", readout, Justification::centred);
    g.setColour (dim);
    g.setFont (font (10.5f, true));
    g.drawText (caption, readout.translated (0.0f, 30.0f), Justification::centred);
}

//==============================================================================
PluginEditorBase::PluginEditorBase (juce::AudioProcessor& p, int w, int h)
    : AudioProcessorEditor (&p), baseWidth (w), baseHeight (h), processorRef (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
}

PluginEditorBase::~PluginEditorBase()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PluginEditorBase::finishSetup()
{
    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 3 / 2, baseHeight * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

void PluginEditorBase::paint (juce::Graphics& g) { g.fillAll (bg0); }

void PluginEditorBase::resized()
{
    content.setBounds (0, 0, baseWidth, baseHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));
    layout();
}

void PluginEditorBase::bindKnob (KnobCell& k, const juce::String& paramId)
{
    content.addAndMakeVisible (k);
    if (auto* p = findParameter (processorRef, paramId))
        bindings.push_back (std::make_unique<ParamBinding> (*p, k.slider));
    else
        jassertfalse; // unknown parameter id
}

void PluginEditorBase::bindButton (juce::ToggleButton& b, const juce::String& paramId, const juce::String& txt, bool invert)
{
    b.setButtonText (txt);
    content.addAndMakeVisible (b);
    if (auto* p = findParameter (processorRef, paramId))
        bindings.push_back (std::make_unique<ParamBinding> (*p, b, invert));
    else
        jassertfalse; // unknown parameter id
}

void PluginEditorBase::timerCallback()
{
    for (auto& b : bindings)
        b->poll();
    onTimer();
}

//==============================================================================
juce::RangedAudioParameter* findParameter (const juce::AudioProcessor& proc, const juce::String& id)
{
    for (auto* p : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (r->getParameterID() == id)
                return r;
    return nullptr;
}

void saveParameterState (const juce::AudioProcessor& proc, const juce::Identifier& tag, juce::MemoryBlock& dest)
{
    juce::XmlElement xml (tag);
    for (auto* p : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", r->getParameterID());
            e->setAttribute ("value", (double) r->convertFrom0to1 (r->getValue()));
        }
    juce::AudioProcessor::copyXmlToBinary (xml, dest);
}

void loadParameterState (juce::AudioProcessor& proc, const juce::Identifier& tag, const void* data, int size)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (tag))
        return;

    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* r = findParameter (proc, e->getStringAttribute ("id")))
            r->setValueNotifyingHost (r->convertTo0to1 ((float) e->getDoubleAttribute ("value", r->convertFrom0to1 (r->getDefaultValue()))));
}

} // namespace kit
