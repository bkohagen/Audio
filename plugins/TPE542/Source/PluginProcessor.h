#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <kit/ui/Components.h>
#include "PluginDSP.h"

namespace ParamIDs
{
    inline constexpr const char* trim       = "trim";
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* speed      = "speed";
    inline constexpr const char* silk       = "silk";
    inline constexpr const char* texture    = "texture";
    inline constexpr const char* blend      = "blend";
    inline constexpr const char* output     = "output";
    inline constexpr const char* bypass     = "bypass";
}

/*  REAL-TIME SAFETY (enforced by TPE542_rt_safety_test):
    * processBlock() never allocates, frees, locks, logs or touches the message thread.
    * Parameters are plain JUCE parameter objects (atomic values); no AudioProcessorValueTreeState,
      no Slider/ButtonAttachment. The editor uses kit::ParamBinding (polling) instead.
    * GUI <-> audio only through std::atomic values. */
class TPE542AudioProcessor final : public juce::AudioProcessor
{
public:
    TPE542AudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Host bypass (VST3/AU/AAX) maps to this parameter, so bypass is crossfaded by our DSP.
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }

    // Meters for the GUI (written by the audio thread, read by the UI timer)
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<float> saturationDb { 0.0f };

private:
    tpe542::Processor dsp;

    juce::AudioParameterFloat*  trim       = nullptr;
    juce::AudioParameterFloat*  saturation = nullptr;
    juce::AudioParameterChoice* speed      = nullptr;
    juce::AudioParameterChoice* silk       = nullptr;
    juce::AudioParameterFloat*  texture    = nullptr;
    juce::AudioParameterFloat*  blend      = nullptr;
    juce::AudioParameterFloat*  output     = nullptr;
    juce::AudioParameterBool*   bypass     = nullptr;

    tpe542::Parameters readParameters() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TPE542AudioProcessor)
};
