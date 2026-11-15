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
/*  Binds a Slider or ToggleButton to a parameter WITHOUT registering a parameter listener.

    JUCE's Slider/ButtonAttachment listen to the parameter; when the host automates it from
    the audio thread, the attachment locks and posts (allocating) a message to the message
    queue from inside the audio callback. Instead we:
      * GUI -> host: begin/endChangeGesture + setValueNotifyingHost on the message thread.
      * host -> GUI: poll the parameter's atomic value from the editor's timer.
    The audio thread therefore never touches anything the GUI owns.
*/
class ParamBinding
{
public:
    ParamBinding (juce::RangedAudioParameter& p, juce::Slider& s);
    ParamBinding (juce::RangedAudioParameter& p, juce::Button& b);
    void poll(); // message thread only

private:
    void sendSliderValue();
    juce::RangedAudioParameter& param;
    juce::Slider* slider = nullptr;
    juce::Button* button = nullptr;
    bool dragging = false;
    float lastPolled = -1.0f;
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
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& name, juce::Colour accent);
    void setupToggle (juce::ToggleButton&, const char* paramId, const juce::String& name);

    VBCAudioProcessor& processor;
    VBCLookAndFeel lnf;

    Knob threshold, ratio, attack, release, makeup, hpfFreq, analog, mix;
    juce::ToggleButton hpfOn, extSc, scListen, bypass;
    std::vector<std::unique_ptr<ParamBinding>> bindings;
    GainReductionMeter meter;
    juce::Label scStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VBCAudioProcessorEditor)
};
