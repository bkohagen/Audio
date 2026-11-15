#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray VBCAudioProcessor::ratioChoices   { "2", "4", "10" };
const juce::StringArray VBCAudioProcessor::attackChoices  { "0.1", "0.3", "1", "3", "10", "30" };
const juce::StringArray VBCAudioProcessor::releaseChoices { "0.1", "0.3", "0.6", "1.2", "Auto" };

static constexpr float kRatioValues[]   { 2.0f, 4.0f, 10.0f };
static constexpr float kAttackValues[]  { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };      // ms
static constexpr float kReleaseValues[] { 100.0f, 300.0f, 600.0f, 1200.0f, 0.0f };    // ms, 0 = auto

VBCAudioProcessor::VBCAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VBC1", createLayout())
{
    pThreshold = apvts.getRawParameterValue (ParamIDs::threshold);
    pRatio     = apvts.getRawParameterValue (ParamIDs::ratio);
    pAttack    = apvts.getRawParameterValue (ParamIDs::attack);
    pRelease   = apvts.getRawParameterValue (ParamIDs::release);
    pMakeup    = apvts.getRawParameterValue (ParamIDs::makeup);
    pHpfOn     = apvts.getRawParameterValue (ParamIDs::hpfOn);
    pHpfFreq   = apvts.getRawParameterValue (ParamIDs::hpfFreq);
    pAnalog    = apvts.getRawParameterValue (ParamIDs::analog);
    pMix       = apvts.getRawParameterValue (ParamIDs::mix);
    pExtSc     = apvts.getRawParameterValue (ParamIDs::extSc);
    pScListen  = apvts.getRawParameterValue (ParamIDs::scListen);
}

juce::AudioProcessorValueTreeState::ParameterLayout VBCAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::threshold, 1 }, "Threshold",
                                                       NormalisableRange<float> (-30.0f, 6.0f, 0.1f), -10.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::ratio, 1 }, "Ratio", ratioChoices, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::attack, 1 }, "Attack (ms)", attackChoices, 4));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::release, 1 }, "Release (s)", releaseChoices, 4));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::makeup, 1 }, "Makeup",
                                                       NormalisableRange<float> (-10.0f, 20.0f, 0.1f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::hpfOn, 1 }, "SC HPF On", true));

    NormalisableRange<float> hpfRange (20.0f, 500.0f, 1.0f);
    hpfRange.setSkewForCentre (100.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::hpfFreq, 1 }, "SC HPF Freq", hpfRange, 90.0f,
                    AudioParameterFloatAttributes().withLabel ("Hz")
                        .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })));

    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%")
                     .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::analog, 1 }, "Analog",
                                                       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f, pctAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::extSc, 1 }, "External SC", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::scListen, 1 }, "SC Listen", false));
    return layout;
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

vbc::Parameters VBCAudioProcessor::readParameters() const
{
    vbc::Parameters p;
    p.thresholdDb = pThreshold->load();
    p.ratio       = kRatioValues  [juce::jlimit (0, 2, (int) pRatio->load())];
    p.attackMs    = kAttackValues [juce::jlimit (0, 5, (int) pAttack->load())];
    p.releaseMs   = kReleaseValues[juce::jlimit (0, 4, (int) pRelease->load())];
    p.makeupDb    = pMakeup->load();
    p.hpfOn       = pHpfOn->load() > 0.5f;
    p.hpfHz       = pHpfFreq->load();
    p.analog      = pAnalog->load() * 0.01f;
    p.mix         = pMix->load() * 0.01f;
    p.externalSc  = pExtSc->load() > 0.5f;
    p.scListen    = pScListen->load() > 0.5f;
    return p;
}

void VBCAudioProcessor::prepareToPlay (double sampleRate, int)
{
    setLatencySamples (vbc::BusCompressor::getLatencySamples()); // always 0
    compressor.prepare (sampleRate);
    compressor.setParameters (readParameters());
    compressor.reset();
}

void VBCAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer (buffer, true, 0);
    const int numCh = juce::jmin (mainBus.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    // Clear any output channels beyond the main input (safety)
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const float* scPtrs[2] = { nullptr, nullptr };
    int scCh = 0;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto scBus = getBusBuffer (buffer, true, 1);
        scCh = juce::jmin (scBus.getNumChannels(), 2);
        for (int ch = 0; ch < scCh; ++ch)
            scPtrs[ch] = scBus.getReadPointer (ch);
    }
    sidechainConnected.store (scCh > 0);

    compressor.setParameters (readParameters());

    float* io[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numCh; ++ch)
        io[ch] = mainBus.getWritePointer (ch);

    compressor.process (io, numCh, scCh > 0 ? scPtrs : nullptr, scCh, numSamples);

    gainReductionDb.store (compressor.getGainReductionDb());
}

juce::AudioProcessorEditor* VBCAudioProcessor::createEditor()
{
    return new VBCAudioProcessorEditor (*this);
}

void VBCAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VBCAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VBCAudioProcessor();
}
