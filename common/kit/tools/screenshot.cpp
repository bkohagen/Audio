// Renders a plugin's editor to a PNG, headless, so UI changes can be reviewed before pushing.
// Built for every plugin by kit_add_plugin() as <TARGET>_screenshot (Linux).
//
//   usage: <TARGET>_screenshot out.png [scale=2]
//
// It feeds ~2 s of a -3 dBFS stereo test tone through the processor first, then lets the
// editor's timers run for ~1.5 s so meters show realistic values.
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: %s out.png [scale]\n", argv[0]); return 2; }
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 2.0f;

    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());

    constexpr double fs = 48000.0;
    constexpr int block = 512;
    p->setRateAndBufferSizeDetails (fs, block);
    p->prepareToPlay (fs, block);

    const int ch = juce::jmax (p->getTotalNumInputChannels(), p->getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (ch, block);
    juce::MidiBuffer midi;
    double phase = 0.0;
    auto feed = [&] (int numBlocks)
    {
        for (int b = 0; b < numBlocks; ++b)
        {
            buffer.clear();
            for (int c = 0; c < juce::jmin (2, p->getMainBusNumInputChannels()); ++c)
                for (int n = 0; n < block; ++n)
                    buffer.setSample (c, n, 0.7f * (float) std::sin (phase + 2.0 * juce::MathConstants<double>::pi * 220.0 * n / fs));
            phase += 2.0 * juce::MathConstants<double>::pi * 220.0 * block / fs;
            p->processBlock (buffer, midi);
        }
    };
    feed (200);

    std::unique_ptr<juce::AudioProcessorEditor> e (p->createEditor());
    if (e == nullptr) { std::printf ("plugin has no editor\n"); return 1; }

    // let timers/animations run while audio keeps flowing
    for (int i = 0; i < 50; ++i)
    {
        feed (3);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
    }

    const auto img = e->createComponentSnapshot (e->getLocalBounds(), true, scale);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    if (! os.openedOk() || ! juce::PNGImageFormat().writeImageToStream (img, os)) { std::printf ("could not write %s\n", argv[1]); return 1; }
    std::printf ("wrote %s (%d x %d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());

    e.reset();
    p->releaseResources();
    return 0;
}
