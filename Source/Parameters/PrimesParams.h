#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "../DSP/PrimesEngine.h"

// HEX-Q PRIMES - parameters. IDs are stable (never rename them); display names may change freely.
//   mode        OPTIMUS / MEGATRON                  bypass     global bypass
//   mix         0..100 %   (global, both modes)     output     -18..+6 dB (global, both modes)
//   optDrive    0..100 %                            optTone    0..100 % (50 = flat)
//   megDrive    0..100 %                            megBits    4..16 bit
//   megRate     1..100 % (100 = no reduction)
// Every parameter is saved always, so switching mode never erases the settings of the other mode.
namespace primesp
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

struct ParamPtrs
{
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* output = nullptr;
    std::atomic<float>* optDrive = nullptr;
    std::atomic<float>* optTone = nullptr;
    std::atomic<float>* megDrive = nullptr;
    std::atomic<float>* megBits = nullptr;
    std::atomic<float>* megRate = nullptr;
};

ParamPtrs getParamPtrs (juce::AudioProcessorValueTreeState&);
void fillSettings (const ParamPtrs&, hexq::primes::Settings&) noexcept;   // audio-thread safe
} // namespace primesp
