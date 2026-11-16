#pragma once

#include <kit/ui/Components.h>
#include "PluginProcessor.h"

/*  Editor: built on kit::PluginEditorBase (fixed base size, scaled to the window, 30 Hz polling,
    no parameter listeners -> real-time safe). */
class TPE542AudioProcessorEditor final : public kit::PluginEditorBase
{
public:
    explicit TPE542AudioProcessorEditor (TPE542AudioProcessor&);

private:
    void layout() override;
    void paintContent (juce::Graphics&) override;
    void onTimer() override;

    TPE542AudioProcessor& processor;

    kit::KnobCell trim, saturation, speed, silk, texture, blend, output;
    kit::GainReductionMeter meter { 12.0f };
    juce::ToggleButton inButton;
    juce::Label inName, inState, status, level;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TPE542AudioProcessorEditor)
};
