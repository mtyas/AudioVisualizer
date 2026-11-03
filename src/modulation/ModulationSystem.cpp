#include "ModulationSystem.h"
#include <cmath>
#include <cstdint>

namespace
{
float hashNoise (int n)
{
    auto x = (uint32_t) n;
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return (float) x / 4294967295.0f;
}

// Smooth 1D value noise, 0..1
float smoothNoise (double t)
{
    const int i = (int) std::floor (t);
    const float f = (float) (t - (double) i), s = f * f * (3.0f - 2.0f * f);
    return hashNoise (i) + (hashNoise (i + 1) - hashNoise (i)) * s;
}

float syncDivToHz (int div, float bpm)
{
    const float beatsPerSec = juce::jmax (20.0f, bpm > 1.0f ? bpm : 120.0f) / 60.0f;
    // 0=1/16 (4 per beat), 1=1/8 (2 per beat), 2=1/4 (1 per beat), 3=1/2, 4=1Bar, 5=2Bars, 6=4Bars
    static const float mults[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.0625f };
    return beatsPerSec * mults[juce::jlimit (0, 6, div)];
}
} // namespace

ModulationSystem::ModulationSystem (SignalBus& b, VisualState& v, ModGraph& g)
    : bus (b), visual (v), graph (g)
{
    sSubBass    = bus.add ("audio.subBass");
    sBass       = bus.add ("audio.bass");
    sLowMid     = bus.add ("audio.lowMid");
    sHighMid    = bus.add ("audio.highMid");
    sPresence   = bus.add ("audio.presence");
    sBrilliance = bus.add ("audio.brilliance");

    for (int i = 0; i < 4; ++i)
        sEnv[(size_t) i] = bus.add ("env." + juce::String (i + 1));
    for (int i = 0; i < 4; ++i)
        sLfo[(size_t) i] = bus.add ("lfo." + juce::String (i + 1));

    sLorX    = bus.add ("chaos.lorenzX");
    sLorY    = bus.add ("chaos.lorenzY");
    sLorZ    = bus.add ("chaos.lorenzZ");
    sRossler = bus.add ("chaos.rossler");
    sDuffing = bus.add ("chaos.duffing");
    sDrift1  = bus.add ("chaos.drift1");
    sDrift2  = bus.add ("chaos.drift2");

    for (int i = 0; i < 2; ++i)
        sSeq[(size_t) i] = bus.add ("seq." + juce::String (i + 1));
    for (int i = 0; i < 8; ++i)
    {
        sMacro[(size_t) i] = bus.add ("macro." + juce::String (i + 1));
        macros[(size_t) i].store (0.5f, std::memory_order_relaxed);
    }

    sClkBeat  = bus.add ("clock.beat");
    sClk2Beat = bus.add ("clock.2beat");
    sClkBar   = bus.add ("clock.bar");
    sClk2Bar  = bus.add ("clock.2bar");
    sClk4Bar  = bus.add ("clock.4bar");
    sClkTri   = bus.add ("clock.triBeat");
    sClkSine  = bus.add ("clock.sineBar");
    sClkPulse = bus.add ("clock.pulse");
    sNoise[0] = bus.add ("noise.slow");
    sNoise[1] = bus.add ("noise.medium");
    sNoise[2] = bus.add ("noise.fast");
    sNoiseStep = bus.add ("noise.perBeat");
    sTimeMin  = bus.add ("time.minute");
    sTimeSlow = bus.add ("time.slowSine");
    sTimeFast = bus.add ("time.fastSine");
    sPerc[0]  = bus.add ("audio.kick");
    sPerc[1]  = bus.add ("audio.snare");
    sPerc[2]  = bus.add ("audio.hat");
    sPeakHold = bus.add ("audio.peakHold");
    sPeak = bus.find ("audio.peak");

    for (int i = 0; i < 16; ++i)
        sBands[(size_t) i] = bus.find ("band." + juce::String (i));
    sTransient = bus.find ("audio.transient");
    sRms       = bus.find ("audio.rms");
    sMidiGate  = bus.find ("midi.gate");
    sBpm       = bus.find ("host.bpm");
    sBeat      = bus.find ("host.beat");
    sPlaying   = bus.find ("host.playing");

    // Diversify default LFOs & Envelopes
    lfos[0].rateHz = 0.25f; lfos[0].shape = 0;
    lfos[1].rateHz = 0.8f;  lfos[1].shape = 1;
    lfos[2].rateHz = 2.0f;  lfos[2].shape = 5; // S&H
    lfos[3].rateHz = 0.12f; lfos[3].shape = 6; // Smooth random

    envs[0].triggerMode = 0; envs[0].attack = 0.008f; envs[0].decay = 0.18f; envs[0].sustain = 0.0f;
    envs[1].triggerMode = 3; envs[1].attack = 0.015f; envs[1].decay = 0.30f; envs[1].sustain = 0.15f;
    envs[2].triggerMode = 1; envs[2].attack = 0.02f;  envs[2].decay = 0.40f; envs[2].sustain = 0.5f;
    envs[3].triggerMode = 2; envs[3].attack = 0.005f; envs[3].decay = 0.22f; envs[3].sustain = 0.0f;

    seqs[1].numSteps = 8;
    seqs[1].direction = 2; // Ping-pong
    seqs[1].steps = { 0.1f, 0.3f, 0.6f, 1.0f, 0.8f, 0.5f, 0.2f, 0.0f,
                      0.2f, 0.4f, 0.7f, 0.9f, 0.6f, 0.3f, 0.1f, 0.5f };

    resetToDefaultRoutes();
}

void ModulationSystem::resolveRouteIndices (ModRoute& r) const
{
    r.srcIdx = r.source.isNotEmpty() ? bus.find (r.source) : -1;
    r.viaIdx = r.via.isNotEmpty() ? bus.find (r.via) : -1;
    r.dstIdx = r.destination.isNotEmpty() ? visual.findParam (r.destination) : -1;
}

void ModulationSystem::resetToDefaultRoutes()
{
    const juce::ScopedLock sl (lock);
    routes.clear();
    addRoute ({ true, "audio.env",       "", "scene.pulse",       0.45f, 1.0f, false, 0.0f });
    addRoute ({ true, "audio.transient", "", "post.bloom",        0.40f, 0.8f, false, 0.0f });
    addRoute ({ true, "audio.centroid",  "", "scene.hue",         0.35f, 1.0f, true,  0.0f });
    addRoute ({ true, "lfo.1",           "", "layer2d.colorShift",0.50f, 1.0f, false, 0.0f });
    addRoute ({ true, "chaos.lorenzX",   "", "scene.camOrbit",    0.30f, 1.0f, true,  0.0f });
    addRoute ({ true, "audio.bass",      "", "post.feedbackZoom", 0.25f, 1.2f, false, 0.0f });
}

void ModulationSystem::tick (float dt)
{
    dt = juce::jlimit (0.0005f, 0.1f, dt);

    // 1. Compute grouped audio bands
    auto avgBands = [&] (int lo, int hi)
    {
        float s = 0.0f;
        for (int i = lo; i <= hi; ++i)
            s += bus.get (sBands[(size_t) i]);
        return s / (float) (hi - lo + 1);
    };
    const float subBass = avgBands (0, 1);
    const float bass    = avgBands (2, 4);
    bus.set (sSubBass,    subBass);
    bus.set (sBass,       bass);
    bus.set (sLowMid,     avgBands (5, 7));
    bus.set (sHighMid,    avgBands (8, 10));
    bus.set (sPresence,   avgBands (11, 13));
    bus.set (sBrilliance, avgBands (14, 15));

    // Publish macros
    for (int i = 0; i < 8; ++i)
        bus.set (sMacro[(size_t) i], macros[(size_t) i].load (std::memory_order_relaxed));

    const float bpm = bus.get (sBpm);
    const float beat = bus.get (sBeat);
    const float trans = bus.get (sTransient);
    const float midiGate = bus.get (sMidiGate);

    const juce::ScopedLock sl (lock);

    // ---- Tempo clocks (follow the host's song position while playing, otherwise free-run at the host BPM) ----
    {
        const double ppq = hostPpq.load (std::memory_order_relaxed);
        if (ppq >= 0.0)
            beatClock = ppq;
        else
            beatClock += (double) dt * (double) (bpm > 1.0f ? bpm : 120.0f) / 60.0;

        auto saw = [this] (double beats) { return (float) std::fmod (beatClock / beats, 1.0); };
        const float beatSaw = saw (1.0), barSaw = saw (4.0);
        bus.set (sClkBeat, beatSaw);
        bus.set (sClk2Beat, saw (2.0));
        bus.set (sClkBar, barSaw);
        bus.set (sClk2Bar, saw (8.0));
        bus.set (sClk4Bar, saw (16.0));
        bus.set (sClkTri, 1.0f - std::abs (2.0f * beatSaw - 1.0f));
        bus.set (sClkSine, 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * barSaw));
        bus.set (sClkPulse, std::exp (-beatSaw * 6.0f));

        // Random value that changes once per beat
        const long long beatIdx = (long long) std::floor (beatClock);
        if (beatIdx != lastBeatIndex)
        {
            lastBeatIndex = beatIdx;
            noiseStepValue = rng.nextFloat();
        }
        bus.set (sNoiseStep, noiseStepValue);

        // Free-running time-based sources and smooth noise
        timeClock += (double) dt;
        bus.set (sNoise[0], smoothNoise (timeClock * 0.3));
        bus.set (sNoise[1], smoothNoise (timeClock * 1.1 + 37.0));
        bus.set (sNoise[2], smoothNoise (timeClock * 3.7 + 91.0));
        bus.set (sTimeMin, (float) std::fmod (timeClock / 60.0, 1.0));
        bus.set (sTimeSlow, 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * timeClock / 20.0));
        bus.set (sTimeFast, 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * timeClock / 2.0));
    }

    // ---- Percussion triggers derived from the spectrum: kick (low), snare (mids), hat (highs) ----
    {
        const float level[3] = { subBass * 0.5f + bass * 0.5f, avgBands (5, 8), avgBands (12, 15) };
        for (int k = 0; k < 3; ++k)
        {
            auto& avg = percAvg[(size_t) k];
            auto& val = percValue[(size_t) k];
            auto& cool = percCooldown[(size_t) k];
            cool = juce::jmax (0.0f, cool - dt);
            if (level[k] > avg * 1.45f + 0.04f && cool <= 0.0f)
            {
                val = 1.0f;
                cool = 0.09f;
            }
            avg += (level[k] - avg) * juce::jmin (1.0f, dt * 6.0f);
            val *= std::exp (-dt * (k == 0 ? 7.0f : 12.0f));
            bus.set (sPerc[(size_t) k], val);
        }
        const float peak = bus.get (sPeak);
        peakHold = juce::jmax (peak, peakHold - dt * 0.35f);
        bus.set (sPeakHold, peakHold);
    }

    // 2. Tick 4 Envelopes
    for (int i = 0; i < 4; ++i)
    {
        auto& e = envs[(size_t) i];
        bool gate = false;
        switch (e.triggerMode)
        {
            case 0: gate = (trans > e.threshold); break;
            case 1: gate = (midiGate > 0.5f); break;
            case 2: gate = (beat < 0.15f); break;
            case 3: gate = (bass > e.threshold); break;
            default: gate = (e.stage == 0 || e.stage == 1 || e.stage == 2); break;
        }

        if (gate && ! e.prevGate)
            e.stage = 1; // Attack
        else if (! gate && e.prevGate && e.stage != 0)
            e.stage = 4; // Release
        e.prevGate = gate;

        switch (e.stage)
        {
            case 1: // Attack
                e.value += dt / juce::jmax (0.001f, e.attack);
                if (e.value >= 1.0f) { e.value = 1.0f; e.stage = 2; }
                break;
            case 2: // Decay
                e.value -= (1.0f - e.sustain) * (dt / juce::jmax (0.005f, e.decay));
                if (e.value <= e.sustain)
                {
                    e.value = e.sustain;
                    e.stage = (e.triggerMode == 1) ? 3 : 4;
                }
                break;
            case 3: // Sustain
                e.value = e.sustain;
                break;
            case 4: // Release
                e.value -= dt / juce::jmax (0.005f, e.release);
                if (e.value <= 0.0f) { e.value = 0.0f; e.stage = (e.triggerMode == 4) ? 1 : 0; }
                break;
            default:
                e.value = 0.0f;
                if (e.triggerMode == 4) e.stage = 1;
                break;
        }
        e.value = juce::jlimit (0.0f, 1.0f, e.value);
        bus.set (sEnv[(size_t) i], e.value);
    }

    // 3. Tick 4 LFOs
    for (int i = 0; i < 4; ++i)
    {
        auto& l = lfos[(size_t) i];
        const float freq = l.tempoSync ? syncDivToHz (l.syncDiv, bpm) : l.rateHz;
        const double prevP = l.phase;
        l.phase = std::fmod (l.phase + (double) freq * dt, 1.0);
        if (l.phase < prevP) // wrapped
        {
            l.shPrev = l.shTarget;
            l.shTarget = rng.nextFloat();
        }
        const float p = std::fmod ((float) l.phase + l.phaseOffset, 1.0f);
        float raw = 0.5f;
        switch (l.shape)
        {
            case 0: raw = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * p); break;
            case 1: raw = 1.0f - std::abs (2.0f * p - 1.0f); break;
            case 2: raw = 1.0f - p; break;
            case 3: raw = p; break;
            case 4: raw = p < 0.5f ? 1.0f : 0.0f; break;
            case 5: raw = l.shTarget; break;
            case 6:
            {
                const float s = p * p * (3.0f - 2.0f * p);
                raw = l.shPrev + (l.shTarget - l.shPrev) * s;
                break;
            }
            default: raw = std::pow (1.0f - p, 4.0f); break;
        }
        raw *= l.depth;
        const float sm = juce::jlimit (0.001f, 1.0f, dt / juce::jmax (0.001f, l.smooth));
        l.value += (raw - l.value) * sm;
        bus.set (sLfo[(size_t) i], juce::jlimit (0.0f, 1.0f, l.value));
    }

    // 4. Tick Chaos Attractors (Lorenz, Rossler, Duffing, Smooth Drift)
    {
        const double h = (double) dt * 0.65 * juce::jlimit (0.05f, 5.0f, chaos.speed);
        const int steps = 4;
        const double hs = h / steps;
        for (int k = 0; k < steps; ++k)
        {
            // Lorenz
            const double dlx = chaos.sigma * (chaos.ly - chaos.lx);
            const double dly = chaos.lx * (chaos.rho - chaos.lz) - chaos.ly;
            const double dlz = chaos.lx * chaos.ly - chaos.beta * chaos.lz;
            chaos.lx = juce::jlimit (-40.0, 40.0, chaos.lx + dlx * hs);
            chaos.ly = juce::jlimit (-40.0, 40.0, chaos.ly + dly * hs);
            chaos.lz = juce::jlimit (0.0,   60.0, chaos.lz + dlz * hs);

            // Rossler (a=0.2, b=0.2, c=5.7)
            const double drx = -chaos.ry - chaos.rz;
            const double dry = chaos.rx + 0.2 * chaos.ry;
            const double drz = 0.2 + chaos.rz * (chaos.rx - 5.7);
            chaos.rx = juce::jlimit (-20.0, 20.0, chaos.rx + drx * hs);
            chaos.ry = juce::jlimit (-20.0, 20.0, chaos.ry + dry * hs);
            chaos.rz = juce::jlimit (0.0,   35.0, chaos.rz + drz * hs);

            // Duffing oscillator
            chaos.dtAcc += hs * 2.5;
            const double ddx = chaos.dv;
            const double ddv = chaos.dx - chaos.dx * chaos.dx * chaos.dx - 0.15 * chaos.dv
                               + chaos.duffingDrive * std::cos (1.2 * chaos.dtAcc);
            chaos.dx = juce::jlimit (-2.5, 2.5, chaos.dx + ddx * hs * 2.5);
            chaos.dv = juce::jlimit (-2.5, 2.5, chaos.dv + ddv * hs * 2.5);
        }

        chaos.outLorenzX = juce::jlimit (0.0f, 1.0f, (float) ((chaos.lx + 22.0) / 44.0));
        chaos.outLorenzY = juce::jlimit (0.0f, 1.0f, (float) ((chaos.ly + 25.0) / 50.0));
        chaos.outLorenzZ = juce::jlimit (0.0f, 1.0f, (float) (chaos.lz / 50.0));
        chaos.outRossler = juce::jlimit (0.0f, 1.0f, (float) ((chaos.rx + 12.0) / 24.0));
        chaos.outDuffing = juce::jlimit (0.0f, 1.0f, (float) ((chaos.dx + 1.6) / 3.2));
        chaos.outDrift1  = 0.5f + 0.5f * (float) std::sin (chaos.dtAcc * 0.37 + std::cos (chaos.dtAcc * 0.23));
        chaos.outDrift2  = 0.5f + 0.5f * (float) std::cos (chaos.dtAcc * 0.51 - std::sin (chaos.dtAcc * 0.19));

        chaos.trailX[(size_t) chaos.trailHead] = chaos.outLorenzX;
        chaos.trailY[(size_t) chaos.trailHead] = chaos.outLorenzZ;
        chaos.trailHead = (chaos.trailHead + 1) % ChaosParams::kTrailLen;

        bus.set (sLorX,    chaos.outLorenzX);
        bus.set (sLorY,    chaos.outLorenzY);
        bus.set (sLorZ,    chaos.outLorenzZ);
        bus.set (sRossler, chaos.outRossler);
        bus.set (sDuffing, chaos.outDuffing);
        bus.set (sDrift1,  chaos.outDrift1);
        bus.set (sDrift2,  chaos.outDrift2);
    }

    // 5. Tick 2 Step Sequencers
    for (int i = 0; i < 2; ++i)
    {
        auto& sq = seqs[(size_t) i];
        const int nSteps = juce::jlimit (1, 16, sq.numSteps);
        const float rate = sq.tempoSync ? syncDivToHz (sq.syncDiv, bpm) : sq.rateHz;
        const double prev = sq.phase;
        sq.phase = std::fmod (sq.phase + (double) rate * dt, 1.0);
        if (sq.phase < prev) // step advance
        {
            switch (sq.direction)
            {
                case 0: sq.currentStep = (sq.currentStep + 1) % nSteps; break;
                case 1: sq.currentStep = (sq.currentStep - 1 + nSteps) % nSteps; break;
                case 2:
                    sq.currentStep += sq.pingDir;
                    if (sq.currentStep >= nSteps - 1) { sq.currentStep = nSteps - 1; sq.pingDir = -1; }
                    if (sq.currentStep <= 0)          { sq.currentStep = 0;          sq.pingDir = 1; }
                    break;
                default: sq.currentStep = rng.nextInt (nSteps); break;
            }
        }
        const float target = sq.steps[(size_t) juce::jlimit (0, 15, sq.currentStep)];
        if (sq.glide <= 0.005f)
            sq.value = target;
        else
            sq.value += (target - sq.value) * juce::jlimit (0.01f, 1.0f, dt / (sq.glide * 0.25f));
        bus.set (sSeq[(size_t) i], juce::jlimit (0.0f, 1.0f, sq.value));
    }

    // 6. Evaluate Node Graph
    graph.evaluate (dt);

    // 7. Compute Final Modulated Parameter Values = Base + ModMatrix + GraphSinks (zero heap alloc!)
    const int numParams = visual.getNumParams();
    std::array<float, VisualState::maxParams> accum {};
    for (int i = 0; i < numParams; ++i)
        accum[(size_t) i] = visual.getBase (i);

    for (auto& r : routes)
    {
        if (! r.enabled || r.srcIdx < 0 || r.dstIdx < 0)
        {
            r.lastContribution = 0.0f;
            continue;
        }

        float s = juce::jlimit (0.0f, 1.0f, bus.get (r.srcIdx));
        if (std::abs (r.curve - 1.0f) > 0.01f)
            s = std::pow (s, juce::jlimit (0.2f, 4.0f, r.curve));
        if (r.bipolar)
            s = (s - 0.5f) * 2.0f;
        if (r.viaIdx >= 0)
            s *= juce::jlimit (0.0f, 1.0f, bus.get (r.viaIdx));

        const auto& info = visual.getParamInfo (r.dstIdx);
        const float span = info.maxVal - info.minVal;
        const float delta = s * r.amount * span;
        r.lastContribution = s * r.amount;
        accum[(size_t) r.dstIdx] += delta;
    }

    // Add node-graph sink contributions as offsets relative to default (or direct if named)
    graph.forEachSink ([&] (const juce::String& name, float value, int& dstIdx)
    {
        if (dstIdx == -2)
            dstIdx = visual.findParam (name);
        if (dstIdx >= 0)
            accum[(size_t) dstIdx] += value * 0.25f;
    });

    for (int i = 0; i < numParams; ++i)
        visual.setModulated (i, accum[(size_t) i]);
}

std::vector<ModRoute> ModulationSystem::getRoutes() const
{
    const juce::ScopedLock sl (lock);
    return routes;
}

void ModulationSystem::setRoutes (const std::vector<ModRoute>& r)
{
    const juce::ScopedLock sl (lock);
    routes = r;
    for (auto& rt : routes)
        resolveRouteIndices (rt);
}

void ModulationSystem::addRoute (const ModRoute& r)
{
    const juce::ScopedLock sl (lock);
    ModRoute copy = r;
    resolveRouteIndices (copy);
    routes.push_back (std::move (copy));
}

void ModulationSystem::removeRoute (int index)
{
    const juce::ScopedLock sl (lock);
    if (index >= 0 && index < (int) routes.size())
        routes.erase (routes.begin() + index);
}

void ModulationSystem::updateRoute (int index, const ModRoute& r)
{
    const juce::ScopedLock sl (lock);
    if (index >= 0 && index < (int) routes.size())
    {
        ModRoute copy = r;
        resolveRouteIndices (copy);
        routes[(size_t) index] = std::move (copy);
    }
}

void ModulationSystem::clearRoutes()
{
    const juce::ScopedLock sl (lock);
    routes.clear();
}

juce::StringArray ModulationSystem::getAllSourceNames() const
{
    juce::StringArray list;
    const int n = bus.size();
    for (int i = 0; i < n; ++i)
    {
        const auto& name = bus.getName (i);
        // Include primary sources; skip all 128 individual notes/cc unless needed to keep dropdown clean
        if (! name.startsWith ("note.") && ! name.startsWith ("cc."))
            list.add (name);
    }
    // Add a few useful MIDI CCs & notes
    list.add ("cc.1");
    list.add ("cc.74");
    return list;
}

juce::var ModulationSystem::toVar() const
{
    const juce::ScopedLock sl (lock);
    auto* root = new juce::DynamicObject();

    // Envelopes
    juce::Array<juce::var> envArr;
    for (const auto& e : envs)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("triggerMode", e.triggerMode);
        o->setProperty ("attack", e.attack);
        o->setProperty ("decay", e.decay);
        o->setProperty ("sustain", e.sustain);
        o->setProperty ("release", e.release);
        o->setProperty ("threshold", e.threshold);
        envArr.add (juce::var (o));
    }
    root->setProperty ("envs", envArr);

    // LFOs
    juce::Array<juce::var> lfoArr;
    for (const auto& l : lfos)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("shape", l.shape);
        o->setProperty ("rateHz", l.rateHz);
        o->setProperty ("tempoSync", l.tempoSync);
        o->setProperty ("syncDiv", l.syncDiv);
        o->setProperty ("phaseOffset", l.phaseOffset);
        o->setProperty ("depth", l.depth);
        o->setProperty ("smooth", l.smooth);
        lfoArr.add (juce::var (o));
    }
    root->setProperty ("lfos", lfoArr);

    // Chaos
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("speed", chaos.speed);
        o->setProperty ("sigma", chaos.sigma);
        o->setProperty ("rho", chaos.rho);
        o->setProperty ("duffingDrive", chaos.duffingDrive);
        root->setProperty ("chaos", juce::var (o));
    }

    // Sequencers
    juce::Array<juce::var> seqArr;
    for (const auto& sq : seqs)
    {
        auto* o = new juce::DynamicObject();
        juce::Array<juce::var> st;
        for (float v : sq.steps) st.add (v);
        o->setProperty ("steps", st);
        o->setProperty ("numSteps", sq.numSteps);
        o->setProperty ("rateHz", sq.rateHz);
        o->setProperty ("tempoSync", sq.tempoSync);
        o->setProperty ("syncDiv", sq.syncDiv);
        o->setProperty ("glide", sq.glide);
        o->setProperty ("direction", sq.direction);
        seqArr.add (juce::var (o));
    }
    root->setProperty ("seqs", seqArr);

    // Macros
    juce::Array<juce::var> macArr;
    for (int i = 0; i < 8; ++i)
        macArr.add (macros[(size_t) i].load (std::memory_order_relaxed));
    root->setProperty ("macros", macArr);

    // Routes
    juce::Array<juce::var> rtArr;
    for (const auto& r : routes)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("enabled", r.enabled);
        o->setProperty ("source", r.source);
        o->setProperty ("via", r.via);
        o->setProperty ("destination", r.destination);
        o->setProperty ("amount", r.amount);
        o->setProperty ("curve", r.curve);
        o->setProperty ("bipolar", r.bipolar);
        rtArr.add (juce::var (o));
    }
    root->setProperty ("routes", rtArr);

    return juce::var (root);
}

void ModulationSystem::fromVar (const juce::var& v)
{
    if (! v.isObject())
        return;
    const juce::ScopedLock sl (lock);

    if (auto* envArr = v.getProperty ("envs", juce::var()).getArray())
    {
        for (int i = 0; i < 4 && i < envArr->size(); ++i)
        {
            const auto& o = (*envArr)[i];
            auto& e = envs[(size_t) i];
            e.triggerMode = (int) o.getProperty ("triggerMode", e.triggerMode);
            e.attack      = (float) (double) o.getProperty ("attack", e.attack);
            e.decay       = (float) (double) o.getProperty ("decay", e.decay);
            e.sustain     = (float) (double) o.getProperty ("sustain", e.sustain);
            e.release     = (float) (double) o.getProperty ("release", e.release);
            e.threshold   = (float) (double) o.getProperty ("threshold", e.threshold);
        }
    }

    if (auto* lfoArr = v.getProperty ("lfos", juce::var()).getArray())
    {
        for (int i = 0; i < 4 && i < lfoArr->size(); ++i)
        {
            const auto& o = (*lfoArr)[i];
            auto& l = lfos[(size_t) i];
            l.shape       = (int) o.getProperty ("shape", l.shape);
            l.rateHz      = (float) (double) o.getProperty ("rateHz", l.rateHz);
            l.tempoSync   = (bool) o.getProperty ("tempoSync", l.tempoSync);
            l.syncDiv     = (int) o.getProperty ("syncDiv", l.syncDiv);
            l.phaseOffset = (float) (double) o.getProperty ("phaseOffset", l.phaseOffset);
            l.depth       = (float) (double) o.getProperty ("depth", l.depth);
            l.smooth      = (float) (double) o.getProperty ("smooth", l.smooth);
        }
    }

    const auto ch = v.getProperty ("chaos", juce::var());
    if (ch.isObject())
    {
        chaos.speed        = (float) (double) ch.getProperty ("speed", chaos.speed);
        chaos.sigma        = (float) (double) ch.getProperty ("sigma", chaos.sigma);
        chaos.rho          = (float) (double) ch.getProperty ("rho", chaos.rho);
        chaos.duffingDrive = (float) (double) ch.getProperty ("duffingDrive", chaos.duffingDrive);
    }

    if (auto* seqArr = v.getProperty ("seqs", juce::var()).getArray())
    {
        for (int i = 0; i < 2 && i < seqArr->size(); ++i)
        {
            const auto& o = (*seqArr)[i];
            auto& sq = seqs[(size_t) i];
            if (auto* st = o.getProperty ("steps", juce::var()).getArray())
                for (int k = 0; k < 16 && k < st->size(); ++k)
                    sq.steps[(size_t) k] = (float) (double) (*st)[k];
            sq.numSteps  = (int) o.getProperty ("numSteps", sq.numSteps);
            sq.rateHz    = (float) (double) o.getProperty ("rateHz", sq.rateHz);
            sq.tempoSync = (bool) o.getProperty ("tempoSync", sq.tempoSync);
            sq.syncDiv   = (int) o.getProperty ("syncDiv", sq.syncDiv);
            sq.glide     = (float) (double) o.getProperty ("glide", sq.glide);
            sq.direction = (int) o.getProperty ("direction", sq.direction);
        }
    }

    if (auto* macArr = v.getProperty ("macros", juce::var()).getArray())
    {
        for (int i = 0; i < 8 && i < macArr->size(); ++i)
            macros[(size_t) i].store ((float) (double) (*macArr)[i], std::memory_order_relaxed);
    }

    if (auto* rtArr = v.getProperty ("routes", juce::var()).getArray())
    {
        routes.clear();
        for (const auto& o : *rtArr)
        {
            ModRoute r;
            r.enabled     = (bool) o.getProperty ("enabled", true);
            r.source      = o.getProperty ("source", "").toString();
            r.via         = o.getProperty ("via", "").toString();
            r.destination = o.getProperty ("destination", "").toString();
            r.amount      = (float) (double) o.getProperty ("amount", 0.5);
            r.curve       = (float) (double) o.getProperty ("curve", 1.0);
            r.bipolar     = (bool) o.getProperty ("bipolar", false);
            resolveRouteIndices (r);
            routes.push_back (std::move (r));
        }
    }
}
