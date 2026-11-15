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
// Gain-reduction meter: 0 dB on the LEFT, 20 dB on the right, needle + fill arc,
// peak-hold marker and numeric readout.
class GainReductionMeter final : public juce::Component
{
public:
    void setGainReduction (float db); // call from the UI timer (~30 Hz)
    void paint (juce::Graphics&) override;
private:
    float needleDb = 0.0f, peakDb = 0.0f;
    int   peakHoldTicks = 0;
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
    ParamBinding (juce::RangedAudioParameter& p, juce::Button& b, bool invert = false);
    void poll(); // message thread only

private:
    void sendSliderValue();
    juce::RangedAudioParameter& param;
    juce::Slider* slider = nullptr;
    juce::Button* button = nullptr;
    bool invert = false;
    bool dragging = false;
    float lastPolled = -1.0f;
};

//==============================================================================
// A knob with its name, value readout and hardware-style scale labels around it.
class KnobCell final : public juce::Component, private juce::Slider::Listener
{
public:
    KnobCell (const juce::String& name, juce::StringArray scaleLabels, bool stepped,
              juce::Colour accent, const juce::String& unitSuffix = {});
    ~KnobCell() override;

    juce::Slider slider;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    juce::String name, suffix;
    juce::StringArray labels;
    bool stepped;
    juce::Colour accent;
    juce::Rectangle<int> knobArea;
};

//==============================================================================
class VBCAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VBCAudioProcessorEditor (VBCAudioProcessor&);
    ~VBCAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 720, baseHeight = 800;

private:
    void timerCallback() override;

    // Everything is laid out at a fixed base size inside this component, which is then
    // scaled with an AffineTransform so the window can be resized freely.
    struct Content final : public juce::Component
    {
        void paint (juce::Graphics&) override;
    };

    void bindKnob (KnobCell&, const char* paramId);
    void bindButton (juce::ToggleButton&, const char* paramId, const juce::String& text, bool invert = false);

    VBCAudioProcessor& processor;
    VBCLookAndFeel lnf;
    Content content;

    KnobCell threshold, makeup, attack, release, ratio, hpfFreq, mix, analog;
    juce::ToggleButton inButton, hpfOn, extSc, scListen;
    GainReductionMeter meter;
    juce::Label scStatus, inName, inState;

    std::vector<std::unique_ptr<ParamBinding>> bindings; // declared after the controls they bind

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VBCAudioProcessorEditor)
};
