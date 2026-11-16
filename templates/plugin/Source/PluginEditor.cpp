#include "PluginEditor.h"

using namespace kit::theme;

__CLASS__AudioProcessorEditor::__CLASS__AudioProcessorEditor (__CLASS__AudioProcessor& p)
    : PluginEditorBase (p, 720, 400), processor (p),
      input  ("INPUT",  { "-24", "+24" }, false),
      output ("OUTPUT", { "-24", "+24" }, false),
      mix    ("MIX",    { "DRY", "WET" }, false)
{
    bindKnob (input,  ParamIDs::input);
    bindKnob (output, ParamIDs::output);
    bindKnob (mix,    ParamIDs::mix);

    inButton.getProperties().set ("style", "in");
    bindButton (inButton, ParamIDs::bypass, "IN", true); // lit = engaged (bypass off)

    inName.setText ("ENGAGE", juce::dontSendNotification);
    inName.setFont (font (11.5f, true));
    inState.setFont (font (13.0f, true));
    level.setFont (font (11.0f, true));
    level.setColour (juce::Label::textColourId, dim);
    for (auto* l : { &inName, &inState, &level })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setInterceptsMouseClicks (false, false);
        getContent().addAndMakeVisible (l);
    }

    finishSetup();
}

void __CLASS__AudioProcessorEditor::paintContent (juce::Graphics& g)
{
    drawBackgroundAndHeader (g, getContent().getLocalBounds(), "__SHORT_NAME__", "__SUBTITLE__", "__BADGE__");
    drawCard (g, { 20.0f, 68.0f, 680.0f, 312.0f });
}

void __CLASS__AudioProcessorEditor::layout()
{
    const int cellW = 156, cellH = 172, top = 100;
    input .setBounds (40,  top, cellW, cellH);
    output.setBounds (200, top, cellW, cellH);
    mix   .setBounds (360, top, cellW, cellH);
    const juce::Rectangle<int> inCell (528, top, cellW, cellH);
    inButton.setBounds (juce::Rectangle<int> (88, 88).withCentre (inCell.getCentre().translated (0, -22)));
    inName  .setBounds (inCell.withTrimmedTop (cellH - 44).withHeight (18));
    inState .setBounds (inCell.withTrimmedTop (cellH - 26).withHeight (18));
    level   .setBounds (40, 316, 640, 20);
}

void __CLASS__AudioProcessorEditor::onTimer()
{
    const bool engaged = inButton.getToggleState();
    inState.setText (engaged ? "ACTIVE" : "BYPASSED", juce::dontSendNotification);
    inState.setColour (juce::Label::textColourId, engaged ? warm : dim);

    const float peakDb = kit::gainToDb (processor.outputPeak.load(), -60.0f);
    level.setText ("OUTPUT PEAK  " + juce::String (peakDb, 1) + " dBFS", juce::dontSendNotification);
}
