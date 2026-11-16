#include "PluginProcessor.h"
#include "PluginEditor.h"

static const juce::Identifier kStateTag { "__TARGET__" };

__CLASS__AudioProcessor::__CLASS__AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    using namespace juce;
    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%")
                     .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });

    // Parameter IDs + version hints are part of saved sessions: never rename or reuse them.
    addParameter (input  = new AudioParameterFloat (ParameterID { ParamIDs::input, 1 },  "Input",
                                                    NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dbAttr));
    addParameter (output = new AudioParameterFloat (ParameterID { ParamIDs::output, 1 }, "Output",
                                                    NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dbAttr));
    addParameter (mix    = new AudioParameterFloat (ParameterID { ParamIDs::mix, 1 },    "Mix",
                                                    NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));
    addParameter (bypass = new AudioParameterBool  (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
}

bool __CLASS__AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

// Audio thread: atomic loads only.
__NS__::Parameters __CLASS__AudioProcessor::readParameters() const noexcept
{
    __NS__::Parameters p;
    p.inputDb  = input->get();
    p.outputDb = output->get();
    p.mix      = mix->get() * 0.01f;
    p.bypass   = bypass->get();
    return p;
}

void __CLASS__AudioProcessor::prepareToPlay (double sampleRate, int)
{
    setLatencySamples (__NS__::Processor::getLatencySamples()); // the host compensates for this
    dsp.prepare (sampleRate);
    dsp.setParameters (readParameters());
    dsp.reset();
}

// Real-time safe: no allocation, no locks, no I/O. See __TARGET___rt_safety_test.
void __CLASS__AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const int mainCh = juce::jmin (getMainBusNumInputChannels(), 2);
    float* io[2] = { nullptr, nullptr };
    for (int ch = 0; ch < mainCh; ++ch)
        io[ch] = buffer.getWritePointer (ch);

    dsp.setParameters (readParameters());
    dsp.process (io, mainCh, numSamples);

    float peak = 0.0f;
    for (int ch = 0; ch < mainCh; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));
    outputPeak.store (peak, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* __CLASS__AudioProcessor::createEditor() { return new __CLASS__AudioProcessorEditor (*this); }

void __CLASS__AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    kit::saveParameterState (*this, kStateTag, destData);
}

void __CLASS__AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    kit::loadParameterState (*this, kStateTag, data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new __CLASS__AudioProcessor(); }
