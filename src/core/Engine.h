#pragma once
#include "SignalBus.h"
#include "VisualState.h"
#include "../audio/Analyzer.h"
#include "../midi/MidiTracker.h"
#include "../graph/ModGraph.h"
#include "../modulation/ModulationSystem.h"
#include "../video/CameraInput.h"
#include "../video/VideoDecoder.h"
#include "../presets/PresetManager.h"
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <functional>
#include <utility>

// Central state shared between processor (audio thread), editor (message thread),
// and renderer (GL thread). Includes UndoManager for full undo/redo support.
class Engine
{
public:
    Engine()
        : analyzer (bus),
          midi (bus),
          graph (bus),
          camera (bus),
          videoDecoder (bus),
          modulation (bus, visual, graph),
          presets (*this)
    {
        graph.loadDefault();
        presets.initialise();
        if (! presets.getPresets().empty())
            loadFullState (presets.getPresets()[0].stateData);
        undoManager.clearUndoHistory();
    }

    SignalBus bus;

    // Host/loop signals are registered here, before the subsystems below, because ModGraph and
    // ModulationSystem resolve "host.*" indices in their constructors.
    int sBpm     = bus.add ("host.bpm");
    int sBeat    = bus.add ("host.beat");
    int sPlaying = bus.add ("host.playing");
    int sLoopPos = bus.add ("loop.pos");
    int sLoopRec = bus.add ("loop.recording");
    int sMouseX  = bus.add ("mouse.x");
    int sMouseY  = bus.add ("mouse.y");
    int sMouseDown = bus.add ("mouse.down");

    VisualState visual;
    Analyzer analyzer;
    MidiTracker midi;
    ModGraph graph;
    CameraInput camera;
    VideoDecoder videoDecoder;
    ModulationSystem modulation;
    PresetManager presets;
    juce::UndoManager undoManager { 100, 30 };

    // Live-loop commands, consumed by the renderer: 0 none, 1 toggle record, 2 clear
    std::atomic<int> loopCommand { 0 };
    std::atomic<float> measuredFps { 60.0f };

    // Output window "always on top" (pinned). Lives here so the choice survives the editor being closed/reopened.
    std::atomic<bool> outputPinned { true };

    std::atomic<int> shaderVersion { 0 }, modelVersion { 0 }, uiRevision { 1 };

    void setShaderPath (const juce::String& p) { { const juce::ScopedLock l (lock); shaderPath = p; } ++shaderVersion; }
    void setModelPath (const juce::String& p) { { const juce::ScopedLock l (lock); modelPath = p; } ++modelVersion; }
    juce::String getShaderPath() const { const juce::ScopedLock l (lock); return shaderPath; }
    juce::String getModelPath() const { const juce::ScopedLock l (lock); return modelPath; }
    void setStatus (const juce::String& s) { const juce::ScopedLock l (lock); status = s; }
    juce::String getStatus() const { const juce::ScopedLock l (lock); return status; }

    // Full state snapshot for presets, DAW state saving, and Undo/Redo
    juce::var saveFullState() const
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("visual", visual.toVar());
        o->setProperty ("modulation", modulation.toVar());
        o->setProperty ("graph", graph.toVar());
        o->setProperty ("shader", getShaderPath());
        o->setProperty ("model", getModelPath());
        o->setProperty ("videoFile", videoDecoder.getLoadedPath());
        o->setProperty ("source", analyzer.source.load());
        o->setProperty ("filterType", analyzer.filterType.load());
        o->setProperty ("cutoff", analyzer.cutoffHz.load());
        o->setProperty ("attack", analyzer.attackMs.load());
        o->setProperty ("release", analyzer.releaseMs.load());
        o->setProperty ("sensitivity", analyzer.transientSensitivity.load());
        return juce::var (o);
    }

    void loadFullState (const juce::var& v)
    {
        if (! v.isObject())
            return;
        if (v.hasProperty ("visual"))
            visual.fromVar (v.getProperty ("visual", juce::var()));
        if (v.hasProperty ("modulation"))
            modulation.fromVar (v.getProperty ("modulation", juce::var()));
        // Only rebuild what actually changed: reloading the graph recompiles scripts and reloads
        // WASM files, and the path setters trigger shader recompiles / glTF / video decodes.
        if (v.hasProperty ("graph"))
        {
            const auto g = v.getProperty ("graph", juce::var());
            if (juce::JSON::toString (g, true) != juce::JSON::toString (graph.toVar(), true))
                graph.fromVar (g);
        }
        if (v.hasProperty ("shader"))
        {
            const auto sp = v.getProperty ("shader", "").toString();
            if (sp != getShaderPath()) setShaderPath (sp);
        }
        if (v.hasProperty ("model"))
        {
            const auto mp = v.getProperty ("model", "").toString();
            if (mp != getModelPath()) setModelPath (mp);
        }
        if (v.hasProperty ("videoFile"))
        {
            const auto vp = v.getProperty ("videoFile", "").toString();
            if (vp.isNotEmpty() && vp != videoDecoder.getLoadedPath()) videoDecoder.loadFile (vp);
        }
        analyzer.source               = (int) v.getProperty ("source", analyzer.source.load());
        analyzer.filterType           = (int) v.getProperty ("filterType", analyzer.filterType.load());
        analyzer.cutoffHz             = (float) (double) v.getProperty ("cutoff", analyzer.cutoffHz.load());
        analyzer.attackMs             = (float) (double) v.getProperty ("attack", analyzer.attackMs.load());
        analyzer.releaseMs            = (float) (double) v.getProperty ("release", analyzer.releaseMs.load());
        analyzer.transientSensitivity = (float) (double) v.getProperty ("sensitivity", analyzer.transientSensitivity.load());
        ++uiRevision;
    }

    // Undoable action helper for any state transition
    class StateTransaction final : public juce::UndoableAction
    {
    public:
        StateTransaction (Engine& e, juce::var beforeState, juce::var afterState, const juce::String& label)
            : eng (e), before (std::move (beforeState)), after (std::move (afterState)), desc (label) {}

        bool perform() override
        {
            // UndoManager::perform() calls this immediately, but the mutator has already applied
            // the new state, so only reload it on redo.
            if (std::exchange (firstPerform, false))
                return true;
            eng.loadFullState (after);
            eng.setStatus (desc);
            return true;
        }
        bool undo() override
        {
            eng.loadFullState (before);
            eng.setStatus ("Undo: " + desc);
            return true;
        }
        int getSizeInUnits() override { return 1; }

    private:
        Engine& eng;
        juce::var before, after;
        juce::String desc;
        bool firstPerform = true;
    };

    void performUndoable (const juce::String& actionName, const std::function<void()>& mutator)
    {
        undoManager.beginNewTransaction (actionName);
        auto before = saveFullState();
        mutator();
        auto after = saveFullState();
        ++uiRevision;
        undoManager.perform (new StateTransaction (*this, std::move (before), std::move (after), actionName));
    }

private:
    mutable juce::CriticalSection lock;
    juce::String shaderPath, modelPath, status { "Ready" };
};
