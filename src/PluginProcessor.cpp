#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

AVProcessor::AVProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Main", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain 1", juce::AudioChannelSet::stereo(), false)
                          .withInput ("Sidechain 2", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Main", juce::AudioChannelSet::stereo(), true))
{
}

void AVProcessor::prepareToPlay (double sampleRate, int)
{
    engine.analyzer.prepare (sampleRate);
    engine.midi.prepare (sampleRate);
}

bool AVProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (auto& in : l.inputBuses)
        if (! in.isDisabled() && in != juce::AudioChannelSet::stereo())
            return false;
    return true;
}

void AVProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    auto channels = [&] (int bus, const float*& l, const float*& r)
    {
        l = r = nullptr;
        auto* b = getBus (true, bus);
        if (b == nullptr || ! b->isEnabled() || b->getNumberOfChannels() < 1)
            return;
        l = buffer.getReadPointer (getChannelIndexInProcessBlockBuffer (true, bus, 0));
        r = b->getNumberOfChannels() > 1 ? buffer.getReadPointer (getChannelIndexInProcessBlockBuffer (true, bus, 1)) : l;
    };

    const float *mL, *mR, *aL, *aR, *bL, *bR;
    channels (0, mL, mR);
    channels (1, aL, aR);
    channels (2, bL, bR);
    engine.analyzer.process (mL, mR, aL, aR, bL, bR, n);
    engine.midi.process (midi, n);

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            engine.bus.set (engine.sBpm, (float) pos->getBpm().orFallback (120.0));
            const double ppq = pos->getPpqPosition().orFallback (0.0);
            engine.bus.set (engine.sBeat, (float) (ppq - std::floor (ppq)));
            engine.bus.set (engine.sPlaying, pos->getIsPlaying() ? 1.0f : 0.0f);
            engine.modulation.hostPpq = pos->getIsPlaying() ? ppq : -1.0;
        }
    // Audio passes through untouched.
}

juce::AudioProcessorEditor* AVProcessor::createEditor() { return new AVEditor (*this); }

void AVProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    const auto s = juce::JSON::toString (engine.saveFullState(), true);
    dest.append (s.toRawUTF8(), s.getNumBytesAsUTF8());
}

void AVProcessor::setStateInformation (const void* data, int size)
{
    const auto v = juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), size));
    engine.loadFullState (v);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AVProcessor(); }
