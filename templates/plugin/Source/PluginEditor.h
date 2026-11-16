#pragma once

#include <kit/ui/Components.h>
#include "PluginProcessor.h"

/*  Editor: built on kit::PluginEditorBase (fixed base size, scaled to the window, 30 Hz polling,
    no parameter listeners -> real-time safe). Add controls as members, bind them in the
    constructor, position them in layout(), draw cards in paintContent(). */
class __CLASS__AudioProcessorEditor final : public kit::PluginEditorBase
{
public:
    explicit __CLASS__AudioProcessorEditor (__CLASS__AudioProcessor&);

private:
    void layout() override;
    void paintContent (juce::Graphics&) override;
    void onTimer() override;

    __CLASS__AudioProcessor& processor;

    kit::KnobCell input, output, mix;
    juce::ToggleButton inButton;
    juce::Label inName, inState, level;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (__CLASS__AudioProcessorEditor)
};
