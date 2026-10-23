#include "PluginEditor.h"
#include "Diag.h"

namespace
{
    const juce::Colour kOptimusAccent (0xffff4d5a);     // OPTIMUS: red
    const juce::Colour kMegatronAccent (0xffa78bfa);    // MEGATRON: purple

    hexq::Palette paletteFor (int mode)
    {
        hexq::Palette p;                                 // shared HEX-Q base colours
        p.accent = (mode == 0) ? kOptimusAccent : kMegatronAccent;
        return p;
    }
}

PrimesAudioProcessorEditor::PrimesAudioProcessorEditor (PrimesAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      palette (paletteFor ((int) p.apvts.getRawParameterValue ("mode")->load())),
      laf (palette),
      header (palette, "HEX-Q", "PRIMES", "v1.0.0"),
      footer (palette, "OPTIMUS: SMOOTH SATURATION    MEGATRON: DISTORTION + BIT CRUSH + SAMPLE-RATE REDUCTION    DRAG / WHEEL / CLICK THE NUMBER TO TYPE    DOUBLE-CLICK = RESET"),
      modeOptimus (palette, kOptimusAccent, "OPTIMUS", "Smooth, musical saturation", 3001),
      modeMegatron (palette, kMegatronAccent, "MEGATRON", "Aggressive distortion, bit crush, sample-rate reduction", 3001)
{
    primesdiag::trace ("editor ctor: members built");
    setLookAndFeel (&laf);

    addAndMakeVisible (header);
    addAndMakeVisible (footer);
    addAndMakeVisible (modeOptimus);
    addAndMakeVisible (modeMegatron);
    modeOptimus.onClick  = [this] { setMode (0); };
    modeMegatron.onClick = [this] { setMode (1); };

    auto setupField = [this] (juce::Slider& s, const juce::String& name, const char* paramId)
    {
        s.setName (name);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 150, 26);
        s.setMouseDragSensitivity (220);
        addChildComponent (s);
        sliderAtt.push_back (std::make_unique<SA> (proc.apvts, paramId, s));
    };
    setupField (optDrive, "Drive",       "optDrive");
    setupField (optTone,  "Tone",        "optTone");
    setupField (megDrive, "Drive",       "megDrive");
    setupField (megBits,  "Bit Depth",   "megBits");
    setupField (megRate,  "Sample Rate", "megRate");
    setupField (mixS,     "Mix",         "mix");
    setupField (outS,     "Output",      "output");
    buttonAtt.push_back (std::make_unique<BA> (proc.apvts, "bypass", header.getBypassButton()));
    primesdiag::trace ("editor ctor: attachments ready");

    proc.apvts.addParameterListener ("mode", this);
    applyMode();

    setSize (860, 424);          // must stay LAST: it calls resized()
    primesdiag::trace ("editor ctor: done");
}

PrimesAudioProcessorEditor::~PrimesAudioProcessorEditor()
{
    primesdiag::trace ("editor dtor");
    cancelPendingUpdate();
    proc.apvts.removeParameterListener ("mode", this);
    setLookAndFeel (nullptr);
}

int PrimesAudioProcessorEditor::currentMode() const
{
    return juce::jlimit (0, 1, (int) proc.apvts.getRawParameterValue ("mode")->load());
}

void PrimesAudioProcessorEditor::setMode (int mode)
{
    if (auto* p = proc.apvts.getParameter ("mode"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) mode));
        p->endChangeGesture();
    }
}

void PrimesAudioProcessorEditor::applyMode()
{
    const int m = currentMode();
    const bool opt = (m == 0);

    // the selected mode decides the accent of every active control
    palette = paletteFor (m);
    laf.setPalette (palette);
    header.setPalette (palette);
    footer.setPalette (palette);

    modeOptimus.setToggleState (opt, juce::dontSendNotification);
    modeMegatron.setToggleState (! opt, juce::dontSendNotification);

    optDrive.setVisible (opt);   optTone.setVisible (opt);
    megDrive.setVisible (! opt); megBits.setVisible (! opt); megRate.setVisible (! opt);
    mixS.setVisible (true);      outS.setVisible (true);

    layoutPanel();
    sendLookAndFeelChange();      // children re-read the palette
    repaint();
}

void PrimesAudioProcessorEditor::layoutPanel()
{
    const bool opt = (currentMode() == 0);
    auto inner = panelArea.reduced (18, 14);

    caption1Area = inner.removeFromTop (16);
    inner.removeFromTop (4);

    const int fw = 150, gap = 24, fh = 66;
    auto cell = [&] (int col, int y) { return juce::Rectangle<int> (inner.getX() + col * (fw + gap), y, fw, fh); };

    const int y1 = inner.getY();
    if (opt)
    {
        optDrive.setBounds (cell (0, y1));
        optTone.setBounds (cell (1, y1));
    }
    else
    {
        megDrive.setBounds (cell (0, y1));
        megBits.setBounds (cell (1, y1));
        megRate.setBounds (cell (2, y1));
    }

    separator = juce::Rectangle<int> (inner.getX(), y1 + fh + 14, inner.getWidth(), 1);
    caption2Area = juce::Rectangle<int> (inner.getX(), separator.getBottom() + 12, inner.getWidth(), 16);
    const int y2 = caption2Area.getBottom() + 4;
    mixS.setBounds (cell (0, y2));
    outS.setBounds (cell (1, y2));
}

void PrimesAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette.background);

    g.setColour (palette.panel);
    g.fillRoundedRectangle (panelArea.toFloat(), hexq::layout::corner);
    g.setColour (palette.border);
    g.drawRoundedRectangle (panelArea.toFloat().reduced (0.5f), hexq::layout::corner, 1.0f);
    g.fillRect (separator);

    g.setFont (hexq::makeFont (11.0f, true));
    g.setColour (palette.accent);
    g.drawText (currentMode() == 0 ? "OPTIMUS  -  SMOOTH SATURATION" : "MEGATRON  -  DISTORTION + BIT CRUSH + SAMPLE-RATE REDUCTION",
                caption1Area, juce::Justification::centredLeft, false);

    g.setColour (palette.textDim);
    g.drawText ("OUTPUT SECTION (BOTH MODES)", caption2Area, juce::Justification::centredLeft, false);
}

void PrimesAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (hexq::layout::headerHeight));
    footer.setBounds (r.removeFromBottom (hexq::layout::footerHeight));

    r = r.reduced (hexq::layout::margin, 0);
    r.removeFromTop (8);

    auto row = r.removeFromTop (84);
    const int gap = hexq::layout::gap;
    const int bw = (row.getWidth() - gap) / 2;
    modeOptimus.setBounds (row.removeFromLeft (bw));
    row.removeFromLeft (gap);
    modeMegatron.setBounds (row);

    r.removeFromTop (gap);
    r.removeFromBottom (6);
    panelArea = r;

    layoutPanel();
}
