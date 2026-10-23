#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/PrimesEngine.h"
#include "Parameters/PrimesParams.h"

class PrimesAudioProcessor : public juce::AudioProcessor
{
public:
    PrimesAudioProcessor();
    ~PrimesAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HEX-Q PRIMES"; }
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

private:
    primesp::ParamPtrs params;
    hexq::primes::PrimesEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrimesAudioProcessor)
};
