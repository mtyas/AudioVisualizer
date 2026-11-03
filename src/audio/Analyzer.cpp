#include "Analyzer.h"
#include <cmath>

void Analyzer::Biquad::set (int type, double fs, double f, double q)
{
    if (type == FilterOff)
    {
        b0 = 1; b1 = b2 = a1 = a2 = 0;
        return;
    }
    const double w = juce::MathConstants<double>::twoPi * f / fs;
    const double c = std::cos (w), s = std::sin (w);
    const double alpha = s / (2.0 * q);
    const double a0 = 1.0 + alpha;
    double nb0, nb1, nb2;
    switch (type)
    {
        case LowPass:  nb0 = (1 - c) / 2; nb1 = 1 - c;       nb2 = (1 - c) / 2; break;
        case HighPass: nb0 = (1 + c) / 2; nb1 = -(1 + c);    nb2 = (1 + c) / 2; break;
        default:       nb0 = alpha;       nb1 = 0;           nb2 = -alpha;      break;
    }
    b0 = (float) (nb0 / a0);
    b1 = (float) (nb1 / a0);
    b2 = (float) (nb2 / a0);
    a1 = (float) (-2.0 * c / a0);
    a2 = (float) ((1.0 - alpha) / a0);
}

float Analyzer::msToCoef (float ms, double sr) noexcept
{
    return 1.0f - std::exp (-1.0f / (juce::jmax (0.01f, ms) * 0.001f * (float) sr));
}

Analyzer::Analyzer (SignalBus& b) : bus (b), fft (fftOrder)
{
    sRms = b.add ("audio.rms");
    sPeak = b.add ("audio.peak");
    sEnv = b.add ("audio.env");
    sTransient = b.add ("audio.transient");
    sCentroid = b.add ("audio.centroid");
    sFlux = b.add ("audio.flux");
    sMid = b.add ("audio.mid");
    sSide = b.add ("audio.side");
    sWidth = b.add ("audio.width");
    sSc1 = b.add ("audio.sc1");
    sSc2 = b.add ("audio.sc2");
    sLeft = b.add ("audio.left");
    sRight = b.add ("audio.right");
    sCorr = b.add ("audio.correlation");
    sNoise = b.add ("audio.noisiness");
    sRolloff = b.add ("audio.rolloff");
    for (int i = 0; i < numBands; ++i)
        sBands[(size_t) i] = b.add ("band." + juce::String (i));

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
}

void Analyzer::prepare (double sr)
{
    sampleRate = sr;
    env = fast = slow = transient = 0;
    lastType = -1;
    fifo.fill (0.0f);
    prevNorm.fill (0.0f);
    bandTarget.fill (0.0f);
    bandSmooth.fill (0.0f);
    writePos = sinceHop = 0;

    const double fmin = 30.0, fmax = juce::jmin (16000.0, sr * 0.45);
    for (int i = 0; i < numBands; ++i)
    {
        const double f0 = fmin * std::pow (fmax / fmin, (double) i / numBands);
        const double f1 = fmin * std::pow (fmax / fmin, (double) (i + 1) / numBands);
        int lo = (int) std::floor (f0 * fftSize / sr);
        int hi = (int) std::ceil (f1 * fftSize / sr);
        lo = juce::jlimit (1, fftSize / 2 - 2, lo);
        hi = juce::jlimit (lo + 1, fftSize / 2 - 1, hi);
        bandLo[(size_t) i] = lo;
        bandHi[(size_t) i] = hi;
    }
}

void Analyzer::runFFT()
{
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = fifo[(size_t) ((writePos + i) & (fftSize - 1))] * window[(size_t) i];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float norm = 4.0f / (float) fftSize; // full-scale sine with Hann window -> ~1
    const int bins = fftSize / 2;
    double sumMag = 0.0, sumFMag = 0.0, fluxSum = 0.0, sumEnergy = 0.0;
    std::array<float, fftSize / 2> nm;

    for (int k = 1; k < bins; ++k)
    {
        const float mag = fftData[(size_t) k] * norm;
        const float db = 20.0f * std::log10 (mag + 1.0e-6f);
        const float n = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 80.0f);
        nm[(size_t) k] = n;
        fluxSum += juce::jmax (0.0f, n - prevNorm[(size_t) k]);
        prevNorm[(size_t) k] = n;
        sumMag += mag;
        sumEnergy += (double) mag * mag;
        sumFMag += mag * ((double) k * sampleRate / fftSize);
    }

    for (int b = 0; b < numBands; ++b)
    {
        float acc = 0.0f;
        const int lo = bandLo[(size_t) b], hi = bandHi[(size_t) b];
        for (int k = lo; k < hi; ++k)
            acc += nm[(size_t) k];
        bandTarget[(size_t) b] = acc / (float) (hi - lo);
    }

    if (sumMag > 1.0e-5)
    {
        const double c = sumFMag / sumMag;
        centroidV = juce::jlimit (0.0f, 1.0f, (float) (std::log2 (juce::jmax (c, 30.0) / 30.0) / std::log2 (16000.0 / 30.0)));
    }
    else
        centroidV = 0.0f;

    fluxV = juce::jlimit (0.0f, 1.0f, (float) (fluxSum / bins) * 6.0f);

    if (sumEnergy > 1.0e-10)
    {
        double acc = 0.0;
        int kRoll = bins - 1;
        for (int k = 1; k < bins; ++k)
        {
            const double mag = (double) fftData[(size_t) k] * norm;
            acc += mag * mag;
            if (acc >= 0.85 * sumEnergy) { kRoll = k; break; }
        }
        const double f = (double) kRoll * sampleRate / fftSize;
        rolloffV = juce::jlimit (0.0f, 1.0f, (float) (std::log2 (juce::jmax (f, 30.0) / 30.0) / std::log2 (16000.0 / 30.0)));
    }
    else
        rolloffV = 0.0f;
}

void Analyzer::process (const float* mainL, const float* mainR,
                        const float* sc1L, const float* sc1R,
                        const float* sc2L, const float* sc2R, int n)
{
    if (n <= 0 || sampleRate <= 0.0)
        return;

    const int src = source.load (std::memory_order_relaxed);
    const int ftype = filterType.load (std::memory_order_relaxed);
    const float cut = juce::jlimit (20.0f, (float) (sampleRate * 0.45), cutoffHz.load (std::memory_order_relaxed));
    if (ftype != lastType || cut != lastCut)
    {
        filt.set (ftype, sampleRate, cut, 0.707);
        lastType = ftype;
        lastCut = cut;
    }

    const float aMs = attackMs.load (std::memory_order_relaxed);
    const float rMs = releaseMs.load (std::memory_order_relaxed);
    const float sens = juce::jmax (1.05f, transientSensitivity.load (std::memory_order_relaxed));
    const float atk = msToCoef (aMs, sampleRate), rel = msToCoef (rMs, sampleRate);
    const float fastA = msToCoef (1.0f, sampleRate), fastR = msToCoef (15.0f, sampleRate);
    const float slowA = msToCoef (40.0f, sampleRate), slowR = msToCoef (150.0f, sampleRate);
    const float tDecay = std::exp (-1.0f / (0.08f * (float) sampleRate));

    double sumA = 0, sumM = 0, sumS = 0, sumC1 = 0, sumC2 = 0, sumL = 0, sumR = 0, sumLR = 0;
    int zeroCross = 0;
    float peak = 0, blockTrans = 0;

    for (int i = 0; i < n; ++i)
    {
        const float l = mainL != nullptr ? mainL[i] : 0.0f;
        const float r = mainR != nullptr ? mainR[i] : l;
        const float m = 0.5f * (l + r), s = 0.5f * (l - r);
        const float c1 = sc1L != nullptr ? 0.5f * (sc1L[i] + (sc1R != nullptr ? sc1R[i] : sc1L[i])) : 0.0f;
        const float c2 = sc2L != nullptr ? 0.5f * (sc2L[i] + (sc2R != nullptr ? sc2R[i] : sc2L[i])) : 0.0f;

        float x;
        switch (src)
        {
            case Mid:        x = m; break;
            case Side:       x = s; break;
            case Left:       x = l; break;
            case Right:      x = r; break;
            case Sidechain1: x = c1; break;
            default:         x = c2; break;
        }
        x = filt.tick (x);
        if ((x > 0.0f) != (prevX > 0.0f)) ++zeroCross;
        prevX = x;
        sumL += (double) l * l;
        sumR += (double) r * r;
        sumLR += (double) l * r;

        sumA += (double) x * x;
        sumM += (double) m * m;
        sumS += (double) s * s;
        sumC1 += (double) c1 * c1;
        sumC2 += (double) c2 * c2;

        const float ax = std::abs (x);
        peak = juce::jmax (peak, ax);
        env += (ax > env ? atk : rel) * (ax - env);
        fast += (ax > fast ? fastA : fastR) * (ax - fast);
        slow += (ax > slow ? slowA : slowR) * (ax - slow);

        float strength = 0.0f;
        if (fast > 0.005f && fast > slow * sens)
            strength = 1.0f - (slow * sens) / fast;
        transient = juce::jmax (transient * tDecay, strength);
        blockTrans = juce::jmax (blockTrans, transient);

        fifo[(size_t) writePos] = x;
        writePos = (writePos + 1) & (fftSize - 1);
        if (++sinceHop >= hopSize)
        {
            sinceHop = 0;
            runFFT();
        }
    }

    const float inv = 1.0f / (float) n;
    const float rmsMid = std::sqrt ((float) sumM * inv);
    const float rmsSide = std::sqrt ((float) sumS * inv);
    bus.set (sRms, std::sqrt ((float) sumA * inv));
    bus.set (sPeak, peak);
    bus.set (sEnv, env);
    bus.set (sTransient, blockTrans);
    bus.set (sCentroid, centroidV);
    bus.set (sFlux, fluxV);
    bus.set (sMid, rmsMid);
    bus.set (sSide, rmsSide);
    bus.set (sWidth, rmsSide / (rmsMid + rmsSide + 1.0e-6f));
    bus.set (sSc1, std::sqrt ((float) sumC1 * inv));
    bus.set (sSc2, std::sqrt ((float) sumC2 * inv));
    bus.set (sLeft, std::sqrt ((float) sumL * inv));
    bus.set (sRight, std::sqrt ((float) sumR * inv));
    bus.set (sCorr, sumL > 1.0e-9 && sumR > 1.0e-9
                        ? 0.5f + 0.5f * (float) (sumLR / std::sqrt (sumL * sumR)) : 0.5f);
    noiseV += ((float) juce::jlimit (0.0, 1.0, (double) zeroCross / (double) n * 4.0) - noiseV) * 0.3f;
    bus.set (sNoise, noiseV);
    bus.set (sRolloff, rolloffV);

    const float relDecay = std::exp (-(float) n / ((float) sampleRate * juce::jmax (0.01f, rMs * 0.001f)));
    for (int b = 0; b < numBands; ++b)
    {
        auto& s = bandSmooth[(size_t) b];
        s = juce::jmax (bandTarget[(size_t) b], s * relDecay);
        bus.set (sBands[(size_t) b], s);
    }
}
