#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "WaveTable.h"

// Values shared by every voice. The processor refreshes these every few samples
// (this is where LFO modulation lands).
struct VoiceParams
{
    int   shape[2]     { 2, 3 };
    int   octave[2]    { 0, 0 };
    float tuneCents[2] { 0.0f, 0.0f };
    float amt[2]       { 0.0f, 0.0f };
    const WaveTable* table[2] { nullptr, nullptr };
    bool  useTable[2]  { false, false };
    float mix          = 0.0f;
    float pitchSemis   = 0.0f;
    float ampMul       = 1.0f;
    float attack = 0.01f, decay = 0.25f, sustain = 0.7f, release = 0.3f;
};

struct Oscillator
{
    double phase = 0.0;
    double sub[7] {};

    void randomise (juce::Random& r)
    {
        phase = r.nextDouble();
        for (auto& p : sub) p = r.nextDouble();
    }

    static float blep (double t, double dt)
    {
        if (t < dt)        { t /= dt;            return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt)  { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    float processTable (const WaveTable& wt, double freq, double sr, float pos)
    {
        const double dt = juce::jlimit (1.0e-5, 0.45, freq / sr);
        const float out = wt.read (phase, WaveTable::mipForFrequency (freq, sr), pos);
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return out;
    }

    // shapes: 0 Sine, 1 Triangle, 2 Saw, 3 Square (amt = pulse width), 4 Supersaw (amt = spread),
    //         5 Wavefold (amt = fold), 6 FM (amt = index), 7 Noise
    float process (int shape, double freq, double sr, float amt, juce::Random& rnd)
    {
        const double dt = juce::jlimit (1.0e-5, 0.45, freq / sr);
        const double t = phase;
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        float out = 0.0f;

        switch (shape)
        {
            case 0: out = (float) std::sin (twoPi * t); break;
            case 1: out = (float) (1.0 - 4.0 * std::abs (t - 0.5)); break;
            case 2: out = (float) (2.0 * t - 1.0) - blep (t, dt); break;
            case 3:
            {
                const double pw = 0.5 - 0.45 * (double) amt;
                double v = (t < pw) ? 1.0 : -1.0;
                v += (double) blep (t, dt);
                double t2 = t - pw;
                if (t2 < 0.0) t2 += 1.0;
                v -= (double) blep (t2, dt);
                out = (float) v;
                break;
            }
            case 4:
            {
                static constexpr double offs[7] { -1.0, -0.6, -0.25, 0.0, 0.25, 0.6, 1.0 };
                const double spread = 8.0 + 52.0 * (double) amt;   // cents
                float sum = 0.0f;
                for (int i = 0; i < 7; ++i)
                {
                    const double dti = juce::jlimit (1.0e-5, 0.45, dt * std::exp2 (offs[i] * spread / 1200.0));
                    const double ti = sub[i];
                    sum += (float) (2.0 * ti - 1.0) - blep (ti, dti);
                    sub[i] += dti;
                    if (sub[i] >= 1.0) sub[i] -= 1.0;
                }
                out = sum * 0.3f;
                break;
            }
            case 5:
            {
                const double x = std::sin (twoPi * t);
                out = (float) std::sin (x * (1.0 + 7.0 * (double) amt));
                break;
            }
            case 6:
                out = (float) std::sin (twoPi * t + 8.0 * (double) amt * std::sin (twoPi * 2.0 * t));
                break;
            default:
                out = rnd.nextFloat() * 2.0f - 1.0f;
                break;
        }

        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return out;
    }
};

struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (const VoiceParams& p) : vp (p) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SynthSound*> (s) != nullptr;
    }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
        sr = newRate > 0.0 ? newRate : 44100.0;
        adsr.setSampleRate (sr);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        baseFreq = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        level = velocity;
        bend = (double) (pitchWheel - 8192) / 8192.0 * 2.0;
        osc[0].randomise (rnd);
        osc[1].randomise (rnd);
        adsr.setParameters ({ vp.attack, vp.decay, vp.sustain, vp.release });
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int v) override { bend = (double) (v - 8192) / 8192.0 * 2.0; }
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        adsr.setParameters ({ vp.attack, vp.decay, vp.sustain, vp.release });

        const double semis = (double) vp.pitchSemis + bend;
        const double f[2] = {
            baseFreq * std::exp2 ((double) vp.octave[0] + (double) vp.tuneCents[0] / 1200.0 + semis / 12.0),
            baseFreq * std::exp2 ((double) vp.octave[1] + (double) vp.tuneCents[1] / 1200.0 + semis / 12.0)
        };

        auto gen = [this, &f] (int i) -> float
        {
            if (vp.useTable[i] && vp.table[i] != nullptr)
                return osc[i].processTable (*vp.table[i], f[i], sr, vp.amt[i]);
            return osc[i].process (vp.shape[i], f[i], sr, vp.amt[i], rnd);
        };

        const int numCh = out.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            const float env = adsr.getNextSample();
            if (! adsr.isActive())
            {
                clearCurrentNote();
                break;
            }

            const float a = gen (0);
            const float b = gen (1);
            const float s = (a * (1.0f - vp.mix) + b * vp.mix) * env * level * vp.ampMul * 0.25f;

            for (int ch = 0; ch < numCh; ++ch)
                out.addSample (ch, startSample + i, s);
        }
    }

private:
    const VoiceParams& vp;
    Oscillator osc[2];
    juce::ADSR adsr;
    juce::Random rnd;
    double baseFreq = 440.0, bend = 0.0, sr = 44100.0;
    float level = 0.0f;
};
