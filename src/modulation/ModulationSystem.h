#pragma once
#include "../core/SignalBus.h"
#include "../core/VisualState.h"
#include "../graph/ModGraph.h"
#include <array>
#include <atomic>
#include <vector>

struct EnvelopeParams
{
    int triggerMode = 0; // 0=Audio Transient, 1=MIDI Gate, 2=Beat Sync, 3=Bass Threshold, 4=Auto Loop
    float attack = 0.015f;
    float decay = 0.25f;
    float sustain = 0.2f;
    float release = 0.35f;
    float threshold = 0.25f;

    // Runtime state
    int stage = 0; // 0=Idle, 1=Attack, 2=Decay, 3=Sustain, 4=Release
    float value = 0.0f;
    bool prevGate = false;
    float prevBeat = 0.0f;
};

struct LfoParams
{
    int shape = 0;       // 0=Sine, 1=Tri, 2=Saw, 3=Ramp, 4=Square, 5=S&H, 6=SmoothRand, 7=ExpPulse
    float rateHz = 0.5f;
    bool tempoSync = false;
    int syncDiv = 2;     // 0=1/16, 1=1/8, 2=1/4, 3=1/2, 4=1Bar, 5=2Bars, 6=4Bars
    float phaseOffset = 0.0f;
    float depth = 1.0f;
    float smooth = 0.05f;

    // Runtime state
    double phase = 0.0;
    float shTarget = 0.5f, shPrev = 0.5f;
    float value = 0.0f;
};

struct ChaosParams
{
    float speed = 1.0f;
    float sigma = 10.0f;
    float rho = 28.0f;
    float beta = 2.667f;
    float duffingDrive = 0.35f;

    // Lorenz state
    double lx = 0.1, ly = 0.0, lz = 0.0;
    // Rossler state
    double rx = 0.1, ry = 0.0, rz = 0.0;
    // Duffing state
    double dx = 0.1, dv = 0.0, dtAcc = 0.0;

    // Normalized outputs (0..1)
    float outLorenzX = 0.5f, outLorenzY = 0.5f, outLorenzZ = 0.5f;
    float outRossler = 0.5f, outDuffing = 0.5f, outDrift1 = 0.5f, outDrift2 = 0.5f;

    // Trail buffer for live 2D phase-portrait scope in UI
    static constexpr int kTrailLen = 96;
    std::array<float, kTrailLen> trailX {}, trailY {};
    int trailHead = 0;
};

struct SequencerParams
{
    std::array<float, 16> steps { 1.0f, 0.25f, 0.75f, 0.4f, 0.9f, 0.15f, 0.6f, 0.3f,
                                  0.85f, 0.2f, 0.7f, 0.5f, 0.95f, 0.35f, 0.65f, 0.1f };
    int numSteps = 16;
    float rateHz = 4.0f;
    bool tempoSync = true;
    int syncDiv = 1;     // 0=1/16, 1=1/8, 2=1/4, 3=1/2, 4=1Bar
    float glide = 0.15f;
    int direction = 0;   // 0=Forward, 1=Reverse, 2=PingPong, 3=Random
    float swing = 0.0f;

    // Runtime
    double phase = 0.0;
    int currentStep = 0;
    int pingDir = 1;
    float value = 0.0f;
};

struct ModRoute
{
    bool enabled = true;
    juce::String source;      // e.g. "audio.env", "lfo.1", "chaos.lorenzX", "seq.1", "pose.handSpread"
    juce::String via;         // optional scaling signal, "" = none
    juce::String destination; // e.g. "scene.pulse", "post.bloom"
    float amount = 0.5f;      // -2.0 .. +2.0 (fraction of destination range)
    float curve = 1.0f;       // 0.25 .. 4.0
    bool bipolar = false;     // if true, maps 0..1 source to -0.5..+0.5 before scaling
    float lastContribution = 0.0f;

    // Cached fast indices for zero-lookup render-thread evaluation
    int srcIdx = -1;
    int viaIdx = -1;
    int dstIdx = -1;
};

class ModulationSystem
{
public:
    ModulationSystem (SignalBus& bus, VisualState& visual, ModGraph& graph);

    // Called on render thread once per frame
    void tick (float dt);

    // Accessors for UI (protected by lock)
    juce::CriticalSection& getLock() const noexcept { return lock; }

    std::array<EnvelopeParams, 4> envs;
    std::array<LfoParams, 4> lfos;
    ChaosParams chaos;
    std::array<SequencerParams, 2> seqs;
    std::array<std::atomic<float>, 8> macros {};

    // Song position in quarter notes while the host is playing (-1 otherwise); set from the audio thread.
    std::atomic<double> hostPpq { -1.0 };
    std::array<juce::String, 8> macroNames { "Macro 1", "Macro 2", "Macro 3", "Macro 4",
                                             "Macro 5", "Macro 6", "Macro 7", "Macro 8" };

    std::vector<ModRoute> getRoutes() const;
    void setRoutes (const std::vector<ModRoute>& r);
    void addRoute (const ModRoute& r);
    void removeRoute (int index);
    void updateRoute (int index, const ModRoute& r);
    void clearRoutes();

    juce::StringArray getAllSourceNames() const;

    void resetToDefaultRoutes();

    juce::var toVar() const;
    void fromVar (const juce::var& v);

private:
    void resolveRouteIndices (ModRoute& r) const;
    SignalBus& bus;
    VisualState& visual;
    ModGraph& graph;
    mutable juce::CriticalSection lock;

    std::vector<ModRoute> routes;

    // Signal indices registered in SignalBus
    int sSubBass = -1, sBass = -1, sLowMid = -1, sHighMid = -1, sPresence = -1, sBrilliance = -1;
    std::array<int, 16> sBands {};
    int sTransient = -1, sRms = -1, sMidiGate = -1, sBpm = -1, sBeat = -1, sPlaying = -1;
    std::array<int, 4> sEnv {};
    std::array<int, 4> sLfo {};
    int sLorX = -1, sLorY = -1, sLorZ = -1, sRossler = -1, sDuffing = -1, sDrift1 = -1, sDrift2 = -1;
    std::array<int, 2> sSeq {};
    std::array<int, 8> sMacro {};

    // Clocks, noise and percussive-trigger sources
    int sClkBeat = -1, sClk2Beat = -1, sClkBar = -1, sClk2Bar = -1, sClk4Bar = -1, sClkTri = -1, sClkSine = -1, sClkPulse = -1;
    std::array<int, 3> sNoise {};
    int sNoiseStep = -1, sTimeMin = -1, sTimeSlow = -1, sTimeFast = -1;
    std::array<int, 3> sPerc {}; // kick, snare, hat
    int sPeakHold = -1, sPeak = -1;
    double beatClock = 0.0, timeClock = 0.0;
    long long lastBeatIndex = -1;
    float noiseStepValue = 0.5f;
    std::array<float, 3> percAvg {}, percValue {}, percCooldown {};
    float peakHold = 0.0f;

    juce::Random rng;
};
