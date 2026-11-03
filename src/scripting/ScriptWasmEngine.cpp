#include "ScriptWasmEngine.h"
#include <cmath>
#include <cstring>

// ==============================================================================
// 1. Luau / Lua-Style Expression & Multi-Statement Script Compiler
// ==============================================================================
struct ScriptProgram::StmtParser
{
    const juce::String& text;
    int pos = 0;
    std::vector<Instr>& out;
    juce::String& err;

    StmtParser (const juce::String& s, std::vector<Instr>& o, juce::String& e)
        : text (s), out (o), err (e) {}

    void skipWS()
    {
        while (pos < text.length())
        {
            const auto ch = text[pos];
            if (ch == ' ' || ch == '\t' || ch == '\r') { ++pos; continue; }
            // Skip `--` or `//` comments
            if ((ch == '-' && pos + 1 < text.length() && text[pos + 1] == '-')
                || (ch == '/' && pos + 1 < text.length() && text[pos + 1] == '/'))
            {
                while (pos < text.length() && text[pos] != '\n') ++pos;
                continue;
            }
            break;
        }
    }

    bool match (char c)
    {
        skipWS();
        if (pos < text.length() && text[pos] == (juce::juce_wchar) c) { ++pos; return true; }
        return false;
    }

    juce::String readIdent()
    {
        skipWS();
        const int start = pos;
        while (pos < text.length() && (juce::CharacterFunctions::isLetterOrDigit (text[pos]) || text[pos] == '_'))
            ++pos;
        return text.substring (start, pos);
    }

    bool parseAll()
    {
        while (pos < text.length())
        {
            skipWS();
            if (pos >= text.length()) break;
            if (match (';') || match ('\n')) continue;

            const int save = pos;
            auto id = readIdent();
            if (id == "local") id = readIdent();
            skipWS();

            if (id.isNotEmpty() && match ('='))
            {
                if (! parseExpr()) return false;
                if (id == "s" || id == "state")
                    out.push_back ({ OpCode::StoreState, 0.0f });
                else
                    out.push_back ({ OpCode::StoreOut, 0.0f });
            }
            else
            {
                pos = save;
                if (id == "return") pos += 6;
                if (! parseExpr()) return false;
                out.push_back ({ OpCode::StoreOut, 0.0f });
            }
            while (match (';') || match ('\n')) {}
        }
        return ! out.empty();
    }

    bool parseExpr() { return parseAddSub(); }

    bool parseAddSub()
    {
        if (! parseMulDiv()) return false;
        for (;;)
        {
            if (match ('+'))      { if (! parseMulDiv()) return false; out.push_back ({ OpCode::Add, 0.0f }); }
            else if (match ('-')) { if (! parseMulDiv()) return false; out.push_back ({ OpCode::Sub, 0.0f }); }
            else break;
        }
        return true;
    }

    bool parseMulDiv()
    {
        if (! parsePow()) return false;
        for (;;)
        {
            if (match ('*'))      { if (! parsePow()) return false; out.push_back ({ OpCode::Mul, 0.0f }); }
            else if (match ('/')) { if (! parsePow()) return false; out.push_back ({ OpCode::Div, 0.0f }); }
            else break;
        }
        return true;
    }

    bool parsePow()
    {
        if (! parseUnary()) return false;
        if (match ('^'))
        {
            if (! parsePow()) return false;
            out.push_back ({ OpCode::Pow, 0.0f });
        }
        return true;
    }

    bool parseUnary()
    {
        if (match ('+')) return parseUnary();
        if (match ('-'))
        {
            if (! parseUnary()) return false;
            out.push_back ({ OpCode::Neg, 0.0f });
            return true;
        }
        return parsePrimary();
    }

    bool parsePrimary()
    {
        skipWS();
        if (match ('('))
        {
            if (! parseExpr()) return false;
            if (! match (')')) { err = "Expected ')'"; return false; }
            return true;
        }

        // Number literal
        if (pos < text.length() && (juce::CharacterFunctions::isDigit (text[pos]) || text[pos] == '.'))
        {
            const int start = pos;
            while (pos < text.length() && (juce::CharacterFunctions::isDigit (text[pos]) || text[pos] == '.'))
                ++pos;
            const float val = text.substring (start, pos).getFloatValue();
            out.push_back ({ OpCode::PushConst, val });
            return true;
        }

        const auto id = readIdent().toLowerCase();
        if (id.isEmpty())
        {
            err = "Unexpected token near pos " + juce::String (pos);
            return false;
        }

        // Function call
        if (match ('('))
        {
            int argc = 0;
            if (! match (')'))
            {
                for (;;)
                {
                    if (! parseExpr()) return false;
                    ++argc;
                    if (match (',')) continue;
                    if (match (')')) break;
                    err = "Expected ',' or ')' in function " + id;
                    return false;
                }
            }
            if (id == "sin" && argc == 1)        out.push_back ({ OpCode::Sin, 0 });
            else if (id == "cos" && argc == 1)   out.push_back ({ OpCode::Cos, 0 });
            else if (id == "tan" && argc == 1)   out.push_back ({ OpCode::Tan, 0 });
            else if (id == "abs" && argc == 1)   out.push_back ({ OpCode::Abs, 0 });
            else if (id == "sqrt" && argc == 1)  out.push_back ({ OpCode::Sqrt, 0 });
            else if (id == "floor" && argc == 1) out.push_back ({ OpCode::Floor, 0 });
            else if (id == "fract" && argc == 1) out.push_back ({ OpCode::Fract, 0 });
            else if (id == "exp" && argc == 1)   out.push_back ({ OpCode::Exp, 0 });
            else if (id == "min" && argc == 2)   out.push_back ({ OpCode::Min, 0 });
            else if (id == "max" && argc == 2)   out.push_back ({ OpCode::Max, 0 });
            else if (id == "pow" && argc == 2)   out.push_back ({ OpCode::Pow, 0 });
            else if (id == "step" && argc == 2)  out.push_back ({ OpCode::Step, 0 });
            else if (id == "clamp" && argc == 3) out.push_back ({ OpCode::Clamp, 0 });
            else if ((id == "mix" || id == "lerp") && argc == 3) out.push_back ({ OpCode::Mix, 0 });
            else if (id == "smoothstep" && argc == 3) out.push_back ({ OpCode::Smoothstep, 0 });
            else { err = "Unknown function or wrong arg count: " + id; return false; }
            return true;
        }

        // Built-in variables
        if (id == "x" || id == "in0" || id == "in1") out.push_back ({ OpCode::PushX, 0 });
        else if (id == "y" || id == "in2")           out.push_back ({ OpCode::PushY, 0 });
        else if (id == "t" || id == "time")          out.push_back ({ OpCode::PushTime, 0 });
        else if (id == "dt")                         out.push_back ({ OpCode::PushDt, 0 });
        else if (id == "a")                          out.push_back ({ OpCode::PushA, 0 });
        else if (id == "b")                          out.push_back ({ OpCode::PushB, 0 });
        else if (id == "c")                          out.push_back ({ OpCode::PushC, 0 });
        else if (id == "d")                          out.push_back ({ OpCode::PushD, 0 });
        else if (id == "rms")                        out.push_back ({ OpCode::PushRms, 0 });
        else if (id == "env")                        out.push_back ({ OpCode::PushEnv, 0 });
        else if (id == "trans" || id == "transient") out.push_back ({ OpCode::PushTrans, 0 });
        else if (id == "beat")                       out.push_back ({ OpCode::PushBeat, 0 });
        else if (id == "s" || id == "state")         out.push_back ({ OpCode::PushState, 0 });
        else if (id == "pi")                         out.push_back ({ OpCode::PushConst, juce::MathConstants<float>::pi });
        else if (id == "tau")                        out.push_back ({ OpCode::PushConst, juce::MathConstants<float>::twoPi });
        else { err = "Unknown variable: " + id; return false; }
        return true;
    }
};

bool ScriptProgram::compile (const juce::String& sourceCode, juce::String& errorOut)
{
    src = sourceCode;
    std::vector<Instr> tmp;
    juce::String err;
    StmtParser p (sourceCode, tmp, err);
    if (! p.parseAll())
    {
        errorOut = err.isEmpty() ? "Empty script" : err;
        return false;
    }
    code = std::move (tmp);
    return true;
}

float ScriptProgram::evaluate (EvalContext& ctx) const noexcept
{
    if (code.empty())
        return ctx.x;

    float stack[64] {};
    int sp = 0;
    auto push = [&] (float v) { if (sp < 64) stack[sp++] = std::isfinite (v) ? v : 0.0f; };
    auto pop  = [&]() -> float { return sp > 0 ? stack[--sp] : 0.0f; };

    float result = 0.0f;
    for (const auto& ins : code)
    {
        switch (ins.op)
        {
            case OpCode::PushConst: push (ins.imm); break;
            case OpCode::PushX:     push (ctx.x); break;
            case OpCode::PushY:     push (ctx.y); break;
            case OpCode::PushTime:  push (ctx.time); break;
            case OpCode::PushDt:    push (ctx.dt); break;
            case OpCode::PushA:     push (ctx.a); break;
            case OpCode::PushB:     push (ctx.b); break;
            case OpCode::PushC:     push (ctx.c); break;
            case OpCode::PushD:     push (ctx.d); break;
            case OpCode::PushRms:   push (ctx.rms); break;
            case OpCode::PushEnv:   push (ctx.env); break;
            case OpCode::PushTrans: push (ctx.trans); break;
            case OpCode::PushBeat:  push (ctx.beat); break;
            case OpCode::PushState: push (ctx.state); break;
            case OpCode::Add: { const float r = pop(), l = pop(); push (l + r); break; }
            case OpCode::Sub: { const float r = pop(), l = pop(); push (l - r); break; }
            case OpCode::Mul: { const float r = pop(), l = pop(); push (l * r); break; }
            case OpCode::Div: { const float r = pop(), l = pop(); push (std::abs (r) > 1e-7f ? l / r : 0.0f); break; }
            case OpCode::Pow: { const float r = pop(), l = pop(); push (std::pow (std::abs (l), r)); break; }
            case OpCode::Neg:   push (-pop()); break;
            case OpCode::Sin:   push (std::sin (pop())); break;
            case OpCode::Cos:   push (std::cos (pop())); break;
            case OpCode::Tan:   push (std::tan (pop())); break;
            case OpCode::Abs:   push (std::abs (pop())); break;
            case OpCode::Sqrt:  push (std::sqrt (juce::jmax (0.0f, pop()))); break;
            case OpCode::Floor: push (std::floor (pop())); break;
            case OpCode::Fract: { const float v = pop(); push (v - std::floor (v)); break; }
            case OpCode::Exp:   push (std::exp (juce::jlimit (-20.0f, 20.0f, pop()))); break;
            case OpCode::Min:   { const float r = pop(), l = pop(); push (juce::jmin (l, r)); break; }
            case OpCode::Max:   { const float r = pop(), l = pop(); push (juce::jmax (l, r)); break; }
            case OpCode::Step:  { const float x = pop(), edge = pop(); push (x >= edge ? 1.0f : 0.0f); break; }
            case OpCode::Clamp: { const float hi = pop(), lo = pop(), v = pop(); push (juce::jlimit (lo, hi, v)); break; }
            case OpCode::Mix:   { const float t = pop(), b = pop(), a = pop(); push (a + (b - a) * t); break; }
            case OpCode::Smoothstep:
            {
                const float x = pop(), e1 = pop(), e0 = pop();
                const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / juce::jmax (1e-6f, e1 - e0));
                push (t * t * (3.0f - 2.0f * t));
                break;
            }
            case OpCode::StoreState: ctx.state = pop(); result = ctx.state; break;
            case OpCode::StoreOut:   result = pop(); break;
        }
    }
    return std::isfinite (result) ? result : 0.0f;
}

// ==============================================================================
// 2. Sandboxed WebAssembly (.wasm) Binary Module Loader & Interpreter
// ==============================================================================
namespace
{
uint32_t readLEB128 (const uint8_t*& p, const uint8_t* end)
{
    uint32_t result = 0, shift = 0;
    while (p < end)
    {
        const uint8_t byte = *p++;
        if (shift < 32) // malformed input can encode more than 5 bytes; shifting >= 32 is UB
            result |= (uint32_t) (byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    return result;
}
} // namespace

bool WasmModule::loadFromFile (const juce::File& file, juce::String& errorOut)
{
    juce::MemoryBlock mb;
    if (! file.loadFileAsData (mb))
    {
        errorOut = "Could not read .wasm file";
        return false;
    }
    return loadFromBytes (mb.getData(), mb.getSize(), errorOut);
}

bool WasmModule::loadFromBytes (const void* data, size_t size, juce::String& errorOut)
{
    if (size < 8)
    {
        errorOut = "File too small to be a valid .wasm module";
        return false;
    }
    const auto* bytes = static_cast<const uint8_t*> (data);
    // Check magic `\0asm` and version 1
    if (bytes[0] != 0x00 || bytes[1] != 0x61 || bytes[2] != 0x73 || bytes[3] != 0x6d
        || bytes[4] != 0x01 || bytes[5] != 0x00 || bytes[6] != 0x00 || bytes[7] != 0x00)
    {
        errorOut = "Invalid WASM magic/version header";
        return false;
    }

    const uint8_t* p = bytes + 8;
    const uint8_t* end = bytes + size;
    std::vector<uint8_t> foundCode;
    uint32_t extraLocals = 0;

    while (p < end)
    {
        const uint8_t sectionId = *p++;
        const uint32_t sectionLen = readLEB128 (p, end);
        if (sectionLen > (uint32_t) (end - p)) break;
        const uint8_t* secEnd = p + sectionLen;

        if (sectionId == 7) // Export section
        {
            const uint32_t count = readLEB128 (p, secEnd);
            for (uint32_t i = 0; i < count && p < secEnd; ++i)
            {
                const uint32_t nameLen = readLEB128 (p, secEnd);
                if (nameLen >= (uint32_t) (secEnd - p)) break; // need the name plus the kind byte
                const auto name = juce::String::fromUTF8 ((const char*) p, (int) nameLen);
                p += nameLen;
                const uint8_t kind = *p++;
                /*const uint32_t idx =*/ readLEB128 (p, secEnd);
                if (kind == 0) exportedName = name;
            }
        }
        else if (sectionId == 10) // Code section
        {
            const uint32_t funcCount = readLEB128 (p, secEnd);
            if (funcCount >= 1 && p < secEnd)
            {
                const uint32_t bodySize = readLEB128 (p, secEnd);
                if (bodySize <= (uint32_t) (secEnd - p))
                {
                    const uint8_t* bodyEnd = p + bodySize;
                    const uint32_t localDeclCount = readLEB128 (p, bodyEnd);
                    for (uint32_t i = 0; i < localDeclCount && p < bodyEnd; ++i)
                    {
                        const uint32_t n = readLEB128 (p, bodyEnd);
                        if (p < bodyEnd) ++p; // valtype
                        extraLocals += n;
                    }
                    foundCode.assign (p, bodyEnd);
                }
            }
        }
        p = secEnd;
    }

    if (foundCode.empty())
    {
        errorOut = "No executable code section found in .wasm module";
        return false;
    }

    bodyBytes = std::move (foundCode);
    numLocals = 8 + extraLocals;
    return true;
}

float WasmModule::evaluate (const EvalContext& ctx) const noexcept
{
    if (bodyBytes.empty())
        return ctx.x;

    std::array<float, 32> locals {};
    locals[0] = ctx.x;
    locals[1] = ctx.y;
    locals[2] = ctx.time;
    locals[3] = ctx.a;
    locals[4] = ctx.b;
    locals[5] = ctx.env;
    locals[6] = ctx.trans;

    float stack[64] {};
    int sp = 0;
    auto push = [&] (float v) { if (sp < 64) stack[sp++] = std::isfinite (v) ? v : 0.0f; };
    auto pop  = [&]() -> float { return sp > 0 ? stack[--sp] : 0.0f; };

    const uint8_t* p = bodyBytes.data();
    const uint8_t* end = p + bodyBytes.size();
    int steps = 0;

    while (p < end && ++steps < 4096)
    {
        const uint8_t op = *p++;
        switch (op)
        {
            case 0x0b: // end
            case 0x0f: // return
                return pop();
            case 0x20: // local.get
            {
                const uint32_t idx = readLEB128 (p, end);
                push (idx < locals.size() ? locals[idx] : 0.0f);
                break;
            }
            case 0x21: // local.set
            {
                const uint32_t idx = readLEB128 (p, end);
                const float v = pop();
                if (idx < locals.size()) locals[idx] = v;
                break;
            }
            case 0x22: // local.tee
            {
                const uint32_t idx = readLEB128 (p, end);
                const float v = sp > 0 ? stack[sp - 1] : 0.0f;
                if (idx < locals.size()) locals[idx] = v;
                break;
            }
            case 0x43: // f32.const
            {
                if (p + 4 > end) return pop();
                float v = 0.0f;
                std::memcpy (&v, p, 4);
                p += 4;
                push (v);
                break;
            }
            case 0x8b: push (std::abs (pop())); break;                       // f32.abs
            case 0x8c: push (-pop()); break;                                 // f32.neg
            case 0x8d: push (std::ceil (pop())); break;                      // f32.ceil
            case 0x8e: push (std::floor (pop())); break;                     // f32.floor
            case 0x91: push (std::sqrt (juce::jmax (0.0f, pop()))); break;   // f32.sqrt
            case 0x92: { const float r = pop(), l = pop(); push (l + r); break; } // f32.add
            case 0x93: { const float r = pop(), l = pop(); push (l - r); break; } // f32.sub
            case 0x94: { const float r = pop(), l = pop(); push (l * r); break; } // f32.mul
            case 0x95: { const float r = pop(), l = pop(); push (std::abs (r) > 1e-7f ? l / r : 0.0f); break; } // f32.div
            case 0x96: { const float r = pop(), l = pop(); push (juce::jmin (l, r)); break; } // f32.min
            case 0x97: { const float r = pop(), l = pop(); push (juce::jmax (l, r)); break; } // f32.max
            default: break;
        }
    }
    return pop();
}
