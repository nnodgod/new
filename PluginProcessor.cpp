#include "PluginProcessor.h"
#include "PluginEditor.h"

VowelSynthProcessor::VowelSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    synth.addSound (new SynthSound());
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new SynthVoice (vp));

    getWavetableFolder().createDirectory();   // so the user can find it and drop .wav files in
}

juce::AudioProcessorValueTreeState::ParameterLayout VowelSynthProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto addF = [&] (const String& id, const String& name, NormalisableRange<float> r, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def));
    };
    auto addC = [&] (const String& id, const String& name, const StringArray& items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto addI = [&] (const String& id, const String& name, int lo, int hi, int def)
    {
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def));
    };

    // Vowel filter
    addF ("x",     "Vowel X",       { 0.0f, 1.0f }, 0.5f);
    addF ("y",     "Vowel Y",       { 0.0f, 1.0f }, 0.1f);
    addF ("res",   "Vowel Resonance", { 2.0f, 30.0f, 0.01f, 0.5f }, 10.0f);
    addF ("shift", "Formant Shift", { 0.5f, 2.0f, 0.001f, 0.5f }, 1.0f);
    addF ("glide", "Vowel Glide (ms)", { 1.0f, 500.0f, 0.1f, 0.5f }, 30.0f);
    addF ("vmix",  "Vowel Mix",     { 0.0f, 1.0f }, 1.0f);
    addF ("vgain", "Vowel Gain (dB)", { -12.0f, 24.0f, 0.1f }, 6.0f);

    // Oscillators
    for (int o = 1; o <= 2; ++o)
    {
        const String s = "o" + String (o);
        addC (s + "src",   "Osc " + String (o) + " Source", kOscSources, 0);
        addC (s + "shape", "Osc " + String (o) + " Shape", kOscShapes, o == 1 ? 2 : 3);
        addI (s + "oct",   "Osc " + String (o) + " Octave", -3, 3, 0);
        addF (s + "tune",  "Osc " + String (o) + " Tune (cents)", { -100.0f, 100.0f, 0.1f }, o == 1 ? 0.0f : 7.0f);
        addF (s + "amt",   "Osc " + String (o) + " Shape Amt / WT Pos", { 0.0f, 1.0f }, 0.0f);
    }
    addF ("omix", "Osc Mix", { 0.0f, 1.0f }, 0.0f);

    // Envelope + master
    addF ("atk", "Attack",  { 0.001f, 5.0f, 0.001f, 0.4f }, 0.01f);
    addF ("dec", "Decay",   { 0.001f, 5.0f, 0.001f, 0.4f }, 0.25f);
    addF ("sus", "Sustain", { 0.0f, 1.0f }, 0.7f);
    addF ("rel", "Release", { 0.001f, 8.0f, 0.001f, 0.4f }, 0.3f);
    addF ("master", "Master (dB)", { -36.0f, 6.0f, 0.1f }, -6.0f);

    // LFOs
    for (int l = 1; l <= kNumLfos; ++l)
    {
        const String s = "l" + String (l);
        addF (s + "rate",  "LFO " + String (l) + " Rate (Hz)", { 0.02f, 20.0f, 0.001f, 0.4f }, 0.5f);
        addC (s + "shape", "LFO " + String (l) + " Shape", kLfoShapes, 0);
        addF (s + "depth", "LFO " + String (l) + " Depth", { -1.0f, 1.0f, 0.001f }, 0.5f);
        addC (s + "dest",  "LFO " + String (l) + " Dest", kLfoDests, 0);
    }

    return { p.begin(), p.end() };
}

// ---------------------------------------------------------------- wavetables

juce::File VowelSynthProcessor::getWavetableFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("VowelSynth").getChildFile ("Wavetables");
}

juce::StringArray VowelSynthProcessor::listWavetables()
{
    juce::Array<juce::File> files;
    getWavetableFolder().findChildFiles (files, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");

    juce::StringArray names;
    for (auto& f : files)
        names.add (f.getFileName());
    names.sortNatural();
    return names;
}

bool VowelSynthProcessor::setWavetable (int osc, const juce::String& fileName)
{
    osc = juce::jlimit (0, 1, osc);

    std::shared_ptr<const WaveTable> t;
    if (fileName.isNotEmpty())
    {
        t = WaveTable::loadFile (getWavetableFolder().getChildFile (fileName));
        if (t == nullptr)
            return false;
    }

    uiTable[osc] = t;
    {
        const juce::SpinLock::ScopedLockType lock (tableLock);
        pendingTable[osc] = t;
        tableDirty[osc] = true;
    }

    apvts.state.setProperty (osc == 0 ? "o1wt" : "o2wt", fileName, nullptr);
    return true;
}

juce::String VowelSynthProcessor::getWavetableName (int osc) const
{
    return apvts.state.getProperty (osc == 0 ? "o1wt" : "o2wt").toString();
}

void VowelSynthProcessor::reloadWavetablesFromState()
{
    for (int o = 0; o < 2; ++o)
        setWavetable (o, getWavetableName (o));
}

void VowelSynthProcessor::getOscPreview (int osc, float* out, int n)
{
    const juce::String s = "o" + juce::String (osc + 1);
    const int src   = juce::roundToInt (apvts.getRawParameterValue (s + "src")->load());
    const int shape = juce::roundToInt (apvts.getRawParameterValue (s + "shape")->load());
    const float amt = apvts.getRawParameterValue (s + "amt")->load();

    if (src == 1 && uiTable[osc] != nullptr)
    {
        for (int i = 0; i < n; ++i)
            out[i] = uiTable[osc]->read ((double) i / (double) n, 0, amt);
        return;
    }

    Oscillator o;
    juce::Random rnd (1234);
    for (int i = 0; i < n; ++i)
        out[i] = o.process (shape, 1.0, (double) n, amt, rnd);
}

// ---------------------------------------------------------------- audio

void VowelSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    for (auto& ch : filters)
        for (auto& f : ch)
        {
            f.prepare (spec);
            f.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
            f.reset();
        }

    computeFormants (apvts.getRawParameterValue ("x")->load(),
                     apvts.getRawParameterValue ("y")->load(),
                     curF[0], curF[1], curF[2]);
}

bool VowelSynthProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// Inverse-distance weighting of the 5 vowels, interpolating in log-frequency
void VowelSynthProcessor::computeFormants (float x, float y, float& f1, float& f2, float& f3)
{
    float wSum = 0.0f, l1 = 0.0f, l2 = 0.0f, l3 = 0.0f;
    for (auto& v : kVowels)
    {
        const float dx = x - v.padX, dy = y - v.padY;
        const float d2 = dx * dx + dy * dy;
        const float w = 1.0f / (d2 * d2 + 1.0e-5f);
        wSum += w;
        l1 += w * std::log (v.f1);
        l2 += w * std::log (v.f2);
        l3 += w * std::log (v.f3);
    }
    f1 = std::exp (l1 / wSum);
    f2 = std::exp (l2 / wSum);
    f3 = std::exp (l3 / wSum);
}

float VowelSynthProcessor::lfoShapeValue (int shape, float p, float rnd)
{
    switch (shape)
    {
        case 0:  return std::sin (juce::MathConstants<float>::twoPi * p);
        case 1:  return 1.0f - 4.0f * std::abs (p - 0.5f);
        case 2:  return 2.0f * p - 1.0f;
        case 3:  return p < 0.5f ? 1.0f : -1.0f;
        default: return rnd;
    }
}

float VowelSynthProcessor::lfoValue (int i, int shape) const
{
    return lfoShapeValue (shape, (float) lfoPhase[i], shValue[i]);
}

void VowelSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());

    // notes played on the on-screen keyboard
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // pick up wavetables the UI thread has finished loading
    {
        const juce::SpinLock::ScopedTryLockType lock (tableLock);
        if (lock.isLocked())
            for (int o = 0; o < 2; ++o)
                if (tableDirty[o])
                {
                    std::swap (activeTable[o], pendingTable[o]);   // old table is released later on the UI thread
                    tableDirty[o] = false;
                }
    }

    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    // ---- read base parameters once per block ----
    const float bx = get ("x"), by = get ("y");
    const float bres = get ("res"), bshift = get ("shift"), bglide = get ("glide");
    const float vmix = get ("vmix");
    const float vgain = juce::Decibels::decibelsToGain (get ("vgain"));
    const float master = juce::Decibels::decibelsToGain (get ("master"));
    const float bmix = get ("omix");
    const float bamt1 = get ("o1amt"), bamt2 = get ("o2amt");
    const float btune2 = get ("o2tune");

    for (int o = 0; o < 2; ++o)
    {
        const juce::String s = "o" + juce::String (o + 1);
        vp.shape[o]    = juce::roundToInt (get (s + "shape"));
        vp.octave[o]   = juce::roundToInt (get (s + "oct"));
        vp.table[o]    = activeTable[o].get();
        vp.useTable[o] = juce::roundToInt (get (s + "src")) == 1 && vp.table[o] != nullptr;
    }
    vp.tuneCents[0] = get ("o1tune");
    vp.attack = get ("atk");
    vp.decay = get ("dec");
    vp.sustain = get ("sus");
    vp.release = get ("rel");

    float lfoRate[kNumLfos], lfoDepth[kNumLfos];
    int lfoShape[kNumLfos], lfoDest[kNumLfos];
    for (int l = 0; l < kNumLfos; ++l)
    {
        const juce::String s = "l" + juce::String (l + 1);
        lfoRate[l]  = get (s + "rate");
        lfoDepth[l] = get (s + "depth");
        lfoShape[l] = juce::roundToInt (get (s + "shape"));
        lfoDest[l]  = juce::jlimit (0, (int) NumDest - 1, juce::roundToInt (get (s + "dest")));
    }

    constexpr float bandGain[kBands] = { 1.0f, 0.7f, 0.4f };
    const float nyq = (float) sr * 0.45f;
    const float glideSec = juce::jmax (0.001f, bglide * 0.001f);

    constexpr int chunk = 16;
    for (int pos = 0; pos < numSamples; pos += chunk)
    {
        const int n = juce::jmin (chunk, numSamples - pos);

        // ---- LFOs ----
        float mod[NumDest] {};
        for (int l = 0; l < kNumLfos; ++l)
        {
            mod[lfoDest[l]] += lfoDepth[l] * lfoValue (l, lfoShape[l]);

            lfoPhase[l] += (double) lfoRate[l] * (double) n / sr;
            if (lfoPhase[l] >= 1.0)
            {
                lfoPhase[l] -= std::floor (lfoPhase[l]);
                shValue[l] = rng.nextFloat() * 2.0f - 1.0f;
            }
        }
        mod[DestOff] = 0.0f;

        const float x = juce::jlimit (0.0f, 1.0f, bx + mod[DestVowelX]);
        const float y = juce::jlimit (0.0f, 1.0f, by + mod[DestVowelY]);
        const float res = juce::jlimit (2.0f, 30.0f, bres + mod[DestRes] * 14.0f);
        const float shift = bshift * std::exp2 (mod[DestShift]);

        vp.pitchSemis = mod[DestPitch] * 12.0f;
        vp.mix = juce::jlimit (0.0f, 1.0f, bmix + mod[DestOscMix]);
        vp.amt[0] = juce::jlimit (0.0f, 1.0f, bamt1 + mod[DestOsc1Amt]);
        vp.amt[1] = juce::jlimit (0.0f, 1.0f, bamt2 + mod[DestOsc2Amt]);
        vp.tuneCents[1] = btune2 + mod[DestOsc2Detune] * 50.0f;
        vp.ampMul = juce::jmax (0.0f, 1.0f + 0.5f * mod[DestTremolo]);

        // ---- oscillators ----
        synth.renderNextBlock (buffer, midi, pos, n);

        // ---- vowel filter ----
        float tgt[kBands];
        computeFormants (x, y, tgt[0], tgt[1], tgt[2]);
        const float k = 1.0f - std::exp (-(float) n / ((float) sr * glideSec));
        for (int b = 0; b < kBands; ++b)
        {
            const float target = juce::jmin (tgt[b] * shift, nyq);
            curF[b] += (target - curF[b]) * k;
        }

        const float invRes = 1.0f / res;   // state-variable band-pass peaks at gain = Q; normalise to 1

        for (int ch = 0; ch < numCh; ++ch)
        {
            for (int b = 0; b < kBands; ++b)
            {
                filters[ch][b].setCutoffFrequency (curF[b]);
                filters[ch][b].setResonance (res);
            }

            float* data = buffer.getWritePointer (ch) + pos;
            for (int i = 0; i < n; ++i)
            {
                const float dry = data[i];
                float wet = 0.0f;
                for (int b = 0; b < kBands; ++b)
                    wet += bandGain[b] * invRes * filters[ch][b].processSample (0, dry);

                const float mixed = dry * (1.0f - vmix) + wet * vmix * vgain;
                data[i] = std::tanh (mixed * master * 2.0f) * 0.5f;   // soft clip as a safety net
            }
        }
    }

    midi.clear();
}

juce::AudioProcessorEditor* VowelSynthProcessor::createEditor()
{
    return new VowelSynthEditor (*this);
}

void VowelSynthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void VowelSynthProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            reloadWavetablesFromState();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VowelSynthProcessor();
}
