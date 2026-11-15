#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
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
}

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

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // For the GUI meter
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<bool>  sidechainConnected { false };

    static const juce::StringArray ratioChoices, attackChoices, releaseChoices;

private:
    vbc::BusCompressor compressor;

    std::atomic<float>* pThreshold = nullptr; std::atomic<float>* pRatio = nullptr;
    std::atomic<float>* pAttack = nullptr;    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pMakeup = nullptr;    std::atomic<float>* pHpfOn = nullptr;
    std::atomic<float>* pHpfFreq = nullptr;   std::atomic<float>* pAnalog = nullptr;
    std::atomic<float>* pMix = nullptr;       std::atomic<float>* pExtSc = nullptr;
    std::atomic<float>* pScListen = nullptr;

    vbc::Parameters readParameters() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VBCAudioProcessor)
};
