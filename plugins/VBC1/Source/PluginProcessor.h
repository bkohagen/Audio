#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <kit/ui/Components.h>
#include "BusCompDSP.h"

namespace ParamIDs
{
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* ratio     = "ratio";
    inline constexpr const char* attack    = "attack";
    inline constexpr const char* release   = "release";
    inline constexpr const char* makeup    = "makeup";
    inline constexpr const char* hpfOn     = "hpfOn";
    inline constexpr const char* hpfFreq   = "hpfFreq";
    inline constexpr const char* analog    = "analog";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* extSc     = "extSc";
    inline constexpr const char* scListen  = "scListen";
    inline constexpr const char* bypass    = "bypass";
}

/*  REAL-TIME SAFETY DESIGN
    -----------------------
    * processBlock() never allocates, frees, locks, logs or touches the message thread.
      This is enforced by tests/rt_safety_test.cpp, which traps malloc/new/free/mutex calls.
    * Parameters are plain JUCE parameter objects whose values are std::atomic<float>.
      The audio thread only does atomic loads.
    * No AudioProcessorValueTreeState and no Slider/ButtonAttachments: those register
      listeners that, when the host automates a parameter inside the audio callback,
      lock a CriticalSection and post (and allocate) messages to the message queue
      from the audio thread. The editor instead polls parameter values on a UI timer.
    * GUI <-> audio communication is via lock-free atomics only.
*/
class VBCAudioProcessor final : public juce::AudioProcessor
{
public:
    VBCAudioProcessor();
    ~VBCAudioProcessor() override = default;

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

    // Host bypass maps to this parameter, so bypass is crossfaded (click-free) by our DSP
    // instead of the wrapper hard-switching to processBlockBypassed().
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypass; }

    // GUI meters (written by audio thread, read by GUI timer)
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<bool>  sidechainConnected { false };
    static_assert (std::atomic<float>::is_always_lock_free, "meter atomics must be lock-free");
    static_assert (std::atomic<bool>::is_always_lock_free,  "meter atomics must be lock-free");

    static const juce::StringArray ratioChoices, attackChoices, releaseChoices;

private:
    vbc::BusCompressor compressor;

    juce::AudioParameterFloat*  threshold = nullptr;
    juce::AudioParameterChoice* ratio     = nullptr;
    juce::AudioParameterChoice* attack    = nullptr;
    juce::AudioParameterChoice* release   = nullptr;
    juce::AudioParameterFloat*  makeup    = nullptr;
    juce::AudioParameterBool*   hpfOn     = nullptr;
    juce::AudioParameterFloat*  hpfFreq   = nullptr;
    juce::AudioParameterFloat*  analog    = nullptr;
    juce::AudioParameterFloat*  mix       = nullptr;
    juce::AudioParameterBool*   extSc     = nullptr;
    juce::AudioParameterBool*   scListen  = nullptr;
    juce::AudioParameterBool*   bypass    = nullptr;

    vbc::Parameters readParameters() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VBCAudioProcessor)
};
