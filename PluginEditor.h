#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// ------------------------------------------------------------------ vowel grid
class VowelPad : public juce::Component, private juce::Timer
{
public:
    explicit VowelPad (juce::AudioProcessorValueTreeState& s) : apvts (s)
    {
        startTimerHz (60);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::white);
        g.fillRect (r);

        g.setColour (juce::Colour (0xff2b3440));
        for (int i = 0; i <= 6; ++i)
        {
            g.drawVerticalLine   ((int) (r.getX() + (r.getWidth() - 1.0f)  * i / 6.0f), r.getY(), r.getBottom());
            g.drawHorizontalLine ((int) (r.getY() + (r.getHeight() - 1.0f) * i / 6.0f), r.getX(), r.getRight());
        }

        g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
        for (auto& v : kVowels)
        {
            auto p = toScreen (v.padX, v.padY, r);
            g.setColour (juce::Colour (0xff29a8e0).withAlpha (0.25f));
            g.fillEllipse (p.x - 22, p.y - 22, 44, 44);
            g.setColour (juce::Colour (0xff1b222b));
            g.drawText (v.name, juce::Rectangle<float> (p.x - 22, p.y - 22, 44, 44), juce::Justification::centred);
        }

        auto d = toScreen (px, py, r);
        g.setColour (juce::Colour (0xffffb347).withAlpha (0.3f));
        g.fillEllipse (d.x - 20, d.y - 20, 40, 40);
        g.setColour (juce::Colour (0xffff8a00));
        g.fillEllipse (d.x - 9, d.y - 9, 18, 18);
        g.setColour (juce::Colour (0xff1b222b));
        g.drawEllipse (d.x - 9, d.y - 9, 18, 18, 1.5f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto* n : { "x", "y" })
            apvts.getParameter (n)->beginChangeGesture();
        setFromMouse (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { setFromMouse (e); }
    void mouseUp (const juce::MouseEvent&) override
    {
        for (auto* n : { "x", "y" })
            apvts.getParameter (n)->endChangeGesture();
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    float px = 0.5f, py = 0.1f;

    static juce::Point<float> toScreen (float x, float y, juce::Rectangle<float> r)
    {
        auto inner = r.reduced (30.0f);
        return { inner.getX() + x * inner.getWidth(), inner.getBottom() - y * inner.getHeight() };
    }

    void setFromMouse (const juce::MouseEvent& e)
    {
        auto inner = getLocalBounds().toFloat().reduced (31.0f);
        float x = juce::jlimit (0.0f, 1.0f, (e.position.x - inner.getX()) / inner.getWidth());
        float y = juce::jlimit (0.0f, 1.0f, (inner.getBottom() - e.position.y) / inner.getHeight());
        apvts.getParameter ("x")->setValueNotifyingHost (x);
        apvts.getParameter ("y")->setValueNotifyingHost (y);
    }

    void timerCallback() override
    {
        float nx = apvts.getRawParameterValue ("x")->load();
        float ny = apvts.getRawParameterValue ("y")->load();
        if (nx != px || ny != py) { px = nx; py = ny; repaint(); }
    }
};

// ------------------------------------------------------------------ oscillator waveform display
class WaveDisplay : public juce::Component, private juce::Timer
{
public:
    WaveDisplay (VowelSynthProcessor& p, int o) : proc (p), osc (o) { startTimerHz (20); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff10161d));
        g.fillRect (r);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        float pts[kPts];
        proc.getOscPreview (osc, pts, kPts);

        juce::Path path;
        for (int i = 0; i < kPts; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (kPts - 1);
            const float y = r.getCentreY() - juce::jlimit (-1.2f, 1.2f, pts[i]) * r.getHeight() * 0.4f;
            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff8bd450));
        g.strokePath (path, juce::PathStrokeType (1.6f));
    }

private:
    static constexpr int kPts = 256;
    VowelSynthProcessor& proc;
    int osc;
    void timerCallback() override { repaint(); }
};

// ------------------------------------------------------------------ LFO shape display
class LfoDisplay : public juce::Component, private juce::Timer
{
public:
    LfoDisplay (VowelSynthProcessor& p, std::function<int()> idx) : proc (p), getIdx (std::move (idx))
    {
        startTimerHz (15);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff10161d));
        g.fillRect (r);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        const juce::String id = "l" + juce::String (getIdx() + 1);
        const int shape = juce::roundToInt (proc.apvts.getRawParameterValue (id + "shape")->load());
        const float depth = proc.apvts.getRawParameterValue (id + "depth")->load();

        static const float steps[4] { 0.6f, -0.4f, 0.9f, -0.7f };
        juce::Path path;
        constexpr int n = 200;
        for (int i = 0; i <= n; ++i)
        {
            const float ph = (float) i / (float) n * 0.9999f;
            const float v = VowelSynthProcessor::lfoShapeValue (shape, ph, steps[juce::jmin (3, (int) (ph * 4.0f))]);
            const float x = r.getX() + r.getWidth() * (float) i / (float) n;
            const float y = r.getCentreY() - v * depth * r.getHeight() * 0.4f;
            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff29a8e0));
        g.strokePath (path, juce::PathStrokeType (1.8f));
    }

private:
    VowelSynthProcessor& proc;
    std::function<int()> getIdx;
    void timerCallback() override { repaint(); }
};

// ------------------------------------------------------------------ envelope display
class EnvDisplay : public juce::Component, private juce::Timer
{
public:
    explicit EnvDisplay (VowelSynthProcessor& p) : proc (p) { startTimerHz (15); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff10161d));
        g.fillRect (r);

        auto get = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
        const float ta = std::sqrt (get ("atk")), td = std::sqrt (get ("dec")), tr = std::sqrt (get ("rel"));
        const float sus = get ("sus");
        const float th = 0.25f * (ta + td + tr) + 0.1f;
        const float total = ta + td + th + tr;

        const float w = r.getWidth() - 16.0f;
        const float x0 = r.getX() + 8.0f;
        const float x1 = x0 + w * ta / total;
        const float x2 = x1 + w * td / total;
        const float x3 = x2 + w * th / total;
        const float x4 = x3 + w * tr / total;
        const float yTop = r.getY() + 10.0f, yBase = r.getBottom() - 10.0f;
        const float yS = yBase - (yBase - yTop) * sus;

        juce::Path path;
        path.startNewSubPath (x0, yBase);
        path.lineTo (x1, yTop);
        path.lineTo (x2, yS);
        path.lineTo (x3, yS);
        path.lineTo (x4, yBase);

        juce::Path fill (path);
        fill.closeSubPath();
        g.setColour (juce::Colour (0xff29a8e0).withAlpha (0.18f));
        g.fillPath (fill);
        g.setColour (juce::Colour (0xff29a8e0));
        g.strokePath (path, juce::PathStrokeType (1.8f));
    }

private:
    VowelSynthProcessor& proc;
    void timerCallback() override { repaint(); }
};

// ------------------------------------------------------------------ editor
class VowelSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VowelSynthEditor (VowelSynthProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        void setShown (bool v) { slider.setVisible (v); label.setVisible (v); }
    };
    struct Chooser
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
        void setShown (bool v) { box.setVisible (v); label.setVisible (v); }
    };
    struct OscUi
    {
        juce::ComboBox sel;
        juce::TextButton prev { "<" }, next { ">" };
    };
    struct Panel
    {
        juce::Rectangle<int> r;
        juce::String title;
    };

    Knob*    addKnob (const juce::String& id, const juce::String& name);
    Chooser* addChooser (const juce::String& id, const juce::String& name, const juce::StringArray& items);
    void styleSlider (juce::Slider& s);
    void setChoiceParam (const juce::String& id, int index);
    void refreshOscSelectors();
    void syncOscSelection (int o);
    void stepOsc (int o, int dir);
    void showLfo (int idx);
    void timerCallback() override;

    static void layoutKnobs (juce::Rectangle<int> r, const std::vector<Knob*>& ks);
    static void layoutKnobGrid (juce::Rectangle<int> r, const std::vector<Knob*>& ks, int cols);
    static void layoutChooser (juce::Rectangle<int> r, Chooser& c);

    VowelSynthProcessor& proc;
    int currentLfo = 0;

    VowelPad pad;
    WaveDisplay waveA, waveB;
    LfoDisplay lfoDisplay;
    EnvDisplay envDisplay;
    juce::MidiKeyboardComponent keyboard;

    // sliders for the dot position (same parameters as the grid, so automate either one)
    juce::Slider xSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider ySlider { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    juce::Label xLabel, yLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xAtt, yAtt;

    juce::TextButton lfoTabs[kNumLfos];
    juce::TextButton openFolderBtn { "Open Wavetable Folder" }, rescanBtn { "Rescan" };
    OscUi oscUi[2];

    std::vector<std::unique_ptr<Knob>> ownedKnobs;
    std::vector<std::unique_ptr<Chooser>> ownedChoosers;

    std::vector<Knob*> vowelKnobs, mainKnobs, envKnobs, oscKnobs[2], lfoKnobs[kNumLfos];
    Chooser* lfoShapeCh[kNumLfos] {};
    Chooser* lfoDestCh[kNumLfos] {};

    std::vector<Panel> panels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VowelSynthEditor)
};
