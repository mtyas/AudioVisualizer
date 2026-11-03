#include "PresetManager.h"
#include "../core/Engine.h"

namespace
{
struct ParamKV { const char* id; float val; };

void applyParams (VisualState& vs, std::initializer_list<ParamKV> list)
{
    for (const auto& kv : list)
        vs.setBase (kv.id, kv.val);
}
} // namespace

void PresetManager::initialise()
{
    buildFactoryPresets();
    refreshUserPresets();
}

juce::File PresetManager::getUserPresetsFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("AudioVisualizer")
                   .getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::String PresetManager::getCurrentPresetName() const
{
    if (currentIndex >= 0 && currentIndex < (int) presets.size())
        return presets[(size_t) currentIndex].name;
    return "Init";
}

void PresetManager::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size())
        return;
    currentIndex = index;
    const auto& p = presets[(size_t) index];
    engine.performUndoable ("Load Preset: " + p.name, [&]
    {
        engine.loadFullState (p.stateData);
    });
    engine.setStatus ("Loaded preset: " + p.name + " [" + p.category + "]");
}

void PresetManager::nextPreset()
{
    if (! presets.empty())
        loadPreset ((currentIndex + 1) % (int) presets.size());
}

void PresetManager::prevPreset()
{
    if (! presets.empty())
        loadPreset ((currentIndex - 1 + (int) presets.size()) % (int) presets.size());
}

bool PresetManager::saveUserPreset (const juce::String& name, const juce::String& category,
                                    const juce::String& tags, const juce::String& description)
{
    if (name.trim().isEmpty())
        return false;

    auto state = engine.saveFullState();
    if (auto* o = state.getDynamicObject())
    {
        o->setProperty ("presetName", name.trim());
        o->setProperty ("presetCategory", category.isNotEmpty() ? category : "User");
        o->setProperty ("presetAuthor", "User");
        o->setProperty ("presetTags", tags);
        o->setProperty ("presetDescription", description);
    }

    const auto safeName = juce::File::createLegalFileName (name.trim()) + ".avpreset";
    const auto file = getUserPresetsFolder().getChildFile (safeName);
    if (! file.replaceWithText (juce::JSON::toString (state, true)))
        return false;

    refreshUserPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].file == file)
            currentIndex = i;
    engine.setStatus ("Saved preset: " + name);
    return true;
}

void PresetManager::refreshUserPresets()
{
    presets.erase (std::remove_if (presets.begin(), presets.end(),
                                   [] (const PresetEntry& p) { return ! p.isFactory; }),
                   presets.end());

    const auto folder = getUserPresetsFolder();
    for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.avpreset;*.json"))
    {
        const auto f = entry.getFile();
        const auto v = juce::JSON::parse (f);
        if (! v.isObject())
            continue;
        PresetEntry pe;
        pe.isFactory   = false;
        pe.file        = f;
        pe.name        = v.getProperty ("presetName", f.getFileNameWithoutExtension()).toString();
        pe.category    = v.getProperty ("presetCategory", "User").toString();
        pe.author      = v.getProperty ("presetAuthor", "User").toString();
        pe.tags        = v.getProperty ("presetTags", "user, custom").toString();
        pe.description = v.getProperty ("presetDescription", "User saved preset").toString();
        pe.stateData   = v;
        presets.push_back (std::move (pe));
    }
}

void PresetManager::buildFactoryPresets()
{
    presets.clear();

    auto makePreset = [this] (const char* name, const char* category, const char* tags,
                              const char* desc, std::function<void()> setup)
    {
        engine.visual.resetAllToDefaults();
        engine.modulation.resetToDefaultRoutes();
        engine.graph.loadDefault();
        setup();
        PresetEntry pe;
        pe.name        = name;
        pe.category    = category;
        pe.author      = "Factory";
        pe.tags        = tags;
        pe.description = desc;
        pe.isFactory   = true;
        pe.stateData   = engine.saveFullState();
        presets.push_back (std::move (pe));
    };

    // 1. Phyllotaxis Audio Supernova
    makePreset ("01. Phyllotaxis Audio Supernova", "3D & Geometry", "3d, spiral, bloom, bass, default",
        "1024 instanced PBR spheres in a golden-angle spiral pulsing with 16 FFT bands, plasma backdrop, and transient HDR bloom.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 0.0f }, { "scene.layout", 0.0f },
                { "scene.instances", 768.0f }, { "scene.pulse", 1.15f }, { "scene.spread", 1.05f },
                { "scene.hue", 0.58f }, { "scene.emissive", 1.8f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 0.0f }, { "layer2d.opacity", 0.55f },
                { "post.feedback", 0.76f }, { "post.bloom", 0.85f }, { "post.rgbSplit", 0.10f }
            });
        });

    // 2. Cyberpunk Glitch Cathedral
    makePreset ("02. Cyberpunk Glitch Cathedral", "3D & Geometry", "3d, cube, grid, glitch, cyberpunk, sequencer",
        "Architectural 3D cube lattice with neon wireframe highlights, step-sequenced camera cuts, CRT scanlines, and transient digital glitch.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 1.0f }, { "scene.layout", 1.0f },
                { "scene.instances", 512.0f }, { "scene.pulse", 1.4f }, { "scene.spread", 1.35f },
                { "scene.hue", 0.85f }, { "scene.metallic", 0.92f }, { "scene.roughness", 0.18f },
                { "scene.emissive", 2.4f }, { "scene.camdist", 7.5f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 2.0f }, { "layer2d.opacity", 0.5f },
                { "post.feedback", 0.65f }, { "post.bloom", 1.1f }, { "post.glitch", 0.25f },
                { "post.rgbSplit", 0.28f }, { "post.scanlines", 0.45f }, { "post.contrast", 1.25f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "audio.transient", "", "post.glitch", 0.65f, 0.8f, false, 0 });
            engine.modulation.addRoute ({ true, "seq.1",           "", "scene.camdist", -0.35f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "audio.bass",      "", "scene.pulse", 0.6f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "lfo.2",           "", "scene.hue", 0.25f, 1.0f, true, 0 });
        });

    // 3. Sacred Kaleido Mandala
    makePreset ("03. Sacred Kaleido Mandala", "Shaders & 2D", "2d, kaleidoscope, mandala, sacred, fractal, chaos",
        "Hypnotic 2D sacred geometry generator routed through an 8-sided mirror kaleidoscope and Lorenz strange-attractor color modulation.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 2.0f }, { "scene.layout", 2.0f },
                { "scene.instances", 256.0f }, { "scene.opacity", 0.65f }, { "scene.hue", 0.12f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 1.0f }, { "layer2d.opacity", 0.95f },
                { "layer2d.scale", 1.6f }, { "layer2d.complexity", 0.78f },
                { "post.kaleidoscope", 8.0f }, { "post.feedback", 0.82f },
                { "post.feedbackRotate", 0.015f }, { "post.bloom", 0.95f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "audio.env",       "", "layer2d.scale", 0.45f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "chaos.lorenzX",   "", "layer2d.colorShift", 0.75f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "audio.transient", "", "post.feedbackZoom", 0.35f, 1.0f, false, 0 });
        });

    // 4. Liquid Plasma Feedback Tunnel
    makePreset ("04. Liquid Plasma Feedback Tunnel", "Shaders & 2D", "2d, plasma, feedback, tunnel, psychedelic, lfo",
        "Deep recursive zoom-and-spin feedback vortex driven by audio envelope and dual LFOs over a domain-warped plasma shader.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 0.0f }, { "layer2d.opacity", 0.9f },
                { "layer2d.speed", 1.4f }, { "layer2d.scale", 1.8f }, { "layer2d.complexity", 0.85f },
                { "post.feedback", 0.91f }, { "post.feedbackZoom", 0.032f },
                { "post.feedbackRotate", 0.018f }, { "post.bloom", 0.8f }, { "post.saturation", 1.35f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "audio.bass",      "", "post.feedbackZoom", 0.4f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "lfo.1",           "", "post.feedbackRotate", 0.5f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "audio.centroid",  "", "post.hueShift", 0.6f, 1.0f, false, 0 });
        });

    // 5. Webcam Neon Ghost Trails
    makePreset ("05. Webcam Neon Ghost Trails", "Webcam & Interactive", "webcam, camera, neon, edge, motion, interactive",
        "Turns your live webcam feed into glowing neon edge contours with audio-reactive UV displacement and long ghost feedback trails.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.opacity", 0.45f }, { "scene.geometry", 3.0f },
                { "scene.layout", 3.0f }, { "scene.instances", 256.0f },
                { "layer2d.enabled", 0.0f },
                { "webcam.enabled", 1.0f }, { "webcam.opacity", 1.0f }, { "webcam.blend", 0.0f },
                { "webcam.edgeDetect", 0.85f }, { "webcam.uvDisplace", 0.45f }, { "webcam.chromaKey", 0.25f },
                { "post.feedback", 0.88f }, { "post.feedbackZoom", 0.015f }, { "post.bloom", 1.15f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "cam.motion",      "", "post.bloom", 0.65f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "cam.x",           "", "scene.camOrbit", 0.8f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "cam.y",           "", "scene.camHeight", 0.7f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "audio.env",       "", "webcam.uvDisplace", 0.6f, 1.0f, false, 0 });
        });

    // 6. Webcam Body-Tracked Gravity Well
    makePreset ("06. Webcam Body-Tracked Gravity Well", "Webcam & Interactive", "webcam, body, gesture, torus, interactive",
        "Follows your body/hand X-Y centroid and motion energy from the webcam to steer a 3D torus ring and kaleidoscopic mirror.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 2.0f }, { "scene.layout", 2.0f },
                { "scene.instances", 512.0f }, { "scene.spread", 1.2f }, { "scene.emissive", 2.2f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 3.0f }, { "layer2d.opacity", 0.4f },
                { "webcam.enabled", 1.0f }, { "webcam.opacity", 0.75f }, { "webcam.posterize", 0.55f },
                { "webcam.edgeDetect", 0.5f },
                { "post.feedback", 0.80f }, { "post.mirrorQuad", 1.0f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "cam.x",      "", "scene.spinY", 1.0f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "cam.y",      "", "scene.spread", 0.7f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "cam.motion", "", "scene.pulse", 0.9f, 1.0f, false, 0 });
        });

    // 7. Live Loop Stutter & Reverse VJ
    makePreset ("07. Live Loop Stutter & Reverse VJ", "Live Looper & Glitch", "video, looper, stutter, reverse, vj, beat",
        "Samples live camera or synth deck frames into the GPU ring buffer and scrubs speed, stutter, and reverse via the 16-step sequencer.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.opacity", 0.5f }, { "scene.geometry", 1.0f },
                { "layer2d.enabled", 0.0f },
                { "video.enabled", 1.0f }, { "video.opacity", 0.95f }, { "video.source", 0.0f },
                { "video.speed", 1.0f }, { "video.stutter", 0.35f },
                { "webcam.enabled", 1.0f }, { "webcam.opacity", 0.45f },
                { "post.feedback", 0.70f }, { "post.rgbSplit", 0.22f }, { "post.glitch", 0.18f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "seq.1",           "", "video.speed", 0.8f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "audio.transient", "", "video.stutter", 0.75f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "audio.bass",      "", "video.zoom", 0.45f, 1.0f, false, 0 });
        });

    // 8. Hyperspace Starfield Warp
    makePreset ("08. Hyperspace Starfield Warp", "3D & Geometry", "starfield, space, icosahedron, helix, warp, subbass",
        "High-velocity 2D volumetric starfield combined with a rotating 3D icosahedron DNA helix driven by sub-bass and high-mid sparkle.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 3.0f }, { "scene.layout", 3.0f },
                { "scene.instances", 640.0f }, { "scene.hue", 0.62f }, { "scene.emissive", 2.5f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 4.0f }, { "layer2d.opacity", 0.85f },
                { "layer2d.speed", 1.8f },
                { "post.feedback", 0.78f }, { "post.feedbackZoom", 0.025f }, { "post.bloom", 1.0f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "audio.subBass",    "", "layer2d.speed", 0.85f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "audio.brilliance", "", "post.bloom", 0.65f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "lfo.1",            "", "scene.camOrbit", 0.6f, 1.0f, true, 0 });
        });

    // 9. Reaction-Diffusion Bio-Morph
    makePreset ("09. Reaction-Diffusion Bio-Morph", "Chaos & Generative", "cellular, voronoi, organic, rossler, chaos",
        "Organic cellular Voronoi membranes morphing under Rossler & Duffing chaos attractors with spectral-flux spore bursts.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 0.0f }, { "scene.layout", 4.0f },
                { "scene.instances", 512.0f }, { "scene.hue", 0.34f }, { "scene.roughness", 0.15f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 3.0f }, { "layer2d.opacity", 0.85f },
                { "layer2d.scale", 2.2f }, { "layer2d.complexity", 0.9f },
                { "post.feedback", 0.84f }, { "post.bloom", 0.7f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "chaos.rossler", "", "layer2d.paramA", 0.8f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "chaos.duffing", "", "layer2d.paramB", 0.8f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "audio.flux",    "", "scene.pulse", 0.9f, 1.0f, false, 0 });
        });

    // 10. MIDI Polyphonic Laser Prism
    makePreset ("10. MIDI Polyphonic Laser Prism", "MIDI Performance", "midi, synth, notes, pitchbend, laser, prism",
        "Designed for live MIDI keyboard/pad performance: note pitch shifts color spectrum, velocity fires emissive lasers, pitchbend warps space.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 3.0f }, { "scene.layout", 2.0f },
                { "scene.instances", 512.0f }, { "scene.emissive", 2.2f }, { "scene.wireframe", 1.0f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 1.0f }, { "layer2d.opacity", 0.7f },
                { "post.kaleidoscope", 6.0f }, { "post.feedback", 0.85f }, { "post.bloom", 1.2f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "midi.note",      "", "scene.hue", 0.95f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "midi.velocity",  "", "scene.emissive", 0.85f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "midi.pitchbend", "", "post.feedbackRotate", 0.75f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "midi.trigger",   "", "post.rgbSplit", 0.65f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "env.3",          "", "scene.pulse", 0.8f, 1.0f, false, 0 });
        });

    // 11. Brutalist Typography Bass Riot
    makePreset ("11. Brutalist Typography Bass Riot", "Text & VJ", "text, typography, brutalist,vj, bass, glitch",
        "Massive 3D-extruded kinetic text layer that bounces and slices on drum transients over a high-contrast wireframe grid.",
        [this]
        {
            engine.visual.setOverlayText ("BASS RIOT // LIVE");
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 1.0f }, { "scene.layout", 1.0f },
                { "scene.wireframe", 1.0f }, { "scene.opacity", 0.55f }, { "scene.hue", 0.02f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 2.0f }, { "layer2d.opacity", 0.45f },
                { "text.enabled", 1.0f }, { "text.opacity", 1.0f }, { "text.size", 1.35f },
                { "text.extrude", 0.65f }, { "text.glitch", 0.35f }, { "text.audioBounce", 1.2f },
                { "text.hue", 0.08f },
                { "post.feedback", 0.68f }, { "post.rgbSplit", 0.25f }, { "post.contrast", 1.35f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "audio.transient", "", "text.glitch", 0.7f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "audio.bass",      "", "text.size", 0.45f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "lfo.1",           "", "text.rotation", 0.15f, 1.0f, true, 0 });
        });

    // 12. Oscilloscope Vector Lab
    makePreset ("12. Oscilloscope Vector Lab", "Shaders & 2D", "oscilloscope, spectrum, vector, wireframe, technical",
        "Classic vector CRT oscilloscope and 16-band spectrum analyzer paired with a wireframe geodesic sphere and emerald phosphor bloom.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 3.0f }, { "scene.layout", 4.0f },
                { "scene.instances", 256.0f }, { "scene.wireframe", 1.0f }, { "scene.hue", 0.38f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 2.0f }, { "layer2d.opacity", 1.0f },
                { "layer2d.colorShift", 0.36f },
                { "post.feedback", 0.62f }, { "post.bloom", 0.95f }, { "post.scanlines", 0.35f }
            });
        });

    // 13. Lorenz Strange Attractor Dreams
    makePreset ("13. Lorenz Strange Attractor Dreams", "Chaos & Generative", "chaos, lorenz, ambient, generative, sphere",
        "Self-evolving ambient visual sculpture where X/Y/Z coordinates of the Lorenz attractor continuously steer camera orbit, spread, and color.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 0.0f }, { "scene.layout", 4.0f },
                { "scene.instances", 800.0f }, { "scene.spread", 1.15f }, { "scene.hue", 0.72f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 0.0f }, { "layer2d.opacity", 0.5f },
                { "post.feedback", 0.87f }, { "post.bloom", 1.05f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "chaos.lorenzX", "", "scene.camOrbit", 0.9f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "chaos.lorenzY", "", "scene.camHeight", 0.8f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "chaos.lorenzZ", "", "scene.spread", 0.55f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "chaos.drift1",  "", "scene.hue", 0.7f, 1.0f, false, 0 });
        });

    // 14. 16-Step Techno Polyrhythm
    makePreset ("14. 16-Step Techno Polyrhythm", "Chaos & Generative", "sequencer, techno, rhythmic, strobe, kaleido",
        "Dual tempo-synced step sequencers (16-step forward against 8-step ping-pong) modulating kaleidoscope symmetry, zoom, and strobe.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 2.0f }, { "scene.layout", 1.0f },
                { "scene.instances", 512.0f }, { "scene.hue", 0.52f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 1.0f }, { "layer2d.opacity", 0.7f },
                { "post.kaleidoscope", 4.0f }, { "post.feedback", 0.79f }, { "post.bloom", 0.9f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "seq.1",           "", "post.kaleidoscope", 0.5f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "seq.2",           "", "scene.camdist", -0.45f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "env.1",           "", "post.rgbSplit", 0.55f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "audio.transient", "", "post.strobe", 0.25f, 2.0f, false, 0 });
        });

    // 15. Custom GLSL Raymarching Temple
    makePreset ("15. Custom GLSL Raymarching Temple", "Shaders & 2D", "glsl, custom, shader, raymarching, fractal",
        "Demonstrates the built-in live GLSL code editor running a custom audio-reactive signed-distance-field (SDF) raymarching shader.",
        [this]
        {
            engine.visual.setCustomShaderCode (R"glsl(
// Custom SDF Raymarching Octahedron Temple (edit live in the 2D Shader tab!)
mat2 rot(float a){ float c=cos(a), s=sin(a); return mat2(c,-s,s,c); }
float sdOcta(vec3 p, float s){ p=abs(p); return (p.x+p.y+p.z-s)*0.57735027; }
float map(vec3 p){
    p.xz *= rot(iTime * 0.35);
    p.xy *= rot(iTime * 0.22);
    vec3 q = mod(p + 2.0, 4.0) - 2.0;
    float d = sdOcta(q, 0.85 + u_env * 0.6);
    return d;
}
void mainImage(out vec4 o, in vec2 fc){
    vec2 uv = (fc - 0.5 * iResolution.xy) / iResolution.y;
    vec3 ro = vec3(0.0, 0.0, iTime * (1.5 + u_rms * 3.0));
    vec3 rd = normalize(vec3(uv, 1.2 - u_transient * 0.3));
    float t = 0.0;
    vec3 col = vec3(0.0);
    for (int i = 0; i < 48; ++i){
        vec3 p = ro + rd * t;
        float d = map(p);
        if (d < 0.002 || t > 24.0) break;
        col += (0.5 + 0.5 * cos(vec3(0.0, 2.0, 4.0) + t * 0.25 + u_centroid * 5.0)) * (0.018 / (abs(d) + 0.04));
        t += d * 0.75;
    }
    o = vec4(col, 1.0);
}
)glsl");
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 5.0f }, { "layer2d.opacity", 1.0f },
                { "post.feedback", 0.68f }, { "post.bloom", 0.9f }
            });
        });

    // 16. Sidechain Pump & Mid-Side Wide
    makePreset ("16. Sidechain Pump & Mid-Side Wide", "3D & Geometry", "sidechain, mid-side, stereo, mixing, analyzer",
        "Uses Mid/Side stereo width and Sidechain 1 & 2 inputs to drive cluster spread, chromatic split, and central core pulse.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 0.0f }, { "scene.layout", 0.0f },
                { "scene.instances", 1024.0f }, { "scene.hue", 0.48f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 2.0f }, { "layer2d.opacity", 0.6f },
                { "post.feedback", 0.75f }, { "post.bloom", 0.85f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "audio.width", "", "scene.spread", 0.85f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "audio.mid",   "", "scene.pulse", 0.75f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "audio.side",  "", "post.rgbSplit", 0.65f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "audio.sc1",   "", "post.bloom", 0.80f, 1.0f, false, 0 });
        });

    // 17. VHS Retro Synthwave 1984
    makePreset ("17. VHS Retro Synthwave 1984", "Text & VJ", "synthwave, retro, vhs, scanlines, sunset, 80s",
        "Outrun neon aesthetic with synth deck horizon, magenta/cyan palette, CRT scanlines, chromatic aberration, and retro title overlay.",
        [this]
        {
            engine.visual.setOverlayText ("SYNTHWAVE // 1984");
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 3.0f }, { "scene.layout", 2.0f },
                { "scene.wireframe", 1.0f }, { "scene.hue", 0.83f }, { "scene.emissive", 2.2f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 4.0f }, { "layer2d.opacity", 0.55f },
                { "video.enabled", 1.0f }, { "video.source", 1.0f }, { "video.opacity", 0.75f },
                { "text.enabled", 1.0f }, { "text.opacity", 0.9f }, { "text.posY", 0.55f },
                { "text.size", 0.95f }, { "text.hue", 0.82f },
                { "post.scanlines", 0.65f }, { "post.rgbSplit", 0.32f }, { "post.bloom", 1.05f },
                { "post.vignette", 0.45f }
            });
        });

    // 18. Minimalist Clean Analyzer
    makePreset ("18. Minimalist Clean Studio Analyzer", "Shaders & 2D", "clean, studio, analyzer, spectrum, minimal",
        "Zero-feedback, precision studio visualiser showing 16-band spectrum, stereo phase/width, and clean 3D level ring.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 0.0f }, { "scene.layout", 2.0f },
                { "scene.instances", 128.0f }, { "scene.hue", 0.55f }, { "scene.emissive", 1.0f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 2.0f }, { "layer2d.opacity", 0.95f },
                { "post.feedback", 0.0f }, { "post.bloom", 0.35f }, { "post.rgbSplit", 0.0f },
                { "post.vignette", 0.15f }
            });
        });

    // 19. Webcam Pose & Hand Gesture Conductor
    makePreset ("19. Webcam Pose & Hand Gesture Conductor", "Webcam & Interactive", "webcam, pose, hands, face, gesture, conductor",
        "Uses dedicated vision thread signals (pose.handSpread, pose.headX/Y, pose.bodyLean, pose.gestureWave) to conduct 3D geometry and kaleido.",
        [this]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 3.0f }, { "scene.layout", 3.0f },
                { "scene.instances", 640.0f }, { "scene.emissive", 2.3f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 1.0f }, { "layer2d.opacity", 0.55f },
                { "webcam.enabled", 1.0f }, { "webcam.opacity", 0.8f }, { "webcam.edgeDetect", 0.75f },
                { "post.feedback", 0.83f }, { "post.bloom", 1.0f }
            });
            engine.modulation.clearRoutes();
            engine.modulation.addRoute ({ true, "pose.handSpread",  "", "scene.spread", 0.9f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "pose.headX",       "", "scene.camOrbit", 0.85f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "pose.bodyLean",    "", "post.feedbackRotate", 0.6f, 1.0f, true, 0 });
            engine.modulation.addRoute ({ true, "pose.gestureWave", "", "post.rgbSplit", 0.75f, 1.0f, false, 0 });
            engine.modulation.addRoute ({ true, "pose.proximity",   "", "post.bloom", 0.7f, 1.0f, false, 0 });
        });

    // 20. ISF Shader & Luau Scripted Attractor
    makePreset ("20. ISF Shader & Luau Scripted Attractor", "Chaos & Generative", "isf, shader, luau, script, wasm, generative",
        "Combines an ISF-format shader (with /*{ INPUTS }*/ header) and a stateful Luau script node in the Node Graph driving 3D torus rings.",
        [this]
        {
            engine.visual.setCustomShaderCode (R"glsl(/*{
  "DESCRIPTION": "ISF Audio Tunnel Example",
  "CREDIT": "AudioVisualizer Open Content Pack (MIT)",
  "INPUTS": [
    { "NAME": "u_env", "TYPE": "float", "DEFAULT": 0.5 }
  ]
}*/
void main()
{
    vec2 uv = isf_FragNormCoord * 2.0 - 1.0;
    uv.x *= RENDERSIZE.x / max(RENDERSIZE.y, 1.0);
    float r = length(uv);
    float a = atan(uv.y, uv.x);
    float rings = sin(12.0 / (r + 0.08) - TIME * 4.0 + sin(a * 6.0) * (1.0 + u_env * 3.0));
    vec3 col = 0.5 + 0.5 * cos(TIME * 0.5 + r * 3.0 + vec3(0.0, 2.1, 4.2));
    gl_FragColor = vec4(col * smoothstep(-0.2, 0.9, rings) * (1.0 - r * 0.45), 1.0);
}
)glsl");
            applyParams (engine.visual, {
                { "scene.enabled", 1.0f }, { "scene.geometry", 2.0f }, { "scene.layout", 2.0f },
                { "scene.instances", 512.0f }, { "scene.opacity", 0.7f },
                { "layer2d.enabled", 1.0f }, { "layer2d.mode", 5.0f }, { "layer2d.opacity", 0.85f },
                { "post.feedback", 0.78f }, { "post.bloom", 0.95f }
            });
        });

    // =============================================================================================
    // 21-42: presets that use the extra layers, shapes, patterns, generators, fonts and sources
    // =============================================================================================
    struct XL { int id; juce::String pre; };
    auto addLayer = [this] (LayerType t, std::initializer_list<std::pair<const char*, float>> vals) -> XL
    {
        const int id = engine.visual.addLayer (t);
        const auto pre = engine.visual.getLayerPrefix (id);
        for (const auto& kv : vals)
            engine.visual.setBase (pre + "." + kv.first, kv.second);
        return { id, pre };
    };
    auto route = [this] (const char* src, const juce::String& dst, float amount, bool bipolar = false, float curve = 1.0f)
    {
        engine.modulation.addRoute ({ true, src, "", dst, amount, curve, bipolar, 0.0f });
    };

    // 21
    makePreset ("21. Cosmic Galaxy Arms", "3D & Geometry", "galaxy, space, spiral, aurora, zoom blur, ambient",
        "Nine hundred glowing spheres swirl in three galaxy arms over drifting aurora clouds. Kick drums punch a zoom blur; slow noise orbits the camera.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 0.0f }, { "scene.layout", 9.0f }, { "scene.instances", 900.0f }, { "scene.hue", 0.72f },
                { "scene.emissive", 2.0f }, { "scene.camdist", 11.0f }, { "scene.camHeight", 6.0f },
                { "layer2d.mode", 10.0f }, { "layer2d.opacity", 0.7f }, { "layer2d.colorShift", 0.6f },
                { "post.feedback", 0.8f }, { "post.zoomBlur", 0.25f }, { "post.bloom", 1.1f }
            });
            engine.modulation.clearRoutes();
            route ("audio.bass", "scene.pulse", 0.6f);
            route ("noise.slow", "scene.camOrbit", 0.9f, true);
            route ("audio.centroid", "layer2d.colorShift", 0.5f);
            route ("audio.kick", "post.zoomBlur", 0.4f);
        });

    // 22
    makePreset ("22. Torus Knot Lightshow", "3D & Geometry", "torus knot, lissajous, metal, tunnel, twirl",
        "Chrome torus knots traced along a Lissajous path in front of a checker tunnel. Envelope twirls the whole picture, transients split the colours.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 9.0f }, { "scene.layout", 8.0f }, { "scene.instances", 320.0f }, { "scene.scale", 1.4f },
                { "scene.emissive", 2.6f }, { "scene.metallic", 0.95f }, { "scene.roughness", 0.15f }, { "scene.hue", 0.9f },
                { "layer2d.mode", 6.0f }, { "layer2d.opacity", 0.55f },
                { "post.twirl", 0.15f }, { "post.bloom", 1.2f }, { "post.feedback", 0.75f }, { "post.rgbSplit", 0.2f }
            });
            engine.modulation.clearRoutes();
            route ("audio.env", "post.twirl", 0.5f, true);
            route ("lfo.1", "scene.hue", 0.4f);
            route ("audio.transient", "post.rgbSplit", 0.5f);
        });

    // 23
    makePreset ("23. Pyramid Tunnel Rave", "3D & Geometry", "pyramid, tunnel, neon grid, kick, snare, rave",
        "Pyramids stream through a tunnel over a neon grid horizon. Kick, snare and hat each drive their own effect: zoom blur, glitch and colour split.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 8.0f }, { "scene.layout", 10.0f }, { "scene.instances", 700.0f }, { "scene.hue", 0.1f },
                { "scene.emissive", 2.2f }, { "layer2d.mode", 9.0f }, { "layer2d.opacity", 0.6f },
                { "post.feedback", 0.7f }, { "post.bloom", 1.0f }, { "post.zoomBlur", 0.2f }, { "post.scanlines", 0.15f }
            });
            engine.modulation.clearRoutes();
            route ("audio.kick", "post.zoomBlur", 0.6f);
            route ("audio.snare", "post.glitch", 0.5f);
            route ("clock.bar", "scene.hue", 0.5f);
            route ("audio.hat", "post.rgbSplit", 0.4f);
        });

    // 24
    makePreset ("24. Octahedron Wave Plane", "3D & Geometry", "octahedron, wave, plane, gradient, overlay",
        "A rolling field of crystal octahedra under a soft radial gradient. Bass lifts the waves while slow noise sails the camera around them.",
        [this, route, addLayer]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 6.0f }, { "scene.layout", 6.0f }, { "scene.instances", 900.0f }, { "scene.camHeight", 7.0f },
                { "scene.camdist", 11.0f }, { "scene.hue", 0.35f }, { "scene.saturation", 0.9f }, { "layer2d.enabled", 0.0f },
                { "post.bloom", 0.9f }, { "post.feedback", 0.6f }
            });
            const auto fill = addLayer (LayerType::Fill, { { "style", 2.0f }, { "hueA", 0.55f }, { "hueB", 0.95f }, { "opacity", 0.8f } });
            engine.modulation.clearRoutes();
            route ("audio.bass", "scene.pulse", 0.7f);
            route ("noise.medium", "scene.camOrbit", 0.8f, true);
            route ("audio.env", fill.pre + ".brightness", 0.3f);
        });

    // 25
    makePreset ("25. Concentric Rings Meditation", "Chaos & Generative", "rings, disc, calm, twirl, ripple, kaleido flower",
        "Slow counter-rotating rings of discs above a kaleido flower. Everything breathes with slow noise and a 20-second sine.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 10.0f }, { "scene.layout", 7.0f }, { "scene.instances", 480.0f }, { "scene.scale", 1.3f }, { "scene.hue", 0.5f },
                { "layer2d.mode", 11.0f }, { "layer2d.opacity", 0.6f },
                { "post.twirl", 0.25f }, { "post.ripple", 0.2f }, { "post.feedback", 0.85f }, { "post.bloom", 0.9f }
            });
            engine.modulation.clearRoutes();
            route ("noise.slow", "post.twirl", 0.6f, true);
            route ("audio.rms", "scene.spread", 0.4f);
            route ("time.slowSine", "layer2d.colorShift", 0.6f);
        });

    // 26
    makePreset ("26. Spectrum Studio Pro", "Shaders & 2D", "spectrum, analyzer, bars, radial, studio, clean",
        "Two spectrum analyzers stacked on a quiet aurora: mirrored bars across the middle and a small radial burst. Clean and readable.",
        [this, route, addLayer]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "layer2d.mode", 10.0f }, { "layer2d.opacity", 0.35f },
                { "post.feedback", 0.3f }, { "post.bloom", 0.6f }
            });
            addLayer (LayerType::Spectrum, { { "style", 1.0f }, { "posY", 0.0f }, { "hue", 0.5f }, { "gain", 1.6f }, { "glow", 0.6f } });
            const auto radial = addLayer (LayerType::Spectrum, { { "style", 2.0f }, { "size", 0.6f }, { "hue", 0.9f }, { "opacity", 0.6f } });
            engine.modulation.clearRoutes();
            route ("audio.centroid", radial.pre + ".hue", 0.5f);
        });

    // 27
    makePreset ("27. Radial Spectrum Bloom", "Shaders & 2D", "radial, spectrum, metaballs, bloom, glow",
        "A fat radial spectrum burst glowing over drifting metaballs, with long bloom and a gentle feedback haze.",
        [this, route, addLayer]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "layer2d.mode", 8.0f }, { "layer2d.opacity", 0.6f },
                { "post.bloom", 1.4f }, { "post.feedback", 0.86f }, { "post.feedbackZoom", 0.02f }
            });
            const auto sp = addLayer (LayerType::Spectrum, { { "style", 2.0f }, { "size", 1.2f }, { "glow", 1.2f }, { "thickness", 0.7f }, { "hueSpread", 0.6f } });
            engine.modulation.clearRoutes();
            route ("audio.env", sp.pre + ".size", 0.3f);
            route ("noise.medium", sp.pre + ".hue", 0.5f);
            route ("audio.kick", "post.bloom", 0.4f);
        });

    // 28
    makePreset ("28. Dot Matrix Display", "Text & VJ", "dots, matrix, led, monospace, text, terminal",
        "An LED dot-matrix spectrum under a monospaced terminal readout, with posterized colour, scanlines and film grain.",
        [this, route, addLayer]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "layer2d.enabled", 0.0f },
                { "post.posterize", 0.3f }, { "post.scanlines", 0.5f }, { "post.vignette", 0.5f }, { "post.grain", 0.2f }, { "post.feedback", 0.4f }
            });
            addLayer (LayerType::Spectrum, { { "style", 4.0f }, { "posY", -0.3f }, { "size", 1.2f }, { "hue", 0.35f } });
            const auto t = addLayer (LayerType::Text, { { "size", 0.8f }, { "posY", 0.55f }, { "hue", 0.3f }, { "glitch", 0.1f } });
            engine.visual.setLayerText (t.id, "SIGNAL // OK");
            engine.visual.setLayerFont (t.id, juce::Font::getDefaultMonospacedFontName());
            engine.modulation.clearRoutes();
            route ("audio.transient", t.pre + ".glitch", 0.7f);
        });

    // 29
    makePreset ("29. Neon Grid Outrun Deluxe", "Text & VJ", "outrun, neon grid, duotone, synthwave, line spectrum",
        "Neon grid horizon with a glowing line spectrum, a duotone magenta-to-amber grade and a big OUTRUN title that shifts colour on the bar.",
        [this, route, addLayer]
        {
            engine.visual.setOverlayText ("OUTRUN");
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "layer2d.mode", 9.0f }, { "layer2d.opacity", 0.9f },
                { "text.enabled", 1.0f }, { "text.posY", 0.5f }, { "text.hue", 0.85f }, { "text.size", 1.1f },
                { "post.duotone", 0.7f }, { "post.duoHueA", 0.8f }, { "post.duoHueB", 0.1f }, { "post.scanlines", 0.35f },
                { "post.bloom", 1.1f }, { "post.rgbSplit", 0.2f }, { "post.vignette", 0.4f }
            });
            addLayer (LayerType::Spectrum, { { "style", 3.0f }, { "posY", -0.1f }, { "hue", 0.85f }, { "glow", 1.2f } });
            engine.modulation.clearRoutes();
            route ("clock.bar", "text.hue", 0.4f);
            route ("audio.bass", "post.bloom", 0.5f);
        });

    // 30
    makePreset ("30. Gradient Wash Ambient", "Chaos & Generative", "gradient, ambient, noise, slow, angular, chaos",
        "A sparse drifting cloud of spheres under an angular gradient sweep that slowly changes colour and angle. Made for long, quiet passages.",
        [this, route, addLayer]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 0.0f }, { "scene.layout", 5.0f }, { "scene.instances", 200.0f }, { "scene.scale", 0.6f },
                { "scene.opacity", 0.6f }, { "scene.hue", 0.55f }, { "layer2d.mode", 10.0f }, { "layer2d.opacity", 0.7f },
                { "post.feedback", 0.88f }, { "post.bloom", 0.9f }, { "post.ripple", 0.2f }
            });
            const auto fill = addLayer (LayerType::Fill, { { "style", 3.0f }, { "scale", 6.0f }, { "hueA", 0.6f }, { "hueB", 0.95f } });
            engine.modulation.clearRoutes();
            route ("noise.slow", fill.pre + ".hueA", 0.7f);
            route ("noise.medium", fill.pre + ".hueB", 0.7f);
            route ("time.slowSine", fill.pre + ".angle", 0.8f, true);
            route ("chaos.lorenzX", "scene.camOrbit", 0.5f, true);
        });

    // 31
    makePreset ("31. Webcam Direct Mirror Portal", "Webcam & Interactive", "webcam, direct, video source, kaleidoscope, twirl, portal",
        "Your live webcam shown straight through the Video layer (no recording step), folded into a six-sided mirror with a swirl that follows your motion.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "layer2d.mode", 11.0f }, { "layer2d.opacity", 0.5f },
                { "video.enabled", 1.0f }, { "video.source", 4.0f }, { "video.opacity", 0.95f }, { "video.blend", 1.0f },
                { "post.kaleidoscope", 6.0f }, { "post.feedback", 0.8f }, { "post.twirl", 0.2f }, { "post.bloom", 1.0f }
            });
            engine.modulation.clearRoutes();
            route ("cam.motion", "post.twirl", 0.6f, true);
            route ("audio.env", "post.bloom", 0.5f);
            route ("cam.x", "post.kaleidoscope", 0.3f);
        });

    // 32
    makePreset ("32. Webcam Kaleido Booth", "Webcam & Interactive", "webcam, kaleidoscope, booth, direct, feedback",
        "A photo-booth kaleidoscope: your webcam, mirrored eight ways with spinning feedback. Bass pumps the zoom, motion cycles the colours.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "layer2d.enabled", 0.0f },
                { "video.enabled", 1.0f }, { "video.source", 4.0f }, { "video.opacity", 1.0f }, { "video.blend", 0.0f },
                { "post.kaleidoscope", 8.0f }, { "post.feedback", 0.85f }, { "post.feedbackRotate", 0.01f }
            });
            engine.modulation.clearRoutes();
            route ("audio.bass", "post.feedbackZoom", 0.4f);
            route ("cam.motion", "post.hueShift", 0.6f);
            route ("audio.env", "post.rgbSplit", 0.5f);
        });

    // 33
    makePreset ("33. Webcam Neon Duotone", "Webcam & Interactive", "webcam, duotone, neon, edge, grain, poster",
        "Direct webcam picture plus neon edge lines, graded into a moody duotone with grain and slight posterize. Slow noise slides the shadow colour.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "layer2d.enabled", 0.0f },
                { "video.enabled", 1.0f }, { "video.source", 4.0f }, { "video.opacity", 1.0f }, { "video.blend", 0.0f }, { "video.tintHue", 0.6f },
                { "webcam.enabled", 1.0f }, { "webcam.edgeDetect", 0.9f }, { "webcam.opacity", 0.8f },
                { "post.duotone", 0.85f }, { "post.duoHueA", 0.75f }, { "post.duoHueB", 0.08f },
                { "post.grain", 0.25f }, { "post.bloom", 1.2f }, { "post.posterize", 0.15f }
            });
            engine.modulation.clearRoutes();
            route ("audio.kick", "post.bloom", 0.5f);
            route ("noise.slow", "post.duoHueA", 0.6f);
        });

    // 34
    makePreset ("34. Webcam Ghost Echo", "Webcam & Interactive", "webcam, ghost, echo, feedback, hands, gesture",
        "Long ghosting echoes of you, from both the direct picture and a neon-edge pass. Spread your hands to smear it outward; waving splits the colours.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 3.0f }, { "scene.layout", 4.0f }, { "scene.instances", 200.0f }, { "scene.opacity", 0.4f },
                { "layer2d.enabled", 0.0f },
                { "video.enabled", 1.0f }, { "video.source", 4.0f }, { "video.opacity", 0.9f },
                { "webcam.enabled", 1.0f }, { "webcam.edgeDetect", 0.8f }, { "webcam.hueShift", 0.3f },
                { "post.feedback", 0.93f }, { "post.feedbackZoom", 0.02f }, { "post.feedbackRotate", 0.006f }, { "post.bloom", 1.1f }
            });
            engine.modulation.clearRoutes();
            route ("pose.handSpread", "post.zoomBlur", 0.8f);
            route ("pose.gestureWave", "post.rgbSplit", 0.7f);
            route ("audio.env", "webcam.uvDisplace", 0.5f);
        });

    // 35
    makePreset ("35. Hand Wave Spectrum", "Webcam & Interactive", "webcam, hands, spectrum, radial, gesture, interactive",
        "A radial spectrum you can shape with your hands: spread them to grow it, move your head to change its colour.",
        [this, route, addLayer]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "layer2d.enabled", 0.0f },
                { "video.enabled", 1.0f }, { "video.source", 4.0f }, { "video.opacity", 0.5f },
                { "post.bloom", 1.2f }, { "post.feedback", 0.7f }
            });
            const auto sp = addLayer (LayerType::Spectrum, { { "style", 2.0f }, { "glow", 1.0f }, { "size", 0.9f } });
            engine.modulation.clearRoutes();
            route ("pose.handSpread", sp.pre + ".size", 0.8f);
            route ("pose.headY", sp.pre + ".hue", 0.6f);
            route ("pose.leftHandY", "post.hueShift", 0.5f);
        });

    // 36
    makePreset ("36. Kick Snare Hat Reactive", "Chaos & Generative", "kick, snare, hat, drums, trigger, reactive",
        "Each part of the drum kit moves something different: kick punches the cubes and blurs the zoom, snare flares the gradient, hats split the colours.",
        [this, route, addLayer]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 1.0f }, { "scene.layout", 1.0f }, { "scene.instances", 343.0f }, { "layer2d.enabled", 0.0f },
                { "post.feedback", 0.6f }, { "post.bloom", 0.9f }
            });
            const auto fill = addLayer (LayerType::Fill, { { "style", 2.0f }, { "hueA", 0.05f }, { "hueB", 0.6f }, { "brightness", 0.5f } });
            engine.modulation.clearRoutes();
            route ("audio.kick", "scene.pulse", 0.8f);
            route ("audio.kick", "post.zoomBlur", 0.5f);
            route ("audio.snare", fill.pre + ".brightness", 0.5f);
            route ("audio.hat", "post.rgbSplit", 0.6f);
            route ("audio.peakHold", "post.bloom", 0.5f);
        });

    // 37
    makePreset ("37. Beat Clock Strobe Grid", "Chaos & Generative", "clock, tempo, beat, grid, checkerboard, host sync",
        "Everything locked to the host tempo: a cube grid, a checkerboard that turns on the beat, a bloom pulse every beat and a camera swing per bar.",
        [this, route, addLayer]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 1.0f }, { "scene.layout", 1.0f }, { "scene.instances", 512.0f }, { "scene.hue", 0.15f },
                { "layer2d.enabled", 0.0f }, { "post.feedback", 0.5f }, { "post.bloom", 0.8f }
            });
            const auto chk = addLayer (LayerType::Fill, { { "style", 5.0f }, { "scale", 1.5f }, { "hueA", 0.1f }, { "hueB", 0.7f }, { "speed", 0.0f }, { "opacity", 0.6f } });
            engine.modulation.clearRoutes();
            route ("clock.pulse", "post.bloom", 0.6f);
            route ("clock.sineBar", "scene.camOrbit", 0.8f, true);
            route ("clock.triBeat", chk.pre + ".angle", 0.4f);
            route ("clock.beat", "post.hueShift", 0.3f);
        });

    // 38
    makePreset ("38. Typewriter Poster", "Text & VJ", "typography, serif, monospace, poster, rings, fonts",
        "A concert-poster layout: a huge serif BASS over a ringed backdrop with a monospaced CULTURE tagline. Kicks make the headline jump.",
        [this, route, addLayer]
        {
            engine.visual.setOverlayText ("BASS");
            engine.visual.setLayerFont (4, juce::Font::getDefaultSerifFontName());
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "layer2d.enabled", 0.0f },
                { "text.enabled", 1.0f }, { "text.size", 1.6f }, { "text.posY", 0.15f }, { "text.hue", 0.08f }, { "text.extrude", 0.6f },
                { "post.bloom", 0.8f }, { "post.vignette", 0.4f }, { "post.grain", 0.15f }, { "post.feedback", 0.4f }
            });
            addLayer (LayerType::Fill, { { "style", 6.0f }, { "hueA", 0.08f }, { "hueB", 0.6f }, { "brightness", 0.6f }, { "opacity", 0.8f } });
            const auto tag = addLayer (LayerType::Text, { { "size", 0.7f }, { "posY", -0.45f }, { "hue", 0.5f }, { "extrude", 0.1f } });
            engine.visual.setLayerText (tag.id, "CULTURE");
            engine.visual.setLayerFont (tag.id, juce::Font::getDefaultMonospacedFontName());
            engine.modulation.clearRoutes();
            route ("audio.kick", "text.size", 0.4f);
            route ("noise.slow", "text.rotation", 0.1f, true);
        });

    // 39
    makePreset ("39. Kinetic Font Chaos", "Text & VJ", "kinetic type, italic, serif, mono, glitch, words",
        "Three words in three different typefaces glitch and tilt over a spirograph. Transients tear the letters, noise slowly rotates each word.",
        [this, route, addLayer]
        {
            applyParams (engine.visual, {
                { "scene.enabled", 0.0f }, { "text.enabled", 0.0f }, { "layer2d.mode", 12.0f }, { "layer2d.opacity", 0.5f },
                { "post.glitch", 0.2f }, { "post.rgbSplit", 0.3f }, { "post.grain", 0.15f }, { "post.bloom", 0.9f }
            });
            const auto a = addLayer (LayerType::Text, { { "size", 1.0f }, { "posY", 0.5f }, { "italic", 1.0f }, { "glitch", 0.4f }, { "hue", 0.0f } });
            engine.visual.setLayerText (a.id, "SOUND");
            const auto b = addLayer (LayerType::Text, { { "size", 1.0f }, { "posY", 0.0f }, { "glitch", 0.4f }, { "hue", 0.3f } });
            engine.visual.setLayerText (b.id, "LIGHT");
            engine.visual.setLayerFont (b.id, juce::Font::getDefaultSerifFontName());
            const auto c = addLayer (LayerType::Text, { { "size", 1.0f }, { "posY", -0.5f }, { "glitch", 0.4f }, { "hue", 0.6f } });
            engine.visual.setLayerText (c.id, "MOTION");
            engine.visual.setLayerFont (c.id, juce::Font::getDefaultMonospacedFontName());
            engine.modulation.clearRoutes();
            for (const auto* l : { &a, &b, &c })
            {
                route ("audio.transient", l->pre + ".glitch", 0.6f);
                route ("noise.fast", l->pre + ".rotation", 0.08f, true);
            }
        });

    // 40
    makePreset ("40. Mouse Playground", "Chaos & Generative", "mouse, interactive, cones, orbit, performance",
        "Play it with the mouse: move over the output window to orbit and tilt the camera around a field of cones; hold the button to flare the bloom.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 4.0f }, { "scene.layout", 0.0f }, { "scene.instances", 640.0f }, { "scene.hue", 0.4f },
                { "layer2d.mode", 0.0f }, { "layer2d.opacity", 0.4f }, { "post.feedback", 0.75f }, { "post.bloom", 0.8f }
            });
            engine.modulation.clearRoutes();
            route ("mouse.x", "scene.camOrbit", 1.0f, true);
            route ("mouse.y", "scene.camHeight", 0.8f, true);
            route ("mouse.down", "post.bloom", 0.8f);
            route ("mouse.y", "layer2d.colorShift", 0.6f);
        });

    // 41
    makePreset ("41. MIDI Aftertouch Pad", "MIDI Performance", "midi, aftertouch, mod wheel, sustain, pad, expressive",
        "For keyboards: pressure blooms the picture, the mod wheel folds a kaleidoscope, the sustain pedal lengthens the trails, and notes pick the colour.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 0.0f }, { "scene.layout", 2.0f }, { "scene.instances", 400.0f }, { "layer2d.enabled", 0.0f },
                { "post.feedback", 0.6f }, { "post.bloom", 0.7f }
            });
            engine.modulation.clearRoutes();
            route ("midi.aftertouch", "post.bloom", 0.8f);
            route ("midi.modwheel", "post.kaleidoscope", 0.5f);
            route ("midi.sustain", "post.feedback", 0.2f);
            route ("midi.velocity", "scene.pulse", 0.8f);
            route ("midi.trigger", "post.zoomBlur", 0.5f);
            route ("midi.note", "scene.hue", 0.8f);
        });

    // 42
    makePreset ("42. Rolloff Rainbow Ribbons", "Shaders & 2D", "stereo, rolloff, noisiness, ribbons, correlation, analysis",
        "Reads the sound's character: brighter spectrum shifts the hue, noisy sounds add grain, wide stereo spreads the wave plane, left and right spin it.",
        [this, route]
        {
            applyParams (engine.visual, {
                { "scene.geometry", 0.0f }, { "scene.layout", 6.0f }, { "scene.instances", 800.0f }, { "scene.camHeight", 6.0f },
                { "layer2d.mode", 10.0f }, { "layer2d.opacity", 0.7f }, { "post.feedback", 0.7f }, { "post.bloom", 0.9f }
            });
            engine.modulation.clearRoutes();
            route ("audio.rolloff", "scene.hue", 0.8f);
            route ("audio.noisiness", "post.grain", 0.6f);
            route ("audio.correlation", "scene.spread", 0.8f);
            route ("audio.left", "scene.spinY", 0.5f, true);
            route ("audio.right", "post.twirl", 0.4f, true);
        });
}
