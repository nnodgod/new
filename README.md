# VowelSynth

An 8-voice VST3 synth for FL Studio: two oscillators (built-in shapes or your own wavetables), six assignable LFOs, one amp envelope and a vowel (A/E/I/O/U) formant filter you control by dragging a dot on a grid.

## Easiest way to build (no Visual Studio)

1. Make a free GitHub repo and upload everything in this folder (including the hidden `.github` folder).
2. Open the **Actions** tab. The build starts by itself (about 10-15 minutes).
3. Download **VowelSynth-Setup** from the finished run, unzip it and run the .exe.
4. In FL Studio: Options > Manage plugins > Find installed plugins. VowelSynth appears under your instruments.

## Build locally

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Run from the x64 Native Tools Command Prompt for VS, as administrator, inside this folder.

## Your own wavetables

Drop .wav files into:

    Documents\VowelSynth\Wavetables

(the plugin creates this folder the first time it loads; the "Open Wavetable Folder" button opens it). Press **Rescan** and they appear in each oscillator's dropdown below the built-in shapes. The < > buttons step through the list.

- **Single-cycle** WAVs (any length) are resampled to one cycle.
- **Multi-frame wavetables**: a WAV whose length is an exact multiple of 2048 samples is read as frames of 2048 samples (Serum style), up to 256 frames. The **Pos / Amt** knob scrolls through the frames, and any LFO can modulate it (Osc1 Amt / Osc2 Amt).
- Mono is used (left channel of stereo files).

## Built-in shapes

Sine, Triangle, Saw, Square, **Supersaw**, **Wavefold**, **FM**, Noise. "Pos / Amt" does something different per shape: pulse width (Square), unison spread (Supersaw), fold depth (Wavefold), FM index (FM).

## LFOs

Six LFOs (tabs). Each has a rate, shape (Sine, Triangle, Saw, Square, Random), depth (negative inverts) and one destination:
Vowel X, Vowel Y, Pitch, Osc Mix, Osc1 Amt, Osc2 Amt, Resonance, Formant Shift, Tremolo, Osc2 Detune.
They all start on Off, so pick a destination to hear them. Several LFOs can share a destination and add up.

## Vowel filter

Drag the dot on the grid, or use the X slider under it and the Y slider beside it (they're the same parameters, so you can automate either in FL). Corners are the vowels, in between they morph. Resonance sharpens the vowels, Shift moves all formants (deeper or smaller voice), Glide smooths the movement, Vowel Mix blends dry/filtered, Vowel Gain makes up the level.

The strip on the far right is a piano keyboard for testing without a MIDI keyboard.
