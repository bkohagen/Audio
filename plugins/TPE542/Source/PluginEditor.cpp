#include "PluginEditor.h"

using namespace kit::theme;

TPE542AudioProcessorEditor::TPE542AudioProcessorEditor (TPE542AudioProcessor& p)
    : PluginEditorBase (p, 720, 768), processor (p),
      trim       ("TRIM",       { "-12", "+12" }, false, accent, " dB"),
      saturation ("SATURATION", { "0", "10" }, false, warm),
      speed      ("SPEED",      { "15", "30" }, true, accent, " IPS"),
      silk       ("SILK",       { "OFF", "RED", "BLUE" }, true, warm),
      texture    ("TEXTURE",    { "0", "10" }, false, warm),
      blend      ("BLEND",      { "DRY", "TAPE" }, false),
      output     ("OUTPUT",     { "-12", "+12" }, false, accent, " dB")
{
    bindKnob (trim,       ParamIDs::trim);
    bindKnob (saturation, ParamIDs::saturation);
    bindKnob (speed,      ParamIDs::speed);
    bindKnob (silk,       ParamIDs::silk);
    bindKnob (texture,    ParamIDs::texture);
    bindKnob (blend,      ParamIDs::blend);
    bindKnob (output,     ParamIDs::output);

    inButton.getProperties().set ("style", "in");
    bindButton (inButton, ParamIDs::bypass, "IN", true); // lit = engaged (bypass off)

    meter.setCaption ("dB  TAPE COMPRESSION");
    getContent().addAndMakeVisible (meter);

    inName.setText ("ENGAGE", juce::dontSendNotification);
    inName.setFont (font (11.5f, true));
    inName.setColour (juce::Label::textColourId, text.withAlpha (0.9f));
    inState.setFont (font (13.0f, true));
    status.setFont (font (11.0f, true));
    status.setColour (juce::Label::textColourId, dim);
    level.setFont (font (11.0f, true));
    level.setColour (juce::Label::textColourId, dim);
    for (auto* l : { &inName, &inState, &status, &level })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setInterceptsMouseClicks (false, false);
        getContent().addAndMakeVisible (l);
    }

    finishSetup();
}

void TPE542AudioProcessorEditor::paintContent (juce::Graphics& g)
{
    drawBackgroundAndHeader (g, getContent().getLocalBounds(), "TPE-542", "TAPE  EMULATOR");

    drawCard (g, { 20.0f, 68.0f, 680.0f, 252.0f });
    drawCard (g, { 20.0f, 336.0f, 680.0f, 204.0f }, "TAPE");
    drawCard (g, { 20.0f, 556.0f, 332.0f, 200.0f }, "SILK");
    drawCard (g, { 368.0f, 556.0f, 332.0f, 200.0f }, "OUTPUT");

    // divider between meter and engage area
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawVerticalLine (362, 90.0f, 298.0f);
}

void TPE542AudioProcessorEditor::layout()
{
    const int cellW = 156, cellH = 172;

    meter.setBounds (36, 82, 306, 224);

    const juce::Point<int> inCentre { 531, 148 };
    inButton.setBounds (juce::Rectangle<int> (88, 88).withCentre (inCentre));
    inName  .setBounds (382, 200, 298, 18);
    inState .setBounds (382, 218, 298, 18);
    status  .setBounds (382, 252, 298, 18);
    level   .setBounds (382, 274, 298, 18);

    const int tapeY = 358;
    trim      .setBounds (55,  tapeY, cellW, cellH);
    saturation.setBounds (282, tapeY, cellW, cellH);
    speed     .setBounds (509, tapeY, cellW, cellH);

    const int lowY = 584;
    silk   .setBounds (28,  lowY, cellW, cellH);
    texture.setBounds (188, lowY, cellW, cellH);
    blend  .setBounds (376, lowY, cellW, cellH);
    output .setBounds (536, lowY, cellW, cellH);
}

void TPE542AudioProcessorEditor::onTimer()
{
    meter.setValue (processor.saturationDb.load());

    const bool engaged = inButton.getToggleState();
    inState.setText (engaged ? "ACTIVE" : "BYPASSED", juce::dontSendNotification);
    inState.setColour (juce::Label::textColourId, engaged ? warm : dim);

    const int silkIdx = juce::roundToInt (silk.slider.getValue());
    const juce::String silkName = silkIdx == 1 ? "RED" : silkIdx == 2 ? "BLUE" : "OFF";
    status.setText ((juce::roundToInt (speed.slider.getValue()) == 1 ? "30 IPS" : "15 IPS")
                        + juce::String ("   |   SILK ") + silkName, juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, silkIdx == 1 ? hot : silkIdx == 2 ? accent : dim);

    const float peakDb = kit::gainToDb (processor.outputPeak.load(), -60.0f);
    level.setText ("OUTPUT PEAK  " + juce::String (peakDb, 1) + " dBFS", juce::dontSendNotification);
}
