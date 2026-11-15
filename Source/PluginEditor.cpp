#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour panel   { 0xff2b2d31 };
    const juce::Colour panelHi { 0xff3a3d43 };
    const juce::Colour text    { 0xffe6e2d8 };
    const juce::Colour dim     { 0xff9a968c };
    const juce::Colour red     { 0xffd2473a };
    const juce::Colour blue    { 0xff4a7fc1 };
    const juce::Colour grey    { 0xffb8b4aa };
    const juce::Colour amber   { 0xffe0a43a };
    const juce::Colour meterBg { 0xfff1e6c8 };
}

//==============================================================================
VBCLookAndFeel::VBCLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Pal::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1f22));
    setColour (juce::Label::textColourId, Pal::text);
    setColour (juce::ToggleButton::textColourId, Pal::text);
}

void VBCLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& s)
{
    using namespace juce;
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);

    // scale ticks
    const int ticks = s.getInterval() >= 1.0 && s.getMaximum() - s.getMinimum() <= 10 ? (int) (s.getMaximum() - s.getMinimum()) + 1 : 11;
    g.setColour (Pal::dim);
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (float) i / (float) (ticks - 1) * (endAngle - startAngle);
        const auto p1 = centre.getPointOnCircumference (radius * 0.98f, a);
        const auto p2 = centre.getPointOnCircumference (radius * 0.86f, a);
        g.drawLine ({ p1, p2 }, 1.2f);
    }

    // knob body
    const float kr = radius * 0.78f;
    g.setGradientFill (ColourGradient (Colour (0xff55585e), centre.x, centre.y - kr,
                                       Colour (0xff1b1c1f), centre.x, centre.y + kr, false));
    g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2, kr * 2);
    g.setColour (Pal::panelHi.brighter (0.2f));
    g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2, kr * 2, 1.0f);

    // coloured cap
    const float cr = kr * 0.55f;
    g.setColour (accent);
    g.fillEllipse (centre.x - cr, centre.y - cr, cr * 2, cr * 2);

    // pointer
    Path p;
    p.addRoundedRectangle (-1.6f, -kr, 3.2f, kr * 0.62f, 1.5f);
    g.setColour (Pal::text);
    g.fillPath (p, AffineTransform::rotation (angle).translated (centre));
}

void VBCLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::amber : Colour (0xff1e1f22));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (highlighted ? Pal::text : Pal::dim);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? Colour (0xff1e1f22) : Pal::text);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), Justification::centred, 1);
}

//==============================================================================
void GainReductionMeter::setGainReduction (float db)
{
    // mechanical needle ballistics (~300 ms VU-like), drawn at GUI rate
    const float target = juce::jlimit (0.0f, 20.0f, db);
    needleDb += (target - needleDb) * (target > needleDb ? 0.45f : 0.18f);
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Colour (0xff111214));
    g.fillRoundedRectangle (r, 6.0f);
    auto face = r.reduced (6.0f);
    g.setColour (Pal::meterBg);
    g.fillRoundedRectangle (face, 4.0f);

    const auto pivot = Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.6f);
    const float len = face.getHeight() * 1.45f;
    const float a0 = -0.55f, a1 = 0.55f; // radians: 0 dB on the right, 20 dB on the left
    auto angleFor = [&] (float db) { return a1 - (db / 20.0f) * (a1 - a0); };

    g.setFont (FontOptions (10.0f));
    for (float db : { 0.0f, 4.0f, 8.0f, 12.0f, 16.0f, 20.0f })
    {
        const float a = angleFor (db);
        const auto p1 = pivot.getPointOnCircumference (len * 0.92f, a);
        const auto p2 = pivot.getPointOnCircumference (len * 0.82f, a);
        g.setColour (db >= 12.0f ? Pal::red : Colour (0xff222222));
        g.drawLine ({ p1, p2 }, 1.5f);
        const auto tp = pivot.getPointOnCircumference (len * 0.75f, a);
        g.drawText (String ((int) db), Rectangle<float> (20, 12).withCentre (tp), Justification::centred);
    }

    g.setColour (Colour (0xff222222));
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("GAIN REDUCTION dB", face.removeFromBottom (22.0f).translated (0.0f, -4.0f), Justification::centred);

    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (face.toNearestInt());
    g.setColour (Colour (0xff101010));
    g.drawLine ({ pivot, pivot.getPointOnCircumference (len * 0.95f, angleFor (needleDb)) }, 1.6f);
}

//==============================================================================
VBCAudioProcessorEditor::VBCAudioProcessorEditor (VBCAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    setupKnob (threshold, ParamIDs::threshold, "THRESHOLD", Pal::red);
    setupKnob (ratio,     ParamIDs::ratio,     "RATIO",     Pal::grey);
    setupKnob (attack,    ParamIDs::attack,    "ATTACK ms", Pal::grey);
    setupKnob (release,   ParamIDs::release,   "RELEASE s", Pal::grey);
    setupKnob (makeup,    ParamIDs::makeup,    "MAKE-UP",   Pal::red);
    setupKnob (hpfFreq,   ParamIDs::hpfFreq,   "SC HPF",    Pal::blue);
    setupKnob (analog,    ParamIDs::analog,    "ANALOG",    Pal::amber);
    setupKnob (mix,       ParamIDs::mix,       "MIX",       Pal::grey);

    setupToggle (hpfOn,    hpfOnAtt,    ParamIDs::hpfOn,    "HPF");
    setupToggle (extSc,    extScAtt,    ParamIDs::extSc,    "EXT SC");
    setupToggle (scListen, scListenAtt, ParamIDs::scListen, "LISTEN");

    addAndMakeVisible (meter);
    scStatus.setJustificationType (juce::Justification::centred);
    scStatus.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (scStatus);

    setSize (820, 330);
    startTimerHz (30);
}

VBCAudioProcessorEditor::~VBCAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VBCAudioProcessorEditor::setupKnob (Knob& k, const char* paramId, const juce::String& name, juce::Colour accent)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, k.slider);
}

void VBCAudioProcessorEditor::setupToggle (juce::ToggleButton& b,
                                           std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                                           const char* paramId, const juce::String& name)
{
    b.setButtonText (name);
    addAndMakeVisible (b);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, paramId, b);
}

void VBCAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Pal::panelHi, 0, 0, Pal::panel, 0, (float) getHeight(), false));
    g.fillAll();

    g.setColour (Pal::text);
    g.setFont (FontOptions (20.0f, Font::bold));
    g.drawText ("VBC-1", 18, 10, 120, 26, Justification::centredLeft);
    g.setColour (Pal::dim);
    g.setFont (FontOptions (12.0f));
    g.drawText ("STEREO VCA BUS COMPRESSOR  -  ZERO LATENCY", 90, 12, 400, 24, Justification::centredLeft);

    // section frames
    g.setColour (Colour (0x30ffffff));
    g.drawRoundedRectangle (12.0f, 44.0f, 510.0f, 274.0f, 6.0f, 1.0f);
    g.drawRoundedRectangle (530.0f, 200.0f, 278.0f, 118.0f, 6.0f, 1.0f);
    g.setColour (Pal::dim);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("SIDECHAIN", 540, 204, 120, 14, Justification::centredLeft);
}

void VBCAudioProcessorEditor::resized()
{
    const int kw = 120, kh = 130;
    auto place = [&] (Knob& k, int x, int y)
    {
        k.label.setBounds (x, y, kw, 16);
        k.slider.setBounds (x, y + 16, kw, kh - 16);
    };

    // top row: dynamics
    place (threshold, 22,  50);
    place (ratio,     142, 50);
    place (attack,    262, 50);
    place (release,   382, 50);
    // bottom row: output / character
    place (makeup,    22,  185);
    place (analog,    142, 185);
    place (mix,       262, 185);

    meter.setBounds (530, 44, 278, 150);

    // sidechain section
    hpfFreq.label.setBounds (540, 220, 100, 14);
    hpfFreq.slider.setBounds (540, 234, 100, 80);
    hpfOn.setBounds    (652, 228, 70, 24);
    extSc.setBounds    (730, 228, 70, 24);
    scListen.setBounds (652, 258, 148, 24);
    scStatus.setBounds (652, 288, 148, 20);
}

void VBCAudioProcessorEditor::timerCallback()
{
    meter.setGainReduction (processor.gainReductionDb.load());

    const bool connected = processor.sidechainConnected.load();
    const bool wantExt   = extSc.getToggleState();
    scStatus.setText (wantExt ? (connected ? "ext key: connected" : "ext key: NOT routed") : "key: internal",
                      juce::dontSendNotification);
    scStatus.setColour (juce::Label::textColourId, (wantExt && ! connected) ? Pal::red : Pal::dim);
}
