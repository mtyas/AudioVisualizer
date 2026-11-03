#include "ModGraph.h"
#include <cmath>

namespace
{
const char* typeNames[] = { "source", "const", "remap", "smooth", "math", "lfo", "sink", "script", "wasm" };
const char* opNames[] = { "add", "sub", "mul", "min", "max" };

const char* defaultGraphJson = R"json({"nodes":[
 {"type":"source","name":"audio.env","x":30,"y":35},
 {"type":"remap","inputs":[0],"a":0,"b":0.5,"c":0,"d":1.2,"x":210,"y":35},
 {"type":"smooth","inputs":[1],"a":0.015,"b":0.16,"x":390,"y":35},
 {"type":"sink","name":"scene.pulse","inputs":[2],"x":570,"y":35},
 {"type":"source","name":"audio.transient","x":30,"y":135},
 {"type":"remap","inputs":[4],"a":0,"b":1,"c":0.0,"d":1.4,"x":210,"y":135},
 {"type":"smooth","inputs":[5],"a":0.005,"b":0.22,"x":390,"y":135},
 {"type":"sink","name":"post.bloom","inputs":[6],"x":570,"y":135},
 {"type":"source","name":"audio.centroid","x":30,"y":245},
 {"type":"script","inputs":[8],"a":1.5,"b":0.4,"script":"s = s * 0.92 + x * 0.08\nout = sin(t * a + s * 6.283) * b","x":240,"y":245},
 {"type":"sink","name":"scene.hue","inputs":[9],"x":480,"y":245}
]})json";

float num (const juce::var& v, const char* key, float def)
{
    const auto p = v.getProperty (key, juce::var());
    return p.isVoid() ? def : (float) (double) p;
}
} // namespace

ModGraph::ModGraph (const SignalBus& b) : bus (b)
{
    sRms   = bus.find ("audio.rms");
    sEnv   = bus.find ("audio.env");
    sTrans = bus.find ("audio.transient");
    sBeat  = bus.find ("host.beat");
}

void ModGraph::prepareNodeRuntime (const SignalBus& bus, Node& n, juce::String* errOut)
{
    n.sinkParam = -2;
    if (n.type == Type::Source)
    {
        n.sig = bus.find (n.name);
    }
    else if (n.type == Type::Script)
    {
        auto prog = std::make_shared<ScriptProgram>();
        juce::String err;
        if (prog->compile (n.scriptCode, err))
            n.compiledScript = std::move (prog);
        else if (errOut != nullptr)
            *errOut = err;
    }
    else if (n.type == Type::Wasm && n.wasmPath.isNotEmpty())
    {
        auto mod = std::make_shared<WasmModule>();
        juce::String err;
        if (mod->loadFromFile (juce::File (n.wasmPath), err))
            n.compiledWasm = std::move (mod);
        else if (errOut != nullptr)
            *errOut = err;
    }
}

void ModGraph::evaluate (float dt)
{
    elapsedTime += (double) dt;
    const float vRms   = sRms   >= 0 ? bus.get (sRms)   : 0.0f;
    const float vEnv   = sEnv   >= 0 ? bus.get (sEnv)   : 0.0f;
    const float vTrans = sTrans >= 0 ? bus.get (sTrans) : 0.0f;
    const float vBeat  = sBeat  >= 0 ? bus.get (sBeat)  : 0.0f;

    const juce::ScopedLock sl (lock);
    const int count = (int) nodes.size();
    for (int i = 0; i < count; ++i)
    {
        auto& n = nodes[(size_t) i];
        const float x = (n.in[0] >= 0 && n.in[0] < count && n.in[0] != i) ? nodes[(size_t) n.in[0]].value : 0.0f;
        const float y = (n.in[1] >= 0 && n.in[1] < count && n.in[1] != i) ? nodes[(size_t) n.in[1]].value : 0.0f;
        switch (n.type)
        {
            case Type::Source:
                if (n.sig < 0 && n.name.isNotEmpty())
                    n.sig = bus.find (n.name);
                n.value = bus.get (n.sig);
                break;
            case Type::Constant: n.value = n.a; break;
            case Type::Remap:
            {
                const float span = n.b - n.a;
                float t = std::abs (span) > 1.0e-9f ? (x - n.a) / span : 0.0f;
                t = juce::jlimit (0.0f, 1.0f, t);
                if (n.curve != 1.0f)
                    t = std::pow (t, juce::jmax (0.01f, n.curve));
                n.value = n.c + t * (n.d - n.c);
                break;
            }
            case Type::Smooth:
            {
                const float tc = x > n.state ? n.a : n.b;
                const float coef = 1.0f - std::exp (-dt / juce::jmax (1.0e-4f, tc));
                n.state += (x - n.state) * coef;
                n.value = n.state;
                break;
            }
            case Type::Math:
                switch (n.op)
                {
                    case Op::Add: n.value = x + y; break;
                    case Op::Sub: n.value = x - y; break;
                    case Op::Mul: n.value = x * y; break;
                    case Op::Min: n.value = juce::jmin (x, y); break;
                    case Op::Max: n.value = juce::jmax (x, y); break;
                }
                break;
            case Type::Lfo:
            {
                n.state = std::fmod (n.state + n.a * dt, 1.0f);
                const float p = n.state;
                switch ((int) n.b)
                {
                    case 1:  n.value = 1.0f - std::abs (2.0f * p - 1.0f); break;
                    case 2:  n.value = p; break;
                    case 3:  n.value = p < 0.5f ? 1.0f : 0.0f; break;
                    default: n.value = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * p); break;
                }
                break;
            }
            case Type::Sink: n.value = x; break;
            case Type::Script:
            {
                if (n.compiledScript)
                {
                    EvalContext ctx { x, y, (float) elapsedTime, dt, n.a, n.b, n.c, n.d, vRms, vEnv, vTrans, vBeat, n.state };
                    n.value = n.compiledScript->evaluate (ctx);
                    n.state = ctx.state;
                }
                else
                    n.value = x;
                break;
            }
            case Type::Wasm:
            {
                if (n.compiledWasm && n.compiledWasm->isLoaded())
                {
                    EvalContext ctx { x, y, (float) elapsedTime, dt, n.a, n.b, n.c, n.d, vRms, vEnv, vTrans, vBeat, n.state };
                    n.value = n.compiledWasm->evaluate (ctx);
                }
                else
                    n.value = x;
                break;
            }
        }
    }
}

std::vector<ModGraph::Node> ModGraph::getNodes() const
{
    const juce::ScopedLock sl (lock);
    return nodes;
}

void ModGraph::setNodes (const std::vector<Node>& newNodes)
{
    const juce::ScopedLock sl (lock);
    nodes = newNodes;
    for (auto& n : nodes)
        prepareNodeRuntime (bus, n);
}

int ModGraph::addNode (const Node& n)
{
    const juce::ScopedLock sl (lock);
    Node copy = n;
    prepareNodeRuntime (bus, copy);
    nodes.push_back (std::move (copy));
    return (int) nodes.size() - 1;
}

void ModGraph::removeNode (int idx)
{
    const juce::ScopedLock sl (lock);
    if (idx < 0 || idx >= (int) nodes.size())
        return;
    nodes.erase (nodes.begin() + idx);
    for (auto& n : nodes)
    {
        for (int k = 0; k < 2; ++k)
        {
            if (n.in[(size_t) k] == idx)
                n.in[(size_t) k] = -1;
            else if (n.in[(size_t) k] > idx)
                --n.in[(size_t) k];
        }
    }
}

void ModGraph::updateNode (int idx, const Node& n, juce::String* compileErrOut)
{
    const juce::ScopedLock sl (lock);
    if (idx < 0 || idx >= (int) nodes.size())
        return;
    Node copy = n;
    prepareNodeRuntime (bus, copy, compileErrOut);
    nodes[(size_t) idx] = std::move (copy);
}

void ModGraph::connect (int fromNode, int toNode, int inputPin)
{
    const juce::ScopedLock sl (lock);
    if (fromNode < 0 || fromNode >= (int) nodes.size() || toNode < 0 || toNode >= (int) nodes.size() || fromNode == toNode)
        return;
    inputPin = juce::jlimit (0, 1, inputPin);
    nodes[(size_t) toNode].in[(size_t) inputPin] = fromNode;
}

void ModGraph::disconnect (int toNode, int inputPin)
{
    const juce::ScopedLock sl (lock);
    if (toNode < 0 || toNode >= (int) nodes.size())
        return;
    inputPin = juce::jlimit (0, 1, inputPin);
    nodes[(size_t) toNode].in[(size_t) inputPin] = -1;
}

juce::var ModGraph::toVar() const
{
    const juce::ScopedLock sl (lock);
    juce::Array<juce::var> arr;
    for (const auto& n : nodes)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", typeNames[(int) n.type]);
        if (n.type == Type::Source || n.type == Type::Sink)
            o->setProperty ("name", n.name);
        if (n.type == Type::Script)
            o->setProperty ("script", n.scriptCode);
        if (n.type == Type::Wasm)
            o->setProperty ("wasmPath", n.wasmPath);
        juce::Array<juce::var> ins;
        for (int k = 0; k < 2; ++k)
            ins.add (n.in[(size_t) k]);
        o->setProperty ("inputs", ins);
        o->setProperty ("a", n.a);
        o->setProperty ("b", n.b);
        o->setProperty ("c", n.c);
        o->setProperty ("d", n.d);
        o->setProperty ("curve", n.curve);
        o->setProperty ("op", opNames[(int) n.op]);
        o->setProperty ("x", n.posX);
        o->setProperty ("y", n.posY);
        arr.add (juce::var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("nodes", arr);
    return juce::var (root);
}

bool ModGraph::fromVar (const juce::var& v)
{
    const juce::Array<juce::var>* arr = v.isArray() ? v.getArray() : v.getProperty ("nodes", juce::var()).getArray();
    if (arr == nullptr)
        return false;

    std::vector<Node> tmp;
    const int total = arr->size();
    for (int i = 0; i < total; ++i)
    {
        const auto& nv = (*arr)[i];
        Node n;
        const auto t = nv.getProperty ("type", juce::var()).toString();
        bool found = false;
        for (int k = 0; k < 9; ++k)
            if (t == typeNames[k]) { n.type = (Type) k; found = true; }
        if (! found)
            return false;
        n.name = nv.getProperty ("name", juce::var()).toString();
        if (nv.hasProperty ("script"))
            n.scriptCode = nv.getProperty ("script", n.scriptCode).toString();
        if (nv.hasProperty ("wasmPath"))
            n.wasmPath = nv.getProperty ("wasmPath", "").toString();
        if (const auto* ia = nv.getProperty ("inputs", juce::var()).getArray())
            for (int k = 0; k < 2 && k < ia->size(); ++k)
            {
                const int idx = (int) (*ia)[k];
                n.in[(size_t) k] = (idx >= 0 && idx < total && idx != i) ? idx : -1;
            }
        n.a = num (nv, "a", 1.0f);
        n.b = num (nv, "b", 1.0f);
        n.c = num (nv, "c", 0.0f);
        n.d = num (nv, "d", 1.0f);
        n.curve = num (nv, "curve", 1.0f);
        n.posX = num (nv, "x", 60.0f + (float) (i % 4) * 190.0f);
        n.posY = num (nv, "y", 50.0f + (float) (i / 4) * 115.0f);
        const auto op = nv.getProperty ("op", juce::var()).toString();
        for (int k = 0; k < 5; ++k)
            if (op == opNames[k]) n.op = (Op) k;
        prepareNodeRuntime (bus, n);
        tmp.push_back (std::move (n));
    }

    const juce::ScopedLock sl (lock);
    nodes = std::move (tmp);
    return true;
}

void ModGraph::loadDefault()
{
    fromVar (juce::JSON::parse (juce::String (defaultGraphJson)));
}
