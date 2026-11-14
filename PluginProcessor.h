#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SynthVoice.h"
#include "WaveTable.h"

struct Vowel
{
    const char* name;
    float padX, padY;      // position on the pad (0..1, y=1 is top)
    float f1, f2, f3;      // formant frequencies in Hz
};

inline const Vowel kVowels[5] = {
    { "A", 0.50f, 0.10f, 730.0f, 1090.0f, 2440.0f },
    { "E", 0.25f, 0.55f, 530.0f, 1840.0f, 2480.0f },
    { "I", 0.10f, 0.90f, 270.0f, 2290.0f, 3010.0f },
    { "O", 0.75f, 0.55f, 570.0f,  840.0f, 2410.0f },
    { "U", 0.90f, 0.90f, 300.0f,  870.0f, 2240.0f },
};

inline const juce::StringArray kOscShapes  { "Sine", "Triangle", "Saw", "Square", "Supersaw", "Wavefold", "FM", "Noise" };
inline const juce::StringArray kOscSources { "Shapes", "Wavetable" };
inline const juce::StringArray kLfoShapes  { "Sine", "Triangle", "Saw", "Square", "Random" };
inline const juce::StringArray kLfoDests   { "Off", "Vowel X", "Vowel Y", "Pitch", "Osc Mix", "Osc1 Amt",
                                             "Osc2 Amt", "Resonance", "Formant Shift", "Tremolo", "Osc2 Detune" };

enum Dest { DestOff = 0, DestVowelX, DestVowelY, DestPitch, DestOscMix, DestOsc1Amt,
            DestOsc2Amt, DestRes, DestShift, DestTremolo, DestOsc2Detune, NumDest };

inline constexpr int kNumLfos = 6;

class VowelSynthProcessor : public juce::AudioProcessor
{
public:
    VowelSynthProcessor();
    ~VowelSynthProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VowelSynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- wavetables (message thread) ----
    static juce::File getWavetableFolder();           // Documents/VowelSynth/Wavetables
    static juce::StringArray listWavetables();        // file names found in that folder
    bool setWavetable (int osc, const juce::String& fileName);   // empty name = clear
    juce::String getWavetableName (int osc) const;
    void reloadWavetablesFromState();

    // one cycle of the oscillator for the UI display
    void getOscPreview (int osc, float* out, int n);
    static float lfoShapeValue (int shape, float phase, float rnd);

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static void computeFormants (float x, float y, float& f1, float& f2, float& f3);
    float lfoValue (int i, int shape) const;

    static constexpr int kBands = 3;

    VoiceParams vp;                 // must be declared before synth (voices hold a reference)
    juce::Synthesiser synth;

    // wavetable hand-off: UI thread -> audio thread
    std::shared_ptr<const WaveTable> uiTable[2], activeTable[2], pendingTable[2];
    bool tableDirty[2] { false, false };
    juce::SpinLock tableLock;

    juce::dsp::StateVariableTPTFilter<float> filters[2][kBands];
    float curF[kBands] { 700.0f, 1100.0f, 2400.0f };

    double lfoPhase[kNumLfos] {};
    float  shValue[kNumLfos]  {};
    juce::Random rng;

    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VowelSynthProcessor)
};
