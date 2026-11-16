#include "PluginEditor.h"

using namespace kit::theme;

VBCAudioProcessorEditor::VBCAudioProcessorEditor (VBCAudioProcessor& p)
    : PluginEditorBase (p, 720, 800), processor (p),
      threshold ("THRESHOLD", { "-30", "+6" }, false),
      makeup    ("MAKE-UP",   { "-10", "+20" }, false),
      attack    ("ATTACK",    { ".1", ".3", "1", "3", "10", "30" }, true, accent, " ms"),
      release   ("RELEASE",   { ".1", ".3", ".6", "1.2", "AUTO" }, true, accent, " s"),
      ratio     ("RATIO",     { "2", "4", "10" }, true, accent, " : 1"),
      hpfFreq   ("S/C HPF",   { "20", "500" }, false),
      mix       ("MIX",       { "DRY", "WET" }, false),
      analog    ("ANALOG",    { "CLEAN", "COLOR" }, false, warm)
{
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
    inName.setFont (font (11.5f, true));
    inName.setColour (juce::Label::textColourId, text.withAlpha (0.9f));
    inState.setFont (font (13.0f, true));
    scStatus.setFont (font (11.0f, true));
    for (auto* l : { &inName, &inState, &scStatus })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setInterceptsMouseClicks (false, false);
        getContent().addAndMakeVisible (l);
    }
    getContent().addAndMakeVisible (meter);

    finishSetup();
}

void VBCAudioProcessorEditor::paintContent (juce::Graphics& g)
{
    drawBackgroundAndHeader (g, getContent().getLocalBounds(), "VBC-1", "VCA  BUS  COMPRESSOR");

    drawCard (g, { 20.0f, 68.0f, 330.0f, 712.0f });
    drawCard (g, { 366.0f, 68.0f, 334.0f, 252.0f });
    drawCard (g, { 366.0f, 336.0f, 334.0f, 262.0f }, "CHARACTER");
    drawCard (g, { 366.0f, 614.0f, 334.0f, 166.0f }, "SIDECHAIN");

    g.setColour (juce::Colours::white.withAlpha (0.04f));
    for (int row = 1; row < 4; ++row)
        g.drawHorizontalLine (68 + 12 + row * 172, 40.0f, 330.0f);

    g.setColour (faint);
    g.setFont (font (10.0f));
    g.drawText ("VCA + output-stage saturation", juce::Rectangle<float> (366.0f, 566.0f, 334.0f, 18.0f), juce::Justification::centred);
}

void VBCAudioProcessorEditor::layout()
{
    // left column: 2 x 4 grid
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

void VBCAudioProcessorEditor::onTimer()
{
    meter.setValue (processor.gainReductionDb.load());

    const bool engaged = inButton.getToggleState();
    inState.setText (engaged ? "ACTIVE" : "BYPASSED", juce::dontSendNotification);
    inState.setColour (juce::Label::textColourId, engaged ? warm : dim);

    const bool connected = processor.sidechainConnected.load();
    const bool wantExt   = extSc.getToggleState();
    juce::String status = wantExt ? (connected ? "KEY: EXTERNAL INPUT" : "KEY: EXTERNAL - NOT ROUTED") : "KEY: INTERNAL";
    status << "   |   HPF " << (hpfOn.getToggleState() ? hpfFreq.slider.getTextFromValue (hpfFreq.slider.getValue()) : juce::String ("OFF"));
    scStatus.setText (status, juce::dontSendNotification);
    scStatus.setColour (juce::Label::textColourId, (wantExt && ! connected) ? hot : dim);
}
