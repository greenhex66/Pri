#pragma once
#include "HexQTheme.h"

namespace hexq
{
// Large mode selector (radio dot + title + one-line description). Each button carries its own accent colour,
// shown when it is the selected one. Reads the palette by reference, so a run-time palette change is picked up.
class HexQModeButton : public juce::Button
{
public:
    HexQModeButton (const Palette& p, juce::Colour accentWhenSelected, const juce::String& titleText,
                    const juce::String& subtitleText, int radioGroup)
        : juce::Button (titleText), palette (p), accent (accentWhenSelected), title (titleText), subtitle (subtitleText)
    {
        setClickingTogglesState (true);
        setRadioGroupId (radioGroup);
    }

    void paintButton (juce::Graphics& g, bool hover, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();

        g.setColour (on ? accent.withAlpha (0.12f) : palette.panel);
        g.fillRoundedRectangle (r, layout::corner);
        g.setColour (on ? accent : (hover ? palette.textDim : palette.border));
        g.drawRoundedRectangle (r, layout::corner, on ? 1.6f : 1.0f);

        const juce::Rectangle<float> dot (r.getX() + 20.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f);
        g.setColour (on ? accent : palette.border);
        g.drawEllipse (dot, 1.5f);
        if (on)
            g.fillEllipse (dot.reduced (3.5f));

        auto text = r.withTrimmedLeft (50.0f).reduced (6.0f, 0.0f);
        g.setFont (makeFont (16.0f, true));
        g.setColour (on ? accent : palette.text);
        g.drawText (title, text.removeFromTop (r.getHeight() * 0.5f + 4.0f), juce::Justification::bottomLeft, false);

        g.setFont (makeFont (11.0f));
        g.setColour (palette.textDim);
        g.drawText (subtitle, text, juce::Justification::topLeft, false);
    }

private:
    const Palette& palette;
    juce::Colour accent;
    juce::String title, subtitle;
};
} // namespace hexq
