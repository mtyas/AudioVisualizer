#pragma once
#include "../core/SignalBus.h"
#include "../scripting/ScriptWasmEngine.h"
#include <array>
#include <memory>
#include <utility>
#include <vector>

// A node graph that maps signals (audio features, MIDI, LFOs, Chaos, Sequencers, Webcam, Pose, Host)
// through built-in, Script (Luau/Lua-style), or sandboxed WebAssembly (.wasm) nodes to named
// destination parameters ("sinks") consumed by the renderer.
class ModGraph
{
public:
    enum class Type { Source, Constant, Remap, Smooth, Math, Lfo, Sink, Script, Wasm };
    enum class Op { Add, Sub, Mul, Min, Max };

    struct Node
    {
        Type type = Type::Constant;
        juce::String name;          // Source: signal name. Sink: parameter name.
        std::array<int, 2> in { -1, -1 };
        float a = 0, b = 1, c = 0, d = 1, curve = 1;
        Op op = Op::Add;
        int sig = -1;
        int sinkParam = -2; // Sink: destination index cached by ModulationSystem (-2 = unresolved)
        float value = 0, state = 0;

        // Script / WASM extensions
        juce::String scriptCode { "s = s * 0.9 + x * 0.1\nout = sin(t * a + s * 6.283) * b + y" };
        juce::String wasmPath;
        std::shared_ptr<ScriptProgram> compiledScript;
        std::shared_ptr<WasmModule> compiledWasm;

        // UI Canvas coordinates
        float posX = 80.0f, posY = 80.0f;
    };

    explicit ModGraph (const SignalBus& b);

    void evaluate (float dt);
    // Calls fn (name, value, cachedParamIdx&) for each sink under the graph lock, without allocating.
    // cachedParamIdx starts at -2 ("unresolved") and is reset whenever the node is (re)prepared,
    // so callers can cache their destination lookup in it.
    template <typename Fn>
    void forEachSink (Fn&& fn)
    {
        const juce::ScopedLock sl (lock);
        for (auto& n : nodes)
            if (n.type == Type::Sink)
                fn (n.name, n.value, n.sinkParam);
    }

    std::vector<Node> getNodes() const;
    void setNodes (const std::vector<Node>& newNodes);
    int addNode (const Node& n);
    void removeNode (int idx);
    void updateNode (int idx, const Node& n, juce::String* compileErrOut = nullptr);
    void connect (int fromNode, int toNode, int inputPin);
    void disconnect (int toNode, int inputPin);

    juce::var toVar() const;
    bool fromVar (const juce::var&);
    void loadDefault();

private:
    static void prepareNodeRuntime (const SignalBus& bus, Node& n, juce::String* errOut = nullptr);

    mutable juce::CriticalSection lock;
    std::vector<Node> nodes;
    const SignalBus& bus;
    double elapsedTime = 0.0;
    int sRms = -1, sEnv = -1, sTrans = -1, sBeat = -1;
};
