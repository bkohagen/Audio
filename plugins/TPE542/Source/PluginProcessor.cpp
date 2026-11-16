#include "PluginProcessor.h"
#include "PluginEditor.h"

static const juce::Identifier kStateTag { "TPE542" };

TPE542AudioProcessor::TPE542AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    using namespace juce;
    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
                    .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%")
                     .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });

    auto tenthAttr = AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return String (v, 1); });

    // Parameter IDs + version hints are part of saved sessions: never rename or reuse them.
    addParameter (trim       = new AudioParameterFloat (ParameterID { ParamIDs::trim, 1 }, "Trim",
                                                        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    addParameter (saturation = new AudioParameterFloat (ParameterID { ParamIDs::saturation, 1 }, "Saturation",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f, tenthAttr));
    addParameter (speed      = new AudioParameterChoice (ParameterID { ParamIDs::speed, 1 }, "Speed",
                                                         StringArray { "15 IPS", "30 IPS" }, 0));
    addParameter (silk       = new AudioParameterChoice (ParameterID { ParamIDs::silk, 1 }, "Silk",
                                                         StringArray { "Off", "Red", "Blue" }, 0));
    addParameter (texture    = new AudioParameterFloat (ParameterID { ParamIDs::texture, 1 }, "Texture",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f, tenthAttr));
    addParameter (blend      = new AudioParameterFloat (ParameterID { ParamIDs::blend, 1 }, "Blend",
                                                        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));
    addParameter (output     = new AudioParameterFloat (ParameterID { ParamIDs::output, 1 }, "Output",
                                                        NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    addParameter (bypass     = new AudioParameterBool  (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
}

bool TPE542AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

// Audio thread: atomic loads only.
tpe542::Parameters TPE542AudioProcessor::readParameters() const noexcept
{
    tpe542::Parameters p;
    p.trimDb     = trim->get();
    p.saturation = saturation->get();
    p.speed      = speed->getIndex();
    p.silk       = silk->getIndex();
    p.texture    = texture->get();
    p.blend      = blend->get() * 0.01f;
    p.outputDb   = output->get();
    p.bypass     = bypass->get();
    return p;
}

void TPE542AudioProcessor::prepareToPlay (double sampleRate, int)
{
    setLatencySamples (tpe542::Processor::getLatencySamples()); // the host compensates for this
    dsp.prepare (sampleRate);
    dsp.setParameters (readParameters());
    dsp.reset();
}

// Real-time safe: no allocation, no locks, no I/O. See TPE542_rt_safety_test.
void TPE542AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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
    saturationDb.store (dsp.getSaturationDb(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* TPE542AudioProcessor::createEditor() { return new TPE542AudioProcessorEditor (*this); }

void TPE542AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    kit::saveParameterState (*this, kStateTag, destData);
}

void TPE542AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    kit::loadParameterState (*this, kStateTag, data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TPE542AudioProcessor(); }
