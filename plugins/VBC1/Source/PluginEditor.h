#pragma once

#include <kit/ui/Components.h>
#include "PluginProcessor.h"

/*  VBC-1 editor. Portrait two-column layout:
      left  : 2 x 4 knob grid (Threshold/Make-up, Attack/Release, Ratio/IN, S/C HPF/Mix)
      right : GR meter, Character (Analog), Sidechain buttons
    All parameter wiring goes through kit::PluginEditorBase (no parameter listeners -> RT safe). */
class VBCAudioProcessorEditor final : public kit::PluginEditorBase
{
public:
    explicit VBCAudioProcessorEditor (VBCAudioProcessor&);

private:
    void layout() override;
    void paintContent (juce::Graphics&) override;
    void onTimer() override;

    VBCAudioProcessor& processor;

    kit::KnobCell threshold, makeup, attack, release, ratio, hpfFreq, mix, analog;
    juce::ToggleButton inButton, hpfOn, extSc, scListen;
    kit::GainReductionMeter meter;
    juce::Label scStatus, inName, inState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VBCAudioProcessorEditor)
};
