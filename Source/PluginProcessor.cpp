#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Diag.h"

PrimesAudioProcessor::PrimesAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", primesp::createLayout())
{
    primesdiag::trace ("processor ctor: parameters = " + juce::String (getParameters().size()));
    params = primesp::getParamPtrs (apvts);
}

void PrimesAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    primesdiag::trace ("prepareToPlay: sr=" + juce::String (sampleRate) + " block=" + juce::String (samplesPerBlock));
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());          // 0: no look-ahead, no buffering
}

// Mono -> mono or stereo -> stereo (the channel configuration is preserved).
bool PrimesAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void PrimesAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numCh < 1 || numSamples <= 0)
        return;

    hexq::primes::Settings s;
    primesp::fillSettings (params, s);

    engine.process (buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr, numSamples, s);

    for (int c = 2; c < numCh; ++c)
        buffer.clear (c, 0, numSamples);
}

juce::AudioProcessorEditor* PrimesAudioProcessor::createEditor()
{
    primesdiag::trace ("createEditor");
    return new PrimesAudioProcessorEditor (*this);
}

void PrimesAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
    primesdiag::trace ("getStateInformation: bytes=" + juce::String ((int) destData.getSize()));
}

void PrimesAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    primesdiag::trace ("setStateInformation: begin, size=" + juce::String (sizeInBytes));
    try
    {
        std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
        if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
    catch (...)
    {
        primesdiag::trace ("setStateInformation: exception");
    }
    primesdiag::trace ("setStateInformation: end");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrimesAudioProcessor();
}
