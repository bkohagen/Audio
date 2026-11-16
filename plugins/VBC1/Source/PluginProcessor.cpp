#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray VBCAudioProcessor::ratioChoices   { "2", "4", "10" };
const juce::StringArray VBCAudioProcessor::attackChoices  { "0.1", "0.3", "1", "3", "10", "30" };
const juce::StringArray VBCAudioProcessor::releaseChoices { "0.1", "0.3", "0.6", "1.2", "Auto" };

static constexpr float kRatioValues[]   { 2.0f, 4.0f, 10.0f };
static constexpr float kAttackValues[]  { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };      // ms
static constexpr float kReleaseValues[] { 100.0f, 300.0f, 600.0f, 1200.0f, 0.0f };    // ms, 0 = auto

static const juce::Identifier kStateTag { "VBC1" };

VBCAudioProcessor::VBCAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true))
{
    using namespace juce;

    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%")
                     .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });

    NormalisableRange<float> hpfRange (20.0f, 500.0f, 1.0f);
    hpfRange.setSkewForCentre (100.0f);

    auto add = [this] (auto* p) { addParameter (p); return p; };

    threshold = add (new AudioParameterFloat (ParameterID { ParamIDs::threshold, 1 }, "Threshold",
                                              NormalisableRange<float> (-30.0f, 6.0f, 0.1f), -10.0f, dbAttr));
    ratio     = add (new AudioParameterChoice (ParameterID { ParamIDs::ratio, 1 }, "Ratio", ratioChoices, 1));
    attack    = add (new AudioParameterChoice (ParameterID { ParamIDs::attack, 1 }, "Attack (ms)", attackChoices, 4));
    release   = add (new AudioParameterChoice (ParameterID { ParamIDs::release, 1 }, "Release (s)", releaseChoices, 4));
    makeup    = add (new AudioParameterFloat (ParameterID { ParamIDs::makeup, 1 }, "Makeup",
                                              NormalisableRange<float> (-10.0f, 20.0f, 0.1f), 0.0f, dbAttr));
    hpfOn     = add (new AudioParameterBool (ParameterID { ParamIDs::hpfOn, 1 }, "SC HPF On", true));
    hpfFreq   = add (new AudioParameterFloat (ParameterID { ParamIDs::hpfFreq, 1 }, "SC HPF Freq", hpfRange, 90.0f,
                        AudioParameterFloatAttributes().withLabel ("Hz")
                            .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })));
    analog    = add (new AudioParameterFloat (ParameterID { ParamIDs::analog, 1 }, "Analog",
                                              NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f, pctAttr));
    mix       = add (new AudioParameterFloat (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                              NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));
    extSc     = add (new AudioParameterBool (ParameterID { ParamIDs::extSc, 1 }, "External SC", false));
    scListen  = add (new AudioParameterBool (ParameterID { ParamIDs::scListen, 1 }, "SC Listen", false));
    bypass    = add (new AudioParameterBool (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
}

bool VBCAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

// Audio thread: atomic loads only.
vbc::Parameters VBCAudioProcessor::readParameters() const noexcept
{
    vbc::Parameters p;
    p.thresholdDb = threshold->get();
    p.ratio       = kRatioValues  [juce::jlimit (0, 2, ratio->getIndex())];
    p.attackMs    = kAttackValues [juce::jlimit (0, 5, attack->getIndex())];
    p.releaseMs   = kReleaseValues[juce::jlimit (0, 4, release->getIndex())];
    p.makeupDb    = makeup->get();
    p.hpfOn       = hpfOn->get();
    p.hpfHz       = hpfFreq->get();
    p.analog      = analog->get() * 0.01f;
    p.mix         = mix->get() * 0.01f;
    p.externalSc  = extSc->get();
    p.scListen    = scListen->get();
    p.bypass      = bypass->get();
    return p;
}

void VBCAudioProcessor::prepareToPlay (double sampleRate, int)
{
    setLatencySamples (vbc::BusCompressor::getLatencySamples()); // always 0
    compressor.prepare (sampleRate);
    compressor.setParameters (readParameters());
    compressor.reset();
}

// Real-time safe: no allocation, no locks, no I/O. See tests/rt_safety_test.cpp.
void VBCAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIns  = getTotalNumInputChannels();
    const int numOuts = getTotalNumOutputChannels();

    for (int ch = numIns; ch < numOuts; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Main bus is always channels [0, mainCh); side-chain bus follows it.
    // (Channel counts are cached by JUCE on layout change, so these calls don't allocate.)
    const int mainCh = juce::jmin (getMainBusNumInputChannels(), 2);
    const int scCh   = juce::jmin (numIns - getMainBusNumInputChannels(), 2);

    float* io[2] = { nullptr, nullptr };
    for (int ch = 0; ch < mainCh; ++ch)
        io[ch] = buffer.getWritePointer (ch);

    const float* scPtrs[2] = { nullptr, nullptr };
    for (int ch = 0; ch < scCh; ++ch)
        scPtrs[ch] = buffer.getReadPointer (getMainBusNumInputChannels() + ch);

    sidechainConnected.store (scCh > 0, std::memory_order_relaxed);

    compressor.setParameters (readParameters());
    compressor.process (io, mainCh, scCh > 0 ? scPtrs : nullptr, scCh, numSamples);

    gainReductionDb.store (compressor.getGainReductionDb(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* VBCAudioProcessor::createEditor()
{
    return new VBCAudioProcessorEditor (*this);
}

// State: <VBC1><PARAM id="..." value="..."/>...</VBC1> (plain values), via the shared kit helpers.
void VBCAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    kit::saveParameterState (*this, kStateTag, destData);
}

void VBCAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    kit::loadParameterState (*this, kStateTag, data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VBCAudioProcessor();
}
