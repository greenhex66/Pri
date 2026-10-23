#pragma once
#include "PluginProcessor.h"
#include "HexQ/HexQLookAndFeel.h"
#include "HexQ/HexQHeader.h"
#include "HexQ/HexQFooter.h"
#include "HexQ/HexQModeButton.h"
#include <memory>
#include <vector>

class PrimesAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::AsyncUpdater,
                                   private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PrimesAudioProcessorEditor (PrimesAudioProcessor&);
    ~PrimesAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { applyMode(); }

    int currentMode() const;
    void setMode (int mode);
    void applyMode();
    void layoutPanel();

    PrimesAudioProcessor& proc;
    hexq::Palette palette;                      // accent follows the selected mode
    hexq::HexQLookAndFeel laf;

    hexq::HexQHeader header;
    hexq::HexQFooter footer;
    hexq::HexQModeButton modeOptimus, modeMegatron;

    juce::Slider optDrive, optTone, megDrive, megBits, megRate, mixS, outS;

    juce::Rectangle<int> panelArea, caption1Area, caption2Area, separator;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SA>> sliderAtt;
    std::vector<std::unique_ptr<BA>> buttonAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrimesAudioProcessorEditor)
};
