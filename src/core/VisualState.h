#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <unordered_map>
#include <vector>

// The kinds of layer that can be stacked. Layers 0..4 (kLegacyLayers) always exist, one of each of the first
// five kinds; up to kExtraLayers more can be added in any kind.
enum class LayerType : int
{
    Scene3D = 0,
    Shader2D,
    VideoLoop,
    WebcamFX,
    Text,
    Spectrum,
    Fill,
    Count
};

enum class BlendMode : int
{
    Add = 0,
    Screen = 1,
    Multiply = 2,
    Overlay = 3,
    Alpha = 4,
    Difference = 5
};

struct ParamInfo
{
    juce::String id;
    juce::String name;
    juce::String category;
    float minVal = 0.0f;
    float maxVal = 1.0f;
    float defaultVal = 0.0f;
    float step = 0.0f; // 0 = continuous, 1 = integer/enum
    int layerId = -1;  // owning layer (-1 = global / post-processing)
    int layerType = -1; // LayerType this parameter belongs to (-1 for global)
};

// Holds all visual destination parameters (base values set by UI/presets, plus modulated values
// computed each frame), the layer stack, custom shader code, and per-layer text/font.
class VisualState
{
public:
    static constexpr int kLegacyLayers = 5, kExtraLayers = 8, kMaxLayers = kLegacyLayers + kExtraLayers;
    static constexpr size_t maxParams = 1024;

    struct LayerStack
    {
        std::array<int, kMaxLayers> ids {};   // bottom to top
        int count = 0;
    };

    VisualState();

    int getNumParams() const noexcept { return (int) params.size(); }
    const ParamInfo& getParamInfo (int idx) const { return params[(size_t) idx]; }
    int findParam (const juce::String& id) const;

    float getBase (int idx) const noexcept
    {
        return (idx >= 0 && idx < (int) params.size())
                   ? baseValues[(size_t) idx].load (std::memory_order_relaxed)
                   : 0.0f;
    }

    float getBase (const juce::String& id) const noexcept { return getBase (findParam (id)); }

    void setBase (int idx, float v) noexcept
    {
        if (idx >= 0 && idx < (int) params.size())
        {
            const auto& info = params[(size_t) idx];
            v = juce::jlimit (info.minVal, info.maxVal, v);
            baseValues[(size_t) idx].store (v, std::memory_order_relaxed);
        }
    }

    void setBase (const juce::String& id, float v) noexcept { setBase (findParam (id), v); }

    float getModulated (int idx) const noexcept
    {
        return (idx >= 0 && idx < (int) params.size())
                   ? modValues[(size_t) idx].load (std::memory_order_relaxed)
                   : 0.0f;
    }

    float getModulated (const juce::String& id) const noexcept { return getModulated (findParam (id)); }

    void setModulated (int idx, float v) noexcept
    {
        if (idx >= 0 && idx < (int) params.size())
        {
            const auto& info = params[(size_t) idx];
            v = juce::jlimit (info.minVal, info.maxVal, v);
            modValues[(size_t) idx].store (v, std::memory_order_relaxed);
        }
    }

    void resetAllToDefaults();

    // ---- Layer stack ----
    LayerStack getLayerStack() const;
    void setLayerStack (const LayerStack&);
    void moveLayer (int fromSlot, int toSlot);          // swaps two stack positions
    int addLayer (LayerType t);                         // returns the new layer id, or -1 when full
    void removeLayer (int layerId);                     // extra layers only
    bool isLayerActive (int layerId) const;             // legacy layers are always active
    LayerType getLayerType (int layerId) const;         // Count if the slot is unused
    juce::String getLayerPrefix (int layerId) const;    // "scene" for layer 0, "L3.shader" for an extra, ...
    juce::String getLayerName (int layerId) const;      // "3D Scene", "2D Shader (L2)", ...
    static bool isLegacyLayer (int layerId) noexcept { return layerId >= 0 && layerId < kLegacyLayers; }
    bool isParamAvailable (int idx) const;              // belongs to an active layer, or is global
    std::atomic<int> layerRevision { 1 };               // bumps whenever layers are added / removed

    // Per-layer text content and font (text layers). Layer 4 is the original overlay text.
    juce::String getLayerText (int layerId) const;
    void setLayerText (int layerId, const juce::String& t);
    juce::String getLayerFont (int layerId) const;      // "" = default font
    void setLayerFont (int layerId, const juce::String& family);
    juce::String getOverlayText() const { return getLayerText (4); }
    void setOverlayText (const juce::String& t) { setLayerText (4, t); }

    // Custom inline shader source (when layer2d.mode == 5 or custom override)
    juce::String getCustomShaderCode() const;
    void setCustomShaderCode (const juce::String& code);
    std::atomic<int> inlineShaderVersion { 1 };

    juce::var toVar() const;
    void fromVar (const juce::var& v);

private:
    void addParam (const juce::String& id, const juce::String& name, const juce::String& cat,
                   float minV, float maxV, float defV, float step = 0.0f);

    // Per-kind parameter sets, registered once for the legacy layer and for every extra slot
    void registerLayerParams (int layerId, LayerType t, const juce::String& prefix, const juce::String& category);
    void registerCommon (const juce::String& p, const juce::String& cat, float defEnabled, float defOpacity, float defBlend);
    void registerPost();
    void resetLayerParams (int layerId);
    void rebuildPrefixes();

    std::vector<ParamInfo> params;
    std::unordered_map<juce::String, int> idToIndex;
    std::array<std::atomic<float>, maxParams> baseValues {};
    std::array<std::atomic<float>, maxParams> modValues {};
    std::array<std::vector<int>, kMaxLayers> layerParams;
    int registeringLayer = -1, registeringType = -1;

    mutable juce::CriticalSection stateLock;
    LayerStack stack;
    std::array<int, kMaxLayers> layerTypes {};
    std::array<juce::String, kMaxLayers> prefixes, layerTexts, layerFonts;
    juce::String customShaderCode;
};
