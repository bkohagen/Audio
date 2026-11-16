// Shared UI components. Everything here runs on the MESSAGE thread only and never registers
// listeners on parameters (see ParamBinding), so nothing here can touch the audio thread.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace kit
{

//==============================================================================
/** Knobs: value arc + tick dots + flat cap. Toggle buttons: LED pills, or a big illuminated
    switch when the button has property  style = "in". */
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

//==============================================================================
/*  Binds a Slider or ToggleButton to a parameter WITHOUT registering a parameter listener.

    JUCE's Slider/ButtonAttachment (and AudioProcessorValueTreeState) listen to the parameter;
    when the host automates it inside the audio callback, they lock and post (allocating)
    messages from the audio thread. Instead:
      * GUI -> host: begin/endChangeGesture + setValueNotifyingHost on the message thread.
      * host -> GUI: poll() reads the parameter's atomic value from the editor's timer.
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
/** A knob with name, value readout and hardware-style scale labels around it.
    stepped = true: one tick per label, the selected label lights up (for choice params). */
class KnobCell final : public juce::Component, private juce::Slider::Listener
{
public:
    KnobCell (const juce::String& name, juce::StringArray scaleLabels, bool stepped,
              juce::Colour accent = theme::accent, const juce::String& unitSuffix = {});
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
};

//==============================================================================
/** Arc meter for gain reduction: 0 dB on the LEFT, maxDb on the right; needle, zone-coloured
    fill arc, peak-hold marker and numeric readout. Call setValue() from the UI timer (~30 Hz). */
class GainReductionMeter final : public juce::Component
{
public:
    explicit GainReductionMeter (float maxDb = 20.0f) : maxDb (maxDb) {}
    void setValue (float gainReductionDb);
    void setCaption (const juce::String& c) { caption = c; }
    void paint (juce::Graphics&) override;

private:
    float maxDb;
    float needleDb = 0.0f, peakDb = 0.0f;
    int   peakHoldTicks = 0;
    juce::String caption { "dB  GAIN REDUCTION" };
};

//==============================================================================
/** Base class for every plugin editor:
      * lays out at a fixed base size inside `content`, scaled to the window with a transform
        (resizable 75–150 %, fixed aspect ratio);
      * owns the LookAndFeel and the ParamBindings and polls them at 30 Hz;
      * subclasses implement layout(), paintContent() and (optionally) onTimer() for meters.
*/
class PluginEditorBase : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    PluginEditorBase (juce::AudioProcessor&, int baseWidth, int baseHeight);
    ~PluginEditorBase() override;

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    /** Lay out children of `content` in base coordinates. */
    virtual void layout() = 0;
    /** Paint the background/cards of `content` in base coordinates. */
    virtual void paintContent (juce::Graphics&) = 0;
    /** Called at 30 Hz after the bindings are polled: update meters/status here. */
    virtual void onTimer() {}

    void bindKnob (KnobCell&, const juce::String& paramId);
    void bindButton (juce::ToggleButton&, const juce::String& paramId, const juce::String& text, bool invert = false);
    juce::Component& getContent() noexcept { return content; }

    /** Call at the end of the subclass constructor (after adding children). */
    void finishSetup();

    const int baseWidth, baseHeight;

private:
    void timerCallback() override;

    struct Content final : public juce::Component
    {
        explicit Content (PluginEditorBase& o) : owner (o) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        PluginEditorBase& owner;
    };

    juce::AudioProcessor& processorRef;
    LookAndFeel lnf;
    Content content { *this };
    std::vector<std::unique_ptr<ParamBinding>> bindings;
};

//==============================================================================
// Parameter helpers (message thread)
juce::RangedAudioParameter* findParameter (const juce::AudioProcessor&, const juce::String& paramId);

/** Save every parameter as <TAG><PARAM id=".." value=".."/></TAG> (plain values). */
void saveParameterState (const juce::AudioProcessor&, const juce::Identifier& tag, juce::MemoryBlock& dest);
/** Restore a state written by saveParameterState(); unknown ids are ignored. */
void loadParameterState (juce::AudioProcessor&, const juce::Identifier& tag, const void* data, int size);

} // namespace kit
