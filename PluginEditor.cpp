#include "PluginEditor.h"

static const juce::Colour kPanelBody   { 0xff1b222b };
static const juce::Colour kPanelHeader { 0xff7f93a8 };
static const juce::Colour kWindowBg    { 0xffb8c2cc };
static const juce::Colour kAccent      { 0xffffb347 };

void VowelSynthEditor::styleSlider (juce::Slider& s)
{
    s.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    s.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    s.setColour (juce::Slider::trackColourId, kAccent);
    s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.85f));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

VowelSynthEditor::Knob* VowelSynthEditor::addKnob (const juce::String& id, const juce::String& name)
{
    auto k = std::make_unique<Knob>();
    styleSlider (k->slider);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    k->label.setText (name, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    k->label.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);

    auto* raw = k.get();
    ownedKnobs.push_back (std::move (k));
    return raw;
}

VowelSynthEditor::Chooser* VowelSynthEditor::addChooser (const juce::String& id, const juce::String& name,
                                                         const juce::StringArray& items)
{
    auto c = std::make_unique<Chooser>();
    c->box.addItemList (items, 1);
    c->label.setText (name, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centredLeft);
    c->label.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    c->label.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (c->box);
    addAndMakeVisible (c->label);
    c->att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c->box);

    auto* raw = c.get();
    ownedChoosers.push_back (std::move (c));
    return raw;
}

void VowelSynthEditor::setChoiceParam (const juce::String& id, int index)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
}

VowelSynthEditor::VowelSynthEditor (VowelSynthProcessor& p)
    : AudioProcessorEditor (&p), proc (p), pad (p.apvts), waveA (p, 0), waveB (p, 1),
      lfoDisplay (p, [this] { return currentLfo; }), envDisplay (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::verticalKeyboardFacingLeft)
{
    addAndMakeVisible (pad);
    addAndMakeVisible (waveA);
    addAndMakeVisible (waveB);
    addAndMakeVisible (lfoDisplay);
    addAndMakeVisible (envDisplay);
    addAndMakeVisible (keyboard);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setAvailableRange (48, 83);

    // ---- X / Y sliders for the dot ----
    for (auto* s : { &xSlider, &ySlider })
    {
        styleSlider (*s);
        s->setNumDecimalPlacesToDisplay (3);
        addAndMakeVisible (*s);
    }
    xSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    ySlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 20);
    xAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "x", xSlider);
    yAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "y", ySlider);
    for (auto* l : { &xLabel, &yLabel })
    {
        l->setColour (juce::Label::textColourId, juce::Colour (0xff1b222b));
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        addAndMakeVisible (*l);
    }
    xLabel.setText ("X", juce::dontSendNotification);
    yLabel.setText ("Y", juce::dontSendNotification);

    // ---- oscillators ----
    for (int o = 0; o < 2; ++o)
    {
        const juce::String s = "o" + juce::String (o + 1);
        oscKnobs[o] = { addKnob (s + "oct", "Octave"), addKnob (s + "tune", "Fine"), addKnob (s + "amt", "Pos / Amt") };

        auto& ui = oscUi[o];
        addAndMakeVisible (ui.sel);
        addAndMakeVisible (ui.prev);
        addAndMakeVisible (ui.next);

        ui.sel.onChange = [this, o]
        {
            const int id = oscUi[o].sel.getSelectedId();
            if (id <= 0) return;
            const juce::String sid = "o" + juce::String (o + 1);
            if (id < 100)
            {
                setChoiceParam (sid + "shape", id - 1);
                setChoiceParam (sid + "src", 0);
            }
            else if (proc.setWavetable (o, oscUi[o].sel.getText()))
            {
                setChoiceParam (sid + "src", 1);
            }
        };
        ui.prev.onClick = [this, o] { stepOsc (o, -1); };
        ui.next.onClick = [this, o] { stepOsc (o, 1); };
    }

    // ---- vowel / mix / envelope ----
    vowelKnobs = { addKnob ("res", "Resonance"), addKnob ("shift", "Shift"), addKnob ("glide", "Glide"),
                   addKnob ("vmix", "Vowel Mix"), addKnob ("vgain", "Vowel Gain") };
    mainKnobs  = { addKnob ("omix", "Osc Mix"), addKnob ("master", "Master") };
    envKnobs   = { addKnob ("atk", "Attack"), addKnob ("dec", "Decay"),
                   addKnob ("sus", "Sustain"), addKnob ("rel", "Release") };

    // ---- LFOs ----
    for (int l = 0; l < kNumLfos; ++l)
    {
        const juce::String s = "l" + juce::String (l + 1);
        lfoShapeCh[l] = addChooser (s + "shape", "Shape", kLfoShapes);
        lfoDestCh[l]  = addChooser (s + "dest",  "Destination", kLfoDests);
        lfoKnobs[l]   = { addKnob (s + "rate", "Rate"), addKnob (s + "depth", "Depth") };

        auto& b = lfoTabs[l];
        b.setButtonText ("LFO " + juce::String (l + 1));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1001);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff232c37));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff29a8e0));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.7f));
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b.onClick = [this, l] { showLfo (l); };
        addAndMakeVisible (b);
    }
    lfoTabs[0].setToggleState (true, juce::dontSendNotification);
    showLfo (0);

    // ---- wavetable folder buttons ----
    addAndMakeVisible (openFolderBtn);
    addAndMakeVisible (rescanBtn);
    openFolderBtn.onClick = [] { VowelSynthProcessor::getWavetableFolder().startAsProcess(); };
    rescanBtn.onClick = [this] { refreshOscSelectors(); };

    refreshOscSelectors();
    startTimerHz (5);

    setSize (1000, 700);
}

void VowelSynthEditor::refreshOscSelectors()
{
    const auto names = VowelSynthProcessor::listWavetables();
    for (int o = 0; o < 2; ++o)
    {
        auto& cb = oscUi[o].sel;
        cb.clear (juce::dontSendNotification);
        for (int i = 0; i < kOscShapes.size(); ++i)
            cb.addItem (kOscShapes[i], i + 1);

        if (names.size() > 0)
        {
            cb.addSeparator();
            for (int i = 0; i < names.size(); ++i)
                cb.addItem (names[i], 100 + i);
        }
        syncOscSelection (o);
    }
}

void VowelSynthEditor::syncOscSelection (int o)
{
    const juce::String s = "o" + juce::String (o + 1);
    const int src   = juce::roundToInt (proc.apvts.getRawParameterValue (s + "src")->load());
    const int shape = juce::roundToInt (proc.apvts.getRawParameterValue (s + "shape")->load());
    auto& cb = oscUi[o].sel;

    if (src == 1)
    {
        const juce::String name = proc.getWavetableName (o);
        for (int i = 0; i < cb.getNumItems(); ++i)
            if (cb.getItemId (i) >= 100 && cb.getItemText (i) == name)
            {
                cb.setSelectedItemIndex (i, juce::dontSendNotification);
                return;
            }
    }
    cb.setSelectedId (shape + 1, juce::dontSendNotification);
}

void VowelSynthEditor::stepOsc (int o, int dir)
{
    auto& cb = oscUi[o].sel;
    const int n = cb.getNumItems();
    if (n <= 0) return;
    int i = cb.getSelectedItemIndex();
    i = ((i < 0 ? 0 : i) + dir + n) % n;
    cb.setSelectedItemIndex (i, juce::sendNotificationSync);
}

void VowelSynthEditor::showLfo (int idx)
{
    currentLfo = idx;
    for (int l = 0; l < kNumLfos; ++l)
    {
        const bool v = (l == idx);
        lfoShapeCh[l]->setShown (v);
        lfoDestCh[l]->setShown (v);
        for (auto* k : lfoKnobs[l])
            k->setShown (v);
    }
    lfoDisplay.repaint();
}

void VowelSynthEditor::timerCallback()
{
    for (int o = 0; o < 2; ++o)
        syncOscSelection (o);
}

void VowelSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (kWindowBg);

    g.setColour (juce::Colour (0xff1b222b));
    g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    g.drawText ("VowelSynth", 10, 8, 300, 22, juce::Justification::centredLeft);

    for (auto& p : panels)
    {
        g.setColour (kPanelBody);
        g.fillRoundedRectangle (p.r.toFloat(), 4.0f);

        auto h = p.r.withHeight (22);
        g.setColour (kPanelHeader);
        g.fillRoundedRectangle (h.toFloat(), 4.0f);
        g.setColour (juce::Colour (0xff10161d));
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (p.title, h.reduced (8, 0), juce::Justification::centredLeft);
    }
}

void VowelSynthEditor::layoutKnobs (juce::Rectangle<int> r, const std::vector<Knob*>& ks)
{
    if (ks.empty()) return;
    const int w = r.getWidth() / (int) ks.size();
    for (auto* k : ks)
    {
        auto cell = r.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (14));
        k->slider.setBounds (cell);
    }
}

void VowelSynthEditor::layoutKnobGrid (juce::Rectangle<int> r, const std::vector<Knob*>& ks, int cols)
{
    if (ks.empty()) return;
    const int rows = ((int) ks.size() + cols - 1) / cols;
    const int w = r.getWidth() / cols;
    const int h = r.getHeight() / rows;
    for (size_t i = 0; i < ks.size(); ++i)
    {
        juce::Rectangle<int> cell (r.getX() + (int) (i % (size_t) cols) * w,
                                   r.getY() + (int) (i / (size_t) cols) * h, w, h);
        ks[i]->label.setBounds (cell.removeFromTop (14));
        ks[i]->slider.setBounds (cell);
    }
}

void VowelSynthEditor::layoutChooser (juce::Rectangle<int> r, Chooser& c)
{
    r = r.reduced (3, 0);
    c.label.setBounds (r.removeFromTop (16));
    c.box.setBounds (r.removeFromTop (24));
}

void VowelSynthEditor::resized()
{
    panels.clear();
    auto area = getLocalBounds().reduced (8);

    // title strip with wavetable buttons
    auto top = area.removeFromTop (22);
    openFolderBtn.setBounds (top.removeFromRight (170));
    top.removeFromRight (4);
    rescanBtn.setBounds (top.removeFromRight (70));
    area.removeFromTop (4);

    // ---- top row: osc A, osc B, vowel filter, mix/master ----
    auto row1 = area.removeFromTop (218);
    const int pw = (row1.getWidth() - 3 * 6) / 4;
    auto nextPanel = [&]
    {
        auto p = row1.removeFromLeft (pw);
        row1.removeFromLeft (6);
        return p;
    };

    for (int o = 0; o < 2; ++o)
    {
        auto p = nextPanel();
        panels.push_back ({ p, o == 0 ? "OSC A" : "OSC B" });

        auto body = p;
        body.removeFromTop (22);
        body = body.reduced (6, 4);

        auto sel = body.removeFromTop (24);
        oscUi[o].next.setBounds (sel.removeFromRight (24));
        oscUi[o].prev.setBounds (sel.removeFromRight (24));
        oscUi[o].sel.setBounds (sel);
        body.removeFromTop (4);

        (o == 0 ? waveA : waveB).setBounds (body.removeFromTop (70));
        layoutKnobs (body, oscKnobs[o]);
    }

    {
        auto p = nextPanel();
        panels.push_back ({ p, "VOWEL FILTER" });
        auto body = p;
        body.removeFromTop (22);
        layoutKnobGrid (body.reduced (6, 6), vowelKnobs, 3);
    }
    {
        auto p = nextPanel();
        panels.push_back ({ p, "MIX / OUT" });
        auto body = p;
        body.removeFromTop (22);
        layoutKnobGrid (body.reduced (6, 6).removeFromTop (110), mainKnobs, 2);
    }

    // ---- lower area ----
    area.removeFromTop (6);

    auto keys = area.removeFromRight (56);
    keyboard.setBounds (keys);
    keyboard.setKeyWidth (juce::jmax (8.0f, (float) keys.getHeight() / 21.0f));
    area.removeFromRight (6);

    auto left = area.removeFromLeft (570);
    area.removeFromLeft (6);

    // vowel grid + X / Y sliders
    {
        auto pa = area;
        auto xRow = pa.removeFromBottom (28);
        auto yCol = pa.removeFromLeft (60);

        pad.setBounds (pa);

        xLabel.setBounds (xRow.removeFromLeft (60));
        xSlider.setBounds (xRow);

        yLabel.setBounds (yCol.removeFromTop (18));
        ySlider.setBounds (yCol);
    }

    // LFO panel
    auto lfoP = left.removeFromTop (236);
    panels.push_back ({ lfoP, "LFO" });
    {
        auto body = lfoP;
        body.removeFromTop (22);
        body = body.reduced (6, 4);

        auto tabs = body.removeFromTop (24);
        const int tw = tabs.getWidth() / kNumLfos;
        for (int l = 0; l < kNumLfos; ++l)
            lfoTabs[l].setBounds (tabs.removeFromLeft (tw));
        body.removeFromTop (6);

        lfoDisplay.setBounds (body.removeFromLeft (330));
        body.removeFromLeft (8);

        auto shapeRow = body.removeFromTop (44);
        auto destRow  = body.removeFromTop (44);
        for (int l = 0; l < kNumLfos; ++l)
        {
            layoutChooser (shapeRow, *lfoShapeCh[l]);
            layoutChooser (destRow, *lfoDestCh[l]);
            layoutKnobs (body, lfoKnobs[l]);
        }
    }

    // envelope panel
    left.removeFromTop (6);
    panels.push_back ({ left, "ENV 1  (AMP)" });
    {
        auto body = left;
        body.removeFromTop (22);
        body = body.reduced (6, 6);
        envDisplay.setBounds (body.removeFromLeft (330));
        body.removeFromLeft (8);
        layoutKnobGrid (body, envKnobs, 2);
    }
}
