#pragma once
#include "../core/SignalBus.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

// Realtime-safe audio analysis. Picks a source from the input matrix, optionally filters it,
// then publishes control-rate features (per block) to the SignalBus:
//   audio.rms/peak/env/transient/centroid/flux, audio.mid/side/width, audio.sc1/sc2, band.0..15
class Analyzer
{
public:
    static constexpr int numBands = 16;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = 512;

    enum Source { Mid = 0, Side, Left, Right, Sidechain1, Sidechain2 };
    enum FilterType { FilterOff = 0, LowPass, HighPass, BandPass };

    explicit Analyzer (SignalBus&);

    void prepare (double sampleRate);

    // Null channel pointers mean "bus not available". Right may equal Left for mono.
    void process (const float* mainL, const float* mainR,
                  const float* sc1L, const float* sc1R,
                  const float* sc2L, const float* sc2R, int numSamples);

    // Parameters (written by the UI thread, read by the audio thread)
    std::atomic<int> source { Mid };
    std::atomic<int> filterType { FilterOff };
    std::atomic<float> cutoffHz { 1000.0f };
    std::atomic<float> attackMs { 5.0f };
    std::atomic<float> releaseMs { 150.0f };
    std::atomic<float> transientSensitivity { 1.8f };

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set (int type, double fs, double f, double q);
        float tick (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    static float msToCoef (float ms, double sr) noexcept;
    void runFFT();

    SignalBus& bus;
    double sampleRate = 0.0;

    int sRms, sPeak, sEnv, sTransient, sCentroid, sFlux, sMid, sSide, sWidth, sSc1, sSc2;
    int sLeft, sRight, sCorr, sNoise, sRolloff;
    float prevX = 0, noiseV = 0, rolloffV = 0;
    std::array<int, numBands> sBands {};

    Biquad filt;
    int lastType = -1;
    float lastCut = -1.0f;

    float env = 0, fast = 0, slow = 0, transient = 0;

    juce::dsp::FFT fft;
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize> window {};
    std::array<float, 2 * fftSize> fftData {};
    std::array<float, fftSize / 2> prevNorm {};
    int writePos = 0, sinceHop = 0;
    std::array<int, numBands> bandLo {}, bandHi {};
    std::array<float, numBands> bandTarget {}, bandSmooth {};
    float centroidV = 0, fluxV = 0;
};
