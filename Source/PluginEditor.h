#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class VBCLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VBCLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
// Analog-style moving-needle gain-reduction meter (0 .. 20 dB)
class GainReductionMeter final : public juce::Component
{
public:
    void setGainReduction (float db);
    void paint (juce::Graphics&) override;
private:
    float needleDb = 0.0f;
};

//==============================================================================
class VBCAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VBCAudioProcessorEditor (VBCAudioProcessor&);
    ~VBCAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& name, juce::Colour accent);
    void setupToggle (juce::ToggleButton&, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>&,
                      const char* paramId, const juce::String& name);

    VBCAudioProcessor& processor;
    VBCLookAndFeel lnf;

    Knob threshold, ratio, attack, release, makeup, hpfFreq, analog, mix;
    juce::ToggleButton hpfOn, extSc, scListen;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hpfOnAtt, extScAtt, scListenAtt;
    GainReductionMeter meter;
    juce::Label scStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VBCAudioProcessorEditor)
};
