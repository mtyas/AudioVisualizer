#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <unordered_map>
#include <vector>

// Named float signals shared between threads (audio thread writes, render thread reads).
// Signals must be registered during construction only (before any thread uses the bus);
// after that the name table is immutable and values are lock-free atomics.
class SignalBus
{
public:
    static constexpr int maxSignals = 1024;

    SignalBus()
    {
        names.reserve (maxSignals);
        index.reserve (maxSignals);
        for (auto& v : values)
            v.store (0.0f, std::memory_order_relaxed);
    }

    int add (const juce::String& name)
    {
        jassert ((int) names.size() < maxSignals);
        names.push_back (name);
        const int idx = (int) names.size() - 1;
        index.emplace (name, idx);
        return idx;
    }

    // O(1) hash lookup (was a linear string scan over ~350 signals).
    int find (const juce::String& name) const
    {
        const auto it = index.find (name);
        return it != index.end() ? it->second : -1;
    }

    void set (int i, float v) noexcept
    {
        if (i >= 0) values[(size_t) i].store (v, std::memory_order_relaxed);
    }

    float get (int i) const noexcept
    {
        return i >= 0 ? values[(size_t) i].load (std::memory_order_relaxed) : 0.0f;
    }

    int size() const noexcept { return (int) names.size(); }
    const juce::String& getName (int i) const { return names[(size_t) i]; }

private:
    std::vector<juce::String> names;
    std::unordered_map<juce::String, int> index;
    std::array<std::atomic<float>, maxSignals> values;
};
