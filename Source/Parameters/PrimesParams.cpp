#include "PrimesParams.h"
#include <cmath>

namespace primesp
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const StringArray modeNames { "Optimus", "Megatron" };

    auto percent = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String ((int) std::round (v)) + "%"; })
        .withValueFromStringFunction ([] (const String& t) { return jlimit (0.0f, 100.0f, t.getFloatValue()); });
    auto bitsAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " bit"; })
        .withValueFromStringFunction ([] (const String& t) { return jlimit (4.0f, 16.0f, t.getFloatValue()); });
    auto dbAttr = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v > 0.049f ? "+" : "") + String (v, 1) + " dB"; })
        .withValueFromStringFunction ([] (const String& t) { return jlimit (-18.0f, 6.0f, t.getFloatValue()); });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modeNames, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "bypass", 1 }, "Bypass", false));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-18.0f, 6.0f, 0.1f), 0.0f, dbAttr));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "optDrive", 1 }, "Optimus Drive", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "optTone", 1 }, "Optimus Tone", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f, percent));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "megDrive", 1 }, "Megatron Drive", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 25.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "megBits", 1 }, "Megatron Bit Depth", NormalisableRange<float> (4.0f, 16.0f, 0.1f), 8.0f, bitsAttr));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "megRate", 1 }, "Megatron Sample Rate", NormalisableRange<float> (1.0f, 100.0f, 1.0f), 50.0f, percent));

    return layout;
}

ParamPtrs getParamPtrs (juce::AudioProcessorValueTreeState& apvts)
{
    ParamPtrs p;
    p.mode     = apvts.getRawParameterValue ("mode");
    p.bypass   = apvts.getRawParameterValue ("bypass");
    p.mix      = apvts.getRawParameterValue ("mix");
    p.output   = apvts.getRawParameterValue ("output");
    p.optDrive = apvts.getRawParameterValue ("optDrive");
    p.optTone  = apvts.getRawParameterValue ("optTone");
    p.megDrive = apvts.getRawParameterValue ("megDrive");
    p.megBits  = apvts.getRawParameterValue ("megBits");
    p.megRate  = apvts.getRawParameterValue ("megRate");
    return p;
}

void fillSettings (const ParamPtrs& p, hexq::primes::Settings& s) noexcept
{
    s.mode     = (hexq::primes::Mode) juce::jlimit (0, 1, (int) p.mode->load());
    s.bypass   = p.bypass->load() > 0.5f;
    s.mix      = p.mix->load() * 0.01f;
    s.outputDb = p.output->load();
    s.optDrive = p.optDrive->load() * 0.01f;
    s.optTone  = p.optTone->load() * 0.01f;
    s.megDrive = p.megDrive->load() * 0.01f;
    s.megBits  = p.megBits->load();
    s.megRate  = p.megRate->load() * 0.01f;
}
} // namespace primesp
