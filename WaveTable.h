#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>

// A wavetable: one or more 2048-sample frames, each stored as 11 band-limited
// copies ("mips") so high notes don't alias.
struct WaveTable
{
    static constexpr int N = 2048;
    static constexpr int numMips = 11;
    static constexpr int maxFrames = 256;

    int numFrames = 1;
    std::vector<float> data;   // [mip][frame][N + 1]  (one guard sample for interpolation)

    const float* frame (int mip, int f) const noexcept
    {
        return data.data() + ((size_t) mip * (size_t) numFrames + (size_t) f) * (size_t) (N + 1);
    }

    float read (double phase, int mip, float pos) const noexcept
    {
        const float fp = juce::jlimit (0.0f, 1.0f, pos) * (float) (numFrames - 1);
        const int f0 = juce::jmin ((int) fp, numFrames - 1);
        const int f1 = juce::jmin (f0 + 1, numFrames - 1);
        const float ft = fp - (float) f0;

        const double idx = phase * (double) N;
        const int i0 = juce::jlimit (0, N - 1, (int) idx);
        const float it = (float) (idx - (double) i0);

        const float* a = frame (mip, f0);
        const float* b = frame (mip, f1);
        const float va = a[i0] + it * (a[i0 + 1] - a[i0]);
        const float vb = b[i0] + it * (b[i0 + 1] - b[i0]);
        return va + ft * (vb - va);
    }

    // pick the mip that keeps all harmonics below Nyquist
    static int mipForFrequency (double freq, double sr) noexcept
    {
        const int maxHarm = juce::jmax (1, (int) (0.5 * sr / juce::jmax (1.0, freq)));
        int m = 0;
        while (m < numMips - 1 && (1024 >> m) > maxHarm)
            ++m;
        return m;
    }

    static std::shared_ptr<const WaveTable> build (std::vector<std::vector<float>>& frames)
    {
        if (frames.empty())
            return nullptr;

        auto wt = std::make_shared<WaveTable>();
        wt->numFrames = (int) frames.size();
        wt->data.assign ((size_t) numMips * (size_t) wt->numFrames * (size_t) (N + 1), 0.0f);

        // remove DC, then normalise the whole table
        float peak = 1.0e-9f;
        for (auto& fr : frames)
        {
            double mean = 0.0;
            for (float s : fr) mean += (double) s;
            mean /= (double) N;
            for (auto& s : fr)
            {
                s -= (float) mean;
                peak = std::max (peak, std::abs (s));
            }
        }
        const float norm = 0.95f / peak;
        for (auto& fr : frames)
            for (auto& s : fr)
                s *= norm;

        juce::dsp::FFT fft (11);   // 2^11 = 2048

        // learn the FFT round-trip scale from an impulse (so we don't depend on library scaling)
        std::vector<float> test ((size_t) (2 * N), 0.0f);
        test[3] = 1.0f;
        fft.performRealOnlyForwardTransform (test.data());
        fft.performRealOnlyInverseTransform (test.data());
        const float invScale = (std::abs (test[3]) > 1.0e-6f) ? 1.0f / test[3] : 1.0f;

        std::vector<float> spec ((size_t) (2 * N));
        std::vector<float> tmp ((size_t) (2 * N));

        for (int f = 0; f < wt->numFrames; ++f)
        {
            std::fill (spec.begin(), spec.end(), 0.0f);
            std::copy (frames[(size_t) f].begin(), frames[(size_t) f].end(), spec.begin());
            fft.performRealOnlyForwardTransform (spec.data());

            for (int m = 0; m < numMips; ++m)
            {
                float* dst = wt->data.data() + ((size_t) m * (size_t) wt->numFrames + (size_t) f) * (size_t) (N + 1);

                if (m == 0)
                {
                    for (int i = 0; i < N; ++i)
                        dst[i] = frames[(size_t) f][(size_t) i];
                }
                else
                {
                    tmp = spec;
                    const int keep = 1024 >> m;
                    tmp[0] = 0.0f;
                    tmp[1] = 0.0f;
                    for (int k = keep + 1; k < N; ++k)
                    {
                        tmp[(size_t) (2 * k)] = 0.0f;
                        tmp[(size_t) (2 * k + 1)] = 0.0f;
                    }
                    fft.performRealOnlyInverseTransform (tmp.data());
                    for (int i = 0; i < N; ++i)
                        dst[i] = tmp[(size_t) i] * invScale;
                }
                dst[N] = dst[0];
            }
        }
        return wt;
    }

    // Loads a .wav/.aif/.flac. A file whose length is an exact multiple of 2048 (and longer than
    // one frame) is read as a multi-frame wavetable (Serum style). Anything else is treated as
    // ONE cycle and resampled to 2048 points.
    static std::shared_ptr<const WaveTable> loadFile (const juce::File& file)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples < 64)
            return nullptr;

        const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) N * maxFrames);
        juce::AudioBuffer<float> buf ((int) juce::jmax<unsigned int> (1u, reader->numChannels), len);
        reader->read (&buf, 0, len, 0, true, true);
        const float* src = buf.getReadPointer (0);

        std::vector<std::vector<float>> frames;
        if (len > N && len % N == 0)
        {
            for (int f = 0; f < len / N; ++f)
                frames.emplace_back (src + f * N, src + (f + 1) * N);
        }
        else
        {
            std::vector<float> one ((size_t) N);
            for (int i = 0; i < N; ++i)
            {
                const double pos = (double) i * (double) len / (double) N;
                const int i0 = (int) pos;
                const int i1 = (i0 + 1) % len;
                const float t = (float) (pos - (double) i0);
                one[(size_t) i] = src[i0] + t * (src[i1] - src[i0]);
            }
            frames.push_back (std::move (one));
        }
        return build (frames);
    }
};
