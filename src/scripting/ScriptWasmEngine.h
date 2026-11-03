#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>
#include <vector>

// Context passed into Script or WASM node evaluation each frame
struct EvalContext
{
    float x = 0.0f;     // Input pin 0
    float y = 0.0f;     // Input pin 1
    float time = 0.0f;  // Seconds since start
    float dt = 0.016f;  // Frame delta time
    float a = 1.0f;     // Node param A
    float b = 1.0f;     // Node param B
    float c = 0.0f;     // Node param C
    float d = 1.0f;     // Node param D
    float rms = 0.0f;
    float env = 0.0f;
    float trans = 0.0f;
    float beat = 0.0f;
    float state = 0.0f; // Persistent state variable (in/out)
};

// Fast pre-compiled Luau/Lua-style math & stateful script evaluator
class ScriptProgram
{
public:
    bool compile (const juce::String& sourceCode, juce::String& errorOut);
    float evaluate (EvalContext& ctx) const noexcept;
    const juce::String& getSource() const noexcept { return src; }

private:
    enum class OpCode : uint8_t
    {
        PushConst,
        PushX, PushY, PushTime, PushDt, PushA, PushB, PushC, PushD,
        PushRms, PushEnv, PushTrans, PushBeat, PushState,
        Add, Sub, Mul, Div, Pow,
        Neg, Sin, Cos, Tan, Abs, Sqrt, Floor, Fract, Exp,
        Min, Max, Clamp, Mix, Step, Smoothstep,
        StoreState, StoreOut
    };

    struct Instr
    {
        OpCode op = OpCode::PushConst;
        float imm = 0.0f;
    };

    struct StmtParser;
    juce::String src;
    std::vector<Instr> code;
};

// Sandboxed WebAssembly (.wasm) binary module loader & deterministic f32 stack interpreter.
// Expects an exported function (e.g. `process(f32 x, f32 y, f32 time, f32 a, f32 b) -> f32`).
class WasmModule
{
public:
    bool loadFromBytes (const void* data, size_t size, juce::String& errorOut);
    bool loadFromFile (const juce::File& file, juce::String& errorOut);
    bool isLoaded() const noexcept { return ! bodyBytes.empty(); }
    float evaluate (const EvalContext& ctx) const noexcept;
    const juce::String& getExportName() const noexcept { return exportedName; }

private:
    std::vector<uint8_t> bodyBytes;
    uint32_t numLocals = 5;
    juce::String exportedName { "process" };
};
