#include "VisualState.h"

namespace
{
const char* defaultInlineShader = R"glsl(
// Custom GLSL Fragment Layer (hot-compiled!)
// Available uniforms: iResolution, iTime, u_rms, u_env, u_transient, u_centroid,
// u_bands[16], iChannel0 (feedback), iChannel1 (composited lower layers), iCam, iLoop
void mainImage(out vec4 o, in vec2 fc)
{
    vec2 uv = (fc - 0.5 * iResolution.xy) / iResolution.y;
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    float wave = sin(r * 18.0 - iTime * 4.0 + sin(a * 6.0 + iTime) * (1.0 + u_env * 4.0));
    float glow = 0.04 / (abs(r - 0.35 - 0.12 * u_bands[2] - 0.05 * wave) + 0.015);
    vec3 col = 0.5 + 0.5 * cos(iTime * 0.6 + uv.xyx * 3.0 + vec3(0.0, 2.0, 4.0) + u_centroid * 4.0);
    o = vec4(col * glow * (0.6 + u_transient * 1.4), clamp(glow, 0.0, 1.0));
}
)glsl";

const char* const kTypeNames[] = { "3D Scene", "2D Shader", "Video & Loop", "Webcam FX", "Text", "Spectrum", "Gradient / Fill" };
const char* const kExtraPrefix[] = { "scene", "shader", "video", "webcam", "text", "spectrum", "fill" };
const char* const kLegacyPrefix[] = { "scene", "layer2d", "video", "webcam", "text" };
const char* const kLegacyCategory[] = { "3D Scene", "2D Shader", "Video & Loop", "Webcam FX", "Text FX" };
} // namespace

VisualState::VisualState()
{
    params.reserve (maxParams);
    customShaderCode = defaultInlineShader;
    layerTypes.fill (-1);
    for (int i = 0; i < kLegacyLayers; ++i)
        layerTypes[(size_t) i] = i;

    // Legacy layers keep their original parameter ids ("scene.pulse", "layer2d.speed", ...)
    for (int i = 0; i < kLegacyLayers; ++i)
        registerLayerParams (i, (LayerType) i, kLegacyPrefix[i], kLegacyCategory[i]);

    // Extra slots: every kind is registered for every slot ("L3.scene.pulse"), only the chosen kind is shown/used
    for (int slot = 0; slot < kExtraLayers; ++slot)
    {
        const int id = kLegacyLayers + slot;
        for (int t = 0; t < (int) LayerType::Count; ++t)
            registerLayerParams (id, (LayerType) t,
                                 "L" + juce::String (slot + 1) + "." + kExtraPrefix[t],
                                 "Layer " + juce::String (slot + 1) + " - " + kTypeNames[t]);
    }
    registeringLayer = registeringType = -1;
    registerPost();

    resetAllToDefaults();
}

void VisualState::registerCommon (const juce::String& p, const juce::String& cat, float defEnabled, float defOpacity, float defBlend)
{
    addParam (p + ".enabled", "Enable",     cat, 0.0f, 1.0f, defEnabled, 1.0f);
    addParam (p + ".opacity", "Opacity",    cat, 0.0f, 1.0f, defOpacity);
    addParam (p + ".blend",   "Blend mode", cat, 0.0f, 5.0f, defBlend, 1.0f);
}

void VisualState::registerLayerParams (int layerId, LayerType t, const juce::String& p, const juce::String& cat)
{
    registeringLayer = layerId;
    registeringType = (int) t;
    const bool legacy = isLegacyLayer (layerId);

    switch (t)
    {
        case LayerType::Scene3D:
            registerCommon (p, cat, 1.0f, legacy ? 1.0f : 0.85f, legacy ? 0.0f : 1.0f);
            addParam (p + ".geometry",  "Shape",           cat, 0.0f, 11.0f, 0.0f, 1.0f);
            addParam (p + ".layout",    "Pattern",         cat, 0.0f, 10.0f, 0.0f, 1.0f);
            addParam (p + ".instances", "Copies",          cat, 16.0f, 1024.0f, 512.0f, 1.0f);
            addParam (p + ".pulse",     "Audio pulse",     cat, 0.0f, 3.0f, 1.0f);
            addParam (p + ".scale",     "Size",            cat, 0.1f, 3.0f, 1.0f);
            addParam (p + ".spread",    "Spread",          cat, 0.1f, 3.0f, 1.0f);
            addParam (p + ".spinX",     "Spin X",          cat, -3.0f, 3.0f, 0.05f);
            addParam (p + ".spinY",     "Spin Y",          cat, -3.0f, 3.0f, 0.25f);
            addParam (p + ".spinZ",     "Spin Z",          cat, -3.0f, 3.0f, 0.0f);
            addParam (p + ".camdist",   "Camera distance", cat, 2.0f, 25.0f, 9.0f);
            addParam (p + ".camOrbit",  "Camera orbit",    cat, -3.14f, 3.14f, 0.0f);
            addParam (p + ".camHeight", "Camera height",   cat, -10.0f, 10.0f, 4.5f);
            addParam (p + ".hue",       "Hue",             cat, 0.0f, 1.0f, 0.58f);
            addParam (p + ".saturation","Saturation",      cat, 0.0f, 1.0f, 0.75f);
            addParam (p + ".metallic",  "Metallic",        cat, 0.0f, 1.0f, 0.8f);
            addParam (p + ".roughness", "Roughness",       cat, 0.04f, 1.0f, 0.28f);
            addParam (p + ".emissive",  "Glow",            cat, 0.0f, 6.0f, 1.6f);
            addParam (p + ".wireframe", "Wireframe",       cat, 0.0f, 1.0f, 0.0f, 1.0f);
            break;

        case LayerType::Shader2D:
            registerCommon (p, cat, 1.0f, legacy ? 0.65f : 0.85f, legacy ? 0.0f : 1.0f);
            addParam (p + ".mode",       "Generator",     cat, 0.0f, 12.0f, 0.0f, 1.0f);
            addParam (p + ".speed",      "Speed",         cat, 0.0f, 4.0f, 1.0f);
            addParam (p + ".scale",      "Pattern scale", cat, 0.1f, 5.0f, 1.2f);
            addParam (p + ".complexity", "Complexity",    cat, 0.0f, 1.0f, 0.5f);
            addParam (p + ".colorShift", "Palette shift", cat, 0.0f, 1.0f, 0.2f);
            addParam (p + ".paramA",     "Morph A",       cat, 0.0f, 1.0f, 0.5f);
            addParam (p + ".paramB",     "Morph B",       cat, 0.0f, 1.0f, 0.5f);
            break;

        case LayerType::VideoLoop:
            registerCommon (p, cat, legacy ? 0.0f : 1.0f, legacy ? 0.75f : 0.85f, 1.0f);
            addParam (p + ".source",    "Source",              cat, 0.0f, 4.0f, 0.0f, 1.0f);
            addParam (p + ".speed",     "Speed",               cat, -3.0f, 3.0f, 1.0f);
            addParam (p + ".tempoSync", "Sync to host tempo",  cat, 0.0f, 1.0f, 0.0f, 1.0f);
            addParam (p + ".syncBeats", "Loop length (beats)", cat, 1.0f, 16.0f, 4.0f, 1.0f);
            addParam (p + ".inPoint",   "Loop start",          cat, 0.0f, 0.95f, 0.0f);
            addParam (p + ".outPoint",  "Loop end",            cat, 0.05f, 1.0f, 1.0f);
            addParam (p + ".reverse",   "Reverse",             cat, 0.0f, 1.0f, 0.0f, 1.0f);
            addParam (p + ".stutter",   "Beat stutter",        cat, 0.0f, 1.0f, 0.0f);
            addParam (p + ".zoom",      "Zoom",                cat, 0.25f, 4.0f, 1.0f);
            addParam (p + ".tintHue",   "Tint hue",            cat, 0.0f, 1.0f, 0.0f);
            break;

        case LayerType::WebcamFX:
            registerCommon (p, cat, legacy ? 0.0f : 1.0f, legacy ? 0.8f : 0.85f, legacy ? 0.0f : 1.0f);
            addParam (p + ".mirror",     "Mirror horizontally", cat, 0.0f, 1.0f, 1.0f, 1.0f);
            addParam (p + ".edgeDetect", "Neon edges",          cat, 0.0f, 1.0f, 0.4f);
            addParam (p + ".posterize",  "Posterize",           cat, 0.0f, 1.0f, 0.0f);
            addParam (p + ".chromaKey",  "Luma / chroma key",   cat, 0.0f, 1.0f, 0.15f);
            addParam (p + ".hueShift",   "Hue shift",           cat, 0.0f, 1.0f, 0.0f);
            addParam (p + ".uvDisplace", "Audio warp",          cat, 0.0f, 1.0f, 0.25f);
            addParam (p + ".scale",      "Scale",               cat, 0.25f, 3.0f, 1.0f);
            break;

        case LayerType::Text:
            registerCommon (p, cat, legacy ? 0.0f : 1.0f, legacy ? 0.9f : 0.9f, legacy ? 0.0f : 1.0f);
            addParam (p + ".size",        "Size",         cat, 0.2f, 3.0f, 1.0f);
            addParam (p + ".posX",        "Position X",   cat, -1.0f, 1.0f, 0.0f);
            addParam (p + ".posY",        "Position Y",   cat, -1.0f, 1.0f, 0.0f);
            addParam (p + ".rotation",    "Rotation",     cat, -3.14f, 3.14f, 0.0f);
            addParam (p + ".extrude",     "3D depth",     cat, 0.0f, 1.0f, 0.35f);
            addParam (p + ".glitch",      "Slice glitch", cat, 0.0f, 1.0f, 0.15f);
            addParam (p + ".hue",         "Colour hue",   cat, 0.0f, 1.0f, 0.1f);
            addParam (p + ".audioBounce", "Audio bounce", cat, 0.0f, 2.0f, 0.5f);
            addParam (p + ".bold",        "Bold",         cat, 0.0f, 1.0f, 1.0f, 1.0f);
            addParam (p + ".italic",      "Italic",       cat, 0.0f, 1.0f, 0.0f, 1.0f);
            break;

        case LayerType::Spectrum:
            registerCommon (p, cat, 1.0f, 0.85f, 1.0f);
            addParam (p + ".style",     "Style",          cat, 0.0f, 4.0f, 0.0f, 1.0f);
            addParam (p + ".hue",       "Hue",            cat, 0.0f, 1.0f, 0.55f);
            addParam (p + ".hueSpread", "Hue spread",     cat, 0.0f, 1.0f, 0.35f);
            addParam (p + ".gain",      "Gain",           cat, 0.2f, 4.0f, 1.4f);
            addParam (p + ".thickness", "Thickness",      cat, 0.05f, 1.0f, 0.6f);
            addParam (p + ".size",      "Size",           cat, 0.2f, 2.0f, 1.0f);
            addParam (p + ".posY",      "Vertical position", cat, -1.0f, 1.0f, -0.6f);
            addParam (p + ".rotation",  "Rotation",       cat, -3.14f, 3.14f, 0.0f);
            addParam (p + ".glow",      "Glow",           cat, 0.0f, 2.0f, 0.8f);
            break;

        case LayerType::Fill:
            registerCommon (p, cat, 1.0f, 0.7f, 3.0f); // Overlay: tints what's below instead of washing it out
            addParam (p + ".style",      "Style",         cat, 0.0f, 6.0f, 1.0f, 1.0f);
            addParam (p + ".hueA",       "Colour A hue",  cat, 0.0f, 1.0f, 0.62f);
            addParam (p + ".hueB",       "Colour B hue",  cat, 0.0f, 1.0f, 0.92f);
            addParam (p + ".saturation", "Saturation",    cat, 0.0f, 1.0f, 0.8f);
            addParam (p + ".brightness", "Brightness",    cat, 0.0f, 2.0f, 0.8f);
            addParam (p + ".angle",      "Angle",         cat, -3.14f, 3.14f, 0.0f);
            addParam (p + ".scale",      "Scale",         cat, 0.2f, 8.0f, 1.0f);
            addParam (p + ".speed",      "Scroll / spin speed", cat, -2.0f, 2.0f, 0.2f);
            addParam (p + ".audio",      "Audio reaction", cat, 0.0f, 2.0f, 0.5f);
            break;

        default: break;
    }
}

void VisualState::registerPost()
{
    // ---- Post-processing & master compositor ----
    addParam ("post.feedback",       "Feedback Amount",   "Post FX", 0.0f, 0.98f, 0.72f);
    addParam ("post.feedbackZoom",   "Feedback Zoom",     "Post FX", -0.1f, 0.1f, 0.012f);
    addParam ("post.feedbackRotate", "Feedback Spin",     "Post FX", -0.15f, 0.15f, 0.004f);
    addParam ("post.bloom",          "Bloom Intensity",   "Post FX", 0.0f, 3.0f, 0.75f);
    addParam ("post.threshold",      "Bloom Threshold",   "Post FX", 0.1f, 2.0f, 0.75f);
    addParam ("post.kaleidoscope",   "Kaleidoscope Sides","Post FX", 0.0f, 16.0f, 0.0f, 1.0f);
    addParam ("post.mirrorQuad",     "Quad Mirror",       "Post FX", 0.0f, 1.0f, 0.0f, 1.0f);
    addParam ("post.rgbSplit",       "Chromatic RGB Split","Post FX", 0.0f, 1.0f, 0.08f);
    addParam ("post.glitch",         "Digital Glitch",    "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.pixelate",       "Pixelate / Mosaic", "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.scanlines",      "CRT Scanlines",     "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.vignette",       "Vignette",          "Post FX", 0.0f, 1.0f, 0.25f);
    addParam ("post.hueShift",       "Master Hue Shift",  "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.saturation",     "Master Saturation", "Post FX", 0.0f, 2.0f, 1.1f);
    addParam ("post.contrast",       "Master Contrast",   "Post FX", 0.5f, 2.5f, 1.05f);
    addParam ("post.strobe",         "White/Inv Strobe",  "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.tonemap",        "ACES Tonemap",      "Post FX", 0.0f, 1.0f, 1.0f, 1.0f);
    addParam ("post.masterDimmer",   "Master Output",     "Post FX", 0.0f, 1.5f, 1.0f);
    addParam ("post.twirl",          "Twirl / Swirl",     "Post FX", -1.0f, 1.0f, 0.0f);
    addParam ("post.ripple",         "Ripple Warp",       "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.zoomBlur",       "Zoom Blur",         "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.grain",          "Film Grain",        "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.posterize",      "Posterize Colours", "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.duotone",        "Duotone Mix",       "Post FX", 0.0f, 1.0f, 0.0f);
    addParam ("post.duoHueA",        "Duotone Shadow Hue","Post FX", 0.0f, 1.0f, 0.62f);
    addParam ("post.duoHueB",        "Duotone Light Hue", "Post FX", 0.0f, 1.0f, 0.06f);

    addParam ("global.speed",        "Animation Speed",   "Global", 0.0f, 4.0f, 1.0f);
}

void VisualState::addParam (const juce::String& id, const juce::String& name, const juce::String& cat,
                            float minV, float maxV, float defV, float step)
{
    const int idx = (int) params.size();
    jassert (idx < (int) maxParams);
    if (idx >= (int) maxParams)
        return;
    ParamInfo info { id, name, cat, minV, maxV, defV, step, registeringLayer, registeringType };
    params.push_back (std::move (info));
    idToIndex[id] = idx;
    baseValues[(size_t) idx].store (defV, std::memory_order_relaxed);
    modValues[(size_t) idx].store (defV, std::memory_order_relaxed);
    if (registeringLayer >= 0)
        layerParams[(size_t) registeringLayer].push_back (idx);
}

int VisualState::findParam (const juce::String& id) const
{
    const auto it = idToIndex.find (id);
    return it != idToIndex.end() ? it->second : -1;
}

// ---------------------------------------------------------------------------------------------
// Layers
// ---------------------------------------------------------------------------------------------
void VisualState::rebuildPrefixes()
{
    for (int i = 0; i < kMaxLayers; ++i)
    {
        const int t = layerTypes[(size_t) i];
        if (t < 0)
            prefixes[(size_t) i] = {};
        else if (isLegacyLayer (i))
            prefixes[(size_t) i] = kLegacyPrefix[i];
        else
            prefixes[(size_t) i] = "L" + juce::String (i - kLegacyLayers + 1) + "." + kExtraPrefix[t];
    }
}

void VisualState::resetLayerParams (int layerId)
{
    for (int idx : layerParams[(size_t) layerId])
    {
        const float d = params[(size_t) idx].defaultVal;
        baseValues[(size_t) idx].store (d, std::memory_order_relaxed);
        modValues[(size_t) idx].store (d, std::memory_order_relaxed);
    }
}

void VisualState::resetAllToDefaults()
{
    for (int i = 0; i < (int) params.size(); ++i)
    {
        baseValues[(size_t) i].store (params[(size_t) i].defaultVal, std::memory_order_relaxed);
        modValues[(size_t) i].store (params[(size_t) i].defaultVal, std::memory_order_relaxed);
    }
    {
        const juce::ScopedLock sl (stateLock);
        for (int i = 0; i < kMaxLayers; ++i)
        {
            layerTypes[(size_t) i] = isLegacyLayer (i) ? i : -1;
            layerTexts[(size_t) i] = i == 4 ? juce::String ("AUDIO VISUALIZER") : juce::String ("TEXT");
            layerFonts[(size_t) i] = {};
        }
        stack.count = kLegacyLayers;
        for (int i = 0; i < kLegacyLayers; ++i)
            stack.ids[(size_t) i] = i;
        rebuildPrefixes();
        if (customShaderCode != defaultInlineShader)
        {
            customShaderCode = defaultInlineShader;
            ++inlineShaderVersion;
        }
    }
    ++layerRevision;
}

VisualState::LayerStack VisualState::getLayerStack() const
{
    const juce::ScopedLock sl (stateLock);
    return stack;
}

void VisualState::setLayerStack (const LayerStack& s)
{
    {
        const juce::ScopedLock sl (stateLock);
        stack = s;
    }
    ++layerRevision;
}

void VisualState::moveLayer (int fromSlot, int toSlot)
{
    const juce::ScopedLock sl (stateLock);
    if (fromSlot < 0 || fromSlot >= stack.count || toSlot < 0 || toSlot >= stack.count)
        return;
    std::swap (stack.ids[(size_t) fromSlot], stack.ids[(size_t) toSlot]);
}

int VisualState::addLayer (LayerType t)
{
    int id = -1;
    {
        const juce::ScopedLock sl (stateLock);
        for (int i = kLegacyLayers; i < kMaxLayers; ++i)
            if (layerTypes[(size_t) i] < 0)
            {
                id = i;
                break;
            }
        if (id < 0 || stack.count >= kMaxLayers)
            return -1;
        layerTypes[(size_t) id] = (int) t;
        layerTexts[(size_t) id] = "TEXT";
        layerFonts[(size_t) id] = {};
        stack.ids[(size_t) stack.count++] = id;
        rebuildPrefixes();
    }
    resetLayerParams (id);
    ++layerRevision;
    return id;
}

void VisualState::removeLayer (int layerId)
{
    if (isLegacyLayer (layerId) || layerId < 0 || layerId >= kMaxLayers)
        return;
    {
        const juce::ScopedLock sl (stateLock);
        int w = 0;
        for (int i = 0; i < stack.count; ++i)
            if (stack.ids[(size_t) i] != layerId)
                stack.ids[(size_t) w++] = stack.ids[(size_t) i];
        stack.count = w;
        layerTypes[(size_t) layerId] = -1;
        rebuildPrefixes();
    }
    resetLayerParams (layerId);
    ++layerRevision;
}

bool VisualState::isLayerActive (int layerId) const
{
    if (layerId < 0 || layerId >= kMaxLayers)
        return false;
    const juce::ScopedLock sl (stateLock);
    return layerTypes[(size_t) layerId] >= 0;
}

LayerType VisualState::getLayerType (int layerId) const
{
    if (layerId < 0 || layerId >= kMaxLayers)
        return LayerType::Count;
    const juce::ScopedLock sl (stateLock);
    const int t = layerTypes[(size_t) layerId];
    return t >= 0 ? (LayerType) t : LayerType::Count;
}

juce::String VisualState::getLayerPrefix (int layerId) const
{
    if (layerId < 0 || layerId >= kMaxLayers)
        return {};
    const juce::ScopedLock sl (stateLock);
    return prefixes[(size_t) layerId];
}

juce::String VisualState::getLayerName (int layerId) const
{
    const auto t = getLayerType (layerId);
    if (t == LayerType::Count)
        return {};
    juce::String n = kTypeNames[(int) t];
    if (! isLegacyLayer (layerId))
        n += " (L" + juce::String (layerId - kLegacyLayers + 1) + ")";
    return n;
}

bool VisualState::isParamAvailable (int idx) const
{
    if (idx < 0 || idx >= (int) params.size())
        return false;
    const auto& info = params[(size_t) idx];
    if (info.layerId < 0 || isLegacyLayer (info.layerId))
        return true;
    return (int) getLayerType (info.layerId) == info.layerType;
}

juce::String VisualState::getLayerText (int layerId) const
{
    if (layerId < 0 || layerId >= kMaxLayers)
        return {};
    const juce::ScopedLock sl (stateLock);
    return layerTexts[(size_t) layerId];
}

void VisualState::setLayerText (int layerId, const juce::String& t)
{
    if (layerId < 0 || layerId >= kMaxLayers)
        return;
    const juce::ScopedLock sl (stateLock);
    layerTexts[(size_t) layerId] = t;
}

juce::String VisualState::getLayerFont (int layerId) const
{
    if (layerId < 0 || layerId >= kMaxLayers)
        return {};
    const juce::ScopedLock sl (stateLock);
    return layerFonts[(size_t) layerId];
}

void VisualState::setLayerFont (int layerId, const juce::String& family)
{
    if (layerId < 0 || layerId >= kMaxLayers)
        return;
    const juce::ScopedLock sl (stateLock);
    layerFonts[(size_t) layerId] = family;
}

juce::String VisualState::getCustomShaderCode() const
{
    const juce::ScopedLock sl (stateLock);
    return customShaderCode;
}

void VisualState::setCustomShaderCode (const juce::String& code)
{
    // Unchanged code must not bump the version: that forces a GLSL recompile on the GL thread.
    {
        const juce::ScopedLock sl (stateLock);
        if (customShaderCode == code)
            return;
        customShaderCode = code;
    }
    ++inlineShaderVersion;
}

// ---------------------------------------------------------------------------------------------
// State (presets, undo, DAW session)
// ---------------------------------------------------------------------------------------------
juce::var VisualState::toVar() const
{
    auto* root = new juce::DynamicObject();
    auto* pObj = new juce::DynamicObject();
    for (int i = 0; i < (int) params.size(); ++i)
    {
        const auto& info = params[(size_t) i];
        // Only active extra layers are written, and only values that differ from the default
        if (! isLegacyLayer (info.layerId) && info.layerId >= 0)
            if (! isParamAvailable (i) || getBase (i) == info.defaultVal)
                continue;
        pObj->setProperty (info.id, (double) getBase (i));
    }
    root->setProperty ("params", juce::var (pObj));

    const auto st = getLayerStack();
    juce::Array<juce::var> ord, slots;
    for (int i = 0; i < st.count; ++i)
        ord.add (st.ids[(size_t) i]);
    root->setProperty ("layerOrder", ord);

    auto* texts = new juce::DynamicObject();
    auto* fonts = new juce::DynamicObject();
    for (int id = 0; id < kMaxLayers; ++id)
    {
        const auto t = getLayerType (id);
        if (t == LayerType::Count)
            continue;
        if (! isLegacyLayer (id))
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", id);
            o->setProperty ("type", (int) t);
            slots.add (juce::var (o));
        }
        if (t == LayerType::Text)
        {
            texts->setProperty (juce::String (id), getLayerText (id));
            fonts->setProperty (juce::String (id), getLayerFont (id));
        }
    }
    root->setProperty ("layerSlots", slots);
    root->setProperty ("layerTexts", juce::var (texts));
    root->setProperty ("layerFonts", juce::var (fonts));
    root->setProperty ("customShaderCode", getCustomShaderCode());
    root->setProperty ("overlayText", getOverlayText()); // kept for older readers
    return juce::var (root);
}

void VisualState::fromVar (const juce::var& v)
{
    if (! v.isObject())
        return;

    // Reset everything except the shader code, which is set exactly once below so that loading
    // a state with the same code doesn't trigger a recompile.
    for (int i = 0; i < (int) params.size(); ++i)
    {
        baseValues[(size_t) i].store (params[(size_t) i].defaultVal, std::memory_order_relaxed);
        modValues[(size_t) i].store (params[(size_t) i].defaultVal, std::memory_order_relaxed);
    }
    setCustomShaderCode (v.hasProperty ("customShaderCode") ? v.getProperty ("customShaderCode", "").toString()
                                                            : juce::String (defaultInlineShader));

    {
        const juce::ScopedLock sl (stateLock);
        for (int i = 0; i < kMaxLayers; ++i)
        {
            layerTypes[(size_t) i] = isLegacyLayer (i) ? i : -1;
            layerTexts[(size_t) i] = i == 4 ? juce::String ("AUDIO VISUALIZER") : juce::String ("TEXT");
            layerFonts[(size_t) i] = {};
        }

        // Extra layers and their kinds
        if (auto* slots = v.getProperty ("layerSlots", juce::var()).getArray())
            for (const auto& s : *slots)
            {
                const int id = (int) s.getProperty ("id", -1), t = (int) s.getProperty ("type", -1);
                if (id >= kLegacyLayers && id < kMaxLayers && t >= 0 && t < (int) LayerType::Count)
                    layerTypes[(size_t) id] = t;
            }

        // Stack order: keep valid, distinct ids; make sure every active layer is present
        LayerStack ns;
        unsigned seen = 0;
        if (auto* ord = v.getProperty ("layerOrder", juce::var()).getArray())
            for (const auto& o : *ord)
            {
                const int id = (int) o;
                if (id >= 0 && id < kMaxLayers && layerTypes[(size_t) id] >= 0 && ! (seen & (1u << id)))
                {
                    ns.ids[(size_t) ns.count++] = id;
                    seen |= 1u << id;
                }
            }
        std::array<int, kMaxLayers> missing {};
        int nMissing = 0;
        for (int id = 0; id < kMaxLayers; ++id)
            if (layerTypes[(size_t) id] >= 0 && ! (seen & (1u << id)))
                missing[(size_t) nMissing++] = id;
        // missing layers go to the bottom (legacy layers are listed first, so they stay below extras)
        std::array<int, kMaxLayers> merged {};
        int n = 0;
        for (int i = 0; i < nMissing; ++i) merged[(size_t) n++] = missing[(size_t) i];
        for (int i = 0; i < ns.count; ++i) merged[(size_t) n++] = ns.ids[(size_t) i];
        stack.ids = merged;
        stack.count = n;

        if (v.hasProperty ("overlayText"))
            layerTexts[4] = v.getProperty ("overlayText", "AUDIO VISUALIZER").toString();
        if (auto* to = v.getProperty ("layerTexts", juce::var()).getDynamicObject())
            for (const auto& p : to->getProperties())
            {
                const int id = p.name.toString().getIntValue();
                if (id >= 0 && id < kMaxLayers)
                    layerTexts[(size_t) id] = p.value.toString();
            }
        if (auto* fo = v.getProperty ("layerFonts", juce::var()).getDynamicObject())
            for (const auto& p : fo->getProperties())
            {
                const int id = p.name.toString().getIntValue();
                if (id >= 0 && id < kMaxLayers)
                    layerFonts[(size_t) id] = p.value.toString();
            }
        rebuildPrefixes();
    }

    if (auto* pObj = v.getProperty ("params", juce::var()).getDynamicObject())
        for (const auto& prop : pObj->getProperties())
            setBase (prop.name.toString(), (float) (double) prop.value);

    ++layerRevision;
}
