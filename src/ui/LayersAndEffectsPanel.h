#pragma once
#include "Theme.h"
#include "Widgets.h"

namespace avlayers
{
inline juce::String titleFor (LayerType t)
{
    switch (t)
    {
        case LayerType::Scene3D:   return "3D Scene";
        case LayerType::Shader2D:  return "2D Shader";
        case LayerType::VideoLoop: return "Video & Live Loop";
        case LayerType::WebcamFX:  return "Webcam FX";
        case LayerType::Text:      return "3D Text";
        case LayerType::Spectrum:  return "Spectrum Analyzer";
        case LayerType::Fill:      return "Gradient / Fill";
        default:                   return "Layer";
    }
}

inline juce::String blurbFor (LayerType t)
{
    switch (t)
    {
        case LayerType::Scene3D:   return "Hundreds of instanced 3D shapes arranged in a pattern that pulses with the spectrum.";
        case LayerType::Shader2D:  return "Procedural backdrops and generators. The original 2D layer can also run your own GLSL.";
        case LayerType::VideoLoop: return "Play a video or image, or record a live webcam loop that can follow the host tempo.";
        case LayerType::WebcamFX:  return "Your webcam as a visual layer, with edge, key and warp effects. Tracks hands and head too.";
        case LayerType::Text:      return "Big kinetic type that bounces and glitches with the music. Pick any installed font.";
        case LayerType::Spectrum:  return "The audio spectrum drawn as bars, mirrored bars, a radial burst, a line or a dot matrix.";
        case LayerType::Fill:      return "Solid colour or an animated gradient, stripes, checkerboard or rings. Try it with Multiply or Overlay.";
        default:                   return {};
    }
}

inline juce::Colour colourFor (LayerType t)
{
    switch (t)
    {
        case LayerType::Scene3D:   return avtheme::accentCyan;
        case LayerType::Shader2D:  return avtheme::accentPurple;
        case LayerType::VideoLoop: return avtheme::accentAmber;
        case LayerType::WebcamFX:  return avtheme::accentGreen;
        case LayerType::Text:      return avtheme::accentRed;
        case LayerType::Spectrum:  return juce::Colour (0xff5b9dff);
        case LayerType::Fill:      return juce::Colour (0xffff8fd0);
        default:                   return avtheme::accentCyan;
    }
}

inline const juce::StringArray& systemFonts()
{
    static const juce::StringArray fonts = juce::Font::findAllTypefaceNames();
    return fonts;
}
} // namespace avlayers

// ==============================================================================
// Inspector for one layer: groups of settings (plus any buttons / editors that kind needs)
// ==============================================================================
class LayerInspector final : public juce::Component
{
public:
    LayerInspector (Engine& e, int id, std::function<void()> onChanged)
        : engine (e), layerId (id), type (e.visual.getLayerType (id)), prefix (e.visual.getLayerPrefix (id)),
          changed (std::move (onChanged))
    {
        addAndMakeVisible (flow);
        switch (type)
        {
            case LayerType::Scene3D:   buildScene();    break;
            case LayerType::Shader2D:  buildShader();   break;
            case LayerType::VideoLoop: buildVideo();    break;
            case LayerType::WebcamFX:  buildWebcam();   break;
            case LayerType::Text:      buildText();     break;
            case LayerType::Spectrum:  buildSpectrum(); break;
            case LayerType::Fill:      buildFill();     break;
            default: break;
        }
        syncFromEngine();
    }

    LayerType getType() const noexcept { return type; }

    bool reveal (const juce::String& paramId)
    {
        for (auto* c : ctrls)
            if (c->getParamId() == paramId)
            {
                flow.reveal (*c);
                c->flash();
                return true;
            }
        return false;
    }

    void syncFromEngine()
    {
        flow.syncFromEngine();

        if (type == LayerType::Shader2D && legacy())
        {
            const int ver = engine.visual.inlineShaderVersion.load();
            if (ver != seenShaderVersion && ! shaderEditor.hasKeyboardFocus (true))
            {
                seenShaderVersion = ver;
                const auto code = engine.visual.getCustomShaderCode();
                if (shaderEditor.getText() != code)
                    shaderEditor.setText (code, false);
            }
        }
        if (type == LayerType::Text)
        {
            if (! textInput.hasKeyboardFocus (true))
            {
                const auto t = engine.visual.getLayerText (layerId);
                if (textInput.getText() != t)
                    textInput.setText (t, false);
            }
            const auto f = engine.visual.getLayerFont (layerId);
            if (f != shownFont)
            {
                shownFont = f;
                selectFontInBox (f);
                fontPreview.repaint();
            }
        }
        loopMeter.repaint();
        poseScope.repaint();
        motionMeter.repaint();
    }

    void resized() override { flow.setBounds (getLocalBounds()); }

private:
    bool legacy() const noexcept { return VisualState::isLegacyLayer (layerId); }
    juce::Colour accent() const { return avlayers::colourFor (type); }

    ParamGroup& makeGroup (const juce::String& title, const juce::String& hint = {})
    {
        groups.push_back (std::make_unique<ParamGroup> (engine, title, hint, accent()));
        flow.addGroup (*groups.back());
        return *groups.back();
    }

    void add (ParamGroup& g, const char* suffix, const char* label = "", juce::StringArray choices = {})
    {
        ctrls.push_back (g.addParam (prefix + "." + suffix, label, std::move (choices)));
    }

    // ---- 3D scene ----
    void buildScene()
    {
        auto& geo = makeGroup ("Shape & pattern", "Pick the mesh, how many copies, and the pattern they are arranged in.");
        add (geo, "geometry", "Shape", { "Sphere", "Cube", "Torus", "Icosahedron", "Cone", "Cylinder", "Octahedron",
                                         "Tetrahedron", "Pyramid", "Torus knot", "Disc", "Custom glTF model" });
        add (geo, "layout", "Pattern", { "Phyllotaxis spiral", "3D grid", "Torus vortex", "DNA double helix", "Fibonacci shell",
                                         "Random cloud", "Wave plane", "Concentric rings", "Lissajous knot", "Galaxy arms", "Tunnel" });
        add (geo, "instances", "Copies");
        add (geo, "wireframe", "Wireframe");
        loadGltfBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Load 3D glTF / GLB Model", juce::File (engine.getModelPath()), "*.glb;*.gltf");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() != juce::File())
                    {
                        const auto p = fc.getResult().getFullPathName();
                        const auto geomId = prefix + ".geometry";
                        engine.performUndoable ("Load glTF Model", [this, p, geomId]
                        {
                            engine.setModelPath (p);
                            engine.visual.setBase (geomId, 11.0f);
                        });
                        changed();
                    }
                });
        };
        geo.addRow ({ &loadGltfBtn });

        auto& motion = makeGroup ("Motion & audio response", "Audio Pulse = how strongly the spectrum pushes the shapes around.");
        add (motion, "pulse", "Audio pulse");
        add (motion, "scale", "Size");
        add (motion, "spread", "Spread");
        add (motion, "spinX", "Spin X");
        add (motion, "spinY", "Spin Y");
        add (motion, "spinZ", "Spin Z");

        auto& cam = makeGroup ("Camera", "Where you are looking from.");
        add (cam, "camdist", "Distance");
        add (cam, "camOrbit", "Orbit angle");
        add (cam, "camHeight", "Height");

        auto& mat = makeGroup ("Colour & material", "A glossy metal look; Glow makes shapes bloom brightly.");
        add (mat, "hue", "Hue");
        add (mat, "saturation", "Saturation");
        add (mat, "metallic", "Metallic");
        add (mat, "roughness", "Roughness");
        add (mat, "emissive", "Glow");
    }

    // ---- 2D shader ----
    void buildShader()
    {
        auto& gen = makeGroup ("Generator", legacy() ? "Choose a built-in generator, or \"Custom GLSL\" to run the code editor below."
                                                      : "Choose one of the built-in generators.");
        juce::StringArray modes { "Domain plasma", "Sacred mandala", "Studio scope & FFT", "Bio-Voronoi cells", "Starfield warp",
                                  legacy() ? "Custom GLSL code" : "Interference waves",
                                  "Tunnel", "Julia fractal", "Metaballs", "Neon grid horizon", "Aurora clouds",
                                  "Kaleido flower", "Lissajous spirograph" };
        add (gen, "mode", "Generator", modes);
        add (gen, "speed", "Speed");
        add (gen, "scale", "Pattern scale");
        add (gen, "complexity", "Complexity");
        add (gen, "colorShift", "Palette shift");
        add (gen, "paramA", "Morph A");
        add (gen, "paramB", "Morph B");

        if (! legacy())
            return;

        auto& code = makeGroup ("Custom GLSL", "Write a Shadertoy-style mainImage(). \"Compile\" applies it instantly and saves it with the preset.");
        shaderEditor.setMultiLine (true, false);
        shaderEditor.setReturnKeyStartsNewLine (true);
        shaderEditor.setTabKeyUsedAsCharacter (true);
        shaderEditor.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain));
        shaderEditor.setText (engine.visual.getCustomShaderCode(), false);
        seenShaderVersion = engine.visual.inlineShaderVersion.load();

        compileShaderBtn.onClick = [this]
        {
            const auto src = shaderEditor.getText();
            engine.performUndoable ("Compile Custom GLSL", [this, src]
            {
                engine.setShaderPath ({});
                engine.visual.setBase ("layer2d.mode", 5.0f);
                engine.visual.setCustomShaderCode (src);
            });
            changed();
        };
        loadShaderFileBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Open Shadertoy GLSL Fragment File", juce::File (engine.getShaderPath()), "*.glsl;*.frag;*.fs;*.txt");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() != juce::File())
                    {
                        const auto f = fc.getResult();
                        const auto src = f.loadFileAsString();
                        shaderEditor.setText (src, false);
                        engine.performUndoable ("Load Shader File", [this, f, src]
                        {
                            engine.visual.setCustomShaderCode (src);
                            engine.setShaderPath (f.getFullPathName());
                            engine.visual.setBase ("layer2d.mode", 5.0f);
                        });
                        changed();
                    }
                });
        };
        code.addRow ({ &compileShaderBtn, &loadShaderFileBtn }, 30);
        code.addRow ({ &shaderEditor }, 280);
    }

    // ---- video / loop ----
    void buildVideo()
    {
        auto& src = makeGroup ("Source", "Pick \"Live webcam\" to show the camera directly, play a loaded file, or record a few seconds of the webcam into a loop you can scrub and stutter.");
        add (src, "source", "Source", { "GPU live loop buffer", "Synthwave horizon deck", "Cyber matrix deck", "Loaded video / image file",
                                        "Live webcam (direct)" });

        // Camera chooser right here, so the webcam source works without visiting the Webcam FX layer
        refreshCameraDevices();
        camDeviceBox.onChange = [this]
        {
            const int id = camDeviceBox.getSelectedId();
            if (id <= 1) engine.camera.close();
            else         engine.camera.open (id - 2);
        };
        src.addRow ({ &camDeviceBox });
        if (legacy())
        {
            recLoopBtn.onClick   = [this] { engine.loopCommand = 1; };
            clearLoopBtn.onClick = [this] { engine.loopCommand = 2; };
            loadVideoBtn.onClick = [this]
            {
                chooser = std::make_unique<juce::FileChooser> ("Open Video or Image File",
                    juce::File (engine.videoDecoder.getLoadedPath()), "*.mp4;*.mov;*.avi;*.wmv;*.mkv;*.png;*.jpg;*.jpeg;*.bmp");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc)
                    {
                        if (fc.getResult() != juce::File())
                        {
                            const auto p = fc.getResult().getFullPathName();
                            engine.performUndoable ("Load Video File", [this, p]
                            {
                                engine.videoDecoder.loadFile (p);
                                engine.visual.setBase ("video.source", 3.0f);
                                engine.visual.setBase ("video.enabled", 1.0f);
                            });
                            changed();
                        }
                    });
            };
            src.addRow ({ &loadVideoBtn });
            src.addRow ({ &recLoopBtn, &clearLoopBtn });
            loopMeter.painter = [this] (juce::Graphics& g, juce::Rectangle<float> r)
            {
                const float pos = engine.bus.get (engine.sLoopPos);
                const bool rec  = engine.bus.get (engine.sLoopRec) > 0.5f;
                g.setColour (rec ? avtheme::accentRed.withAlpha (0.8f) : avtheme::accentAmber.withAlpha (0.55f));
                g.fillRoundedRectangle (r.reduced (2.0f).withWidth ((r.getWidth() - 4.0f) * juce::jlimit (0.0f, 1.0f, pos)), 4.0f);
                g.setColour (avtheme::textBright);
                g.setFont (avtheme::font (11.5f, true));
                g.drawText (rec ? "RECORDING..." : "Loop playhead " + juce::String ((int) (pos * 100.0f)) + "%",
                            r.toNearestInt(), juce::Justification::centred);
            };
            src.addRow ({ &loopMeter }, 26);

            auto& play = makeGroup ("Playback", "Speed, loop range, and optional sync to the host tempo (applies to every video layer).");
            add (play, "speed", "Speed");
            add (play, "reverse", "Reverse");
            add (play, "tempoSync", "Sync to host tempo");
            add (play, "syncBeats", "Loop length (beats)");
            add (play, "inPoint", "Loop start");
            add (play, "outPoint", "Loop end");
            add (play, "stutter", "Beat stutter");
        }

        auto& look = makeGroup ("Look");
        add (look, "zoom", "Zoom");
        add (look, "tintHue", "Tint hue");
        if (! legacy())
            add (look, "stutter", "Beat stutter");
    }

    // ---- webcam ----
    void buildWebcam()
    {
        if (legacy())
        {
            auto& in = makeGroup ("Camera input", "Pick a camera. Hand, head and motion tracking feed the modulation sources (pose.*, cam.*).");
            refreshCameraDevices();
            camDeviceBox.onChange = [this]
            {
                const int id = camDeviceBox.getSelectedId();
                if (id <= 1) engine.camera.close();
                else         engine.camera.open (id - 2);
            };
            refreshCamBtn.onClick = [this] { refreshCameraDevices(); };
            loadOnnxBtn.onClick = [this]
            {
                chooser = std::make_unique<juce::FileChooser> ("Load ONNX Pose / Face Model (.onnx)", juce::File(), "*.onnx");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this] (const juce::FileChooser& fc)
                    {
                        if (fc.getResult() != juce::File())
                        {
                            engine.camera.loadOnnxModel (fc.getResult().getFullPathName());
                            engine.setStatus (engine.camera.getOnnxModelStatus());
                            repaint();
                        }
                    });
            };
            in.addRow ({ &camDeviceBox });
            in.addRow ({ &refreshCamBtn, &loadOnnxBtn });

            poseScope.painter = [this] (juce::Graphics& g, juce::Rectangle<float> box)
            {
                const auto lm = engine.camera.getLandmarks();
                if (! lm.active)
                {
                    g.setColour (avtheme::textMuted);
                    g.setFont (avtheme::font (12.0f));
                    g.drawText ("Camera off - choose a device above", box.toNearestInt(), juce::Justification::centred);
                    return;
                }
                auto mapPt = [&] (float nx, float ny) -> juce::Point<float>
                {
                    return { box.getX() + nx * box.getWidth(), box.getBottom() - ny * box.getHeight() };
                };
                const auto pHead = mapPt (lm.headX, lm.headY), pTorso = mapPt (lm.torsoX, lm.torsoY);
                const auto pLeft = mapPt (lm.leftHandX, lm.leftHandY), pRight = mapPt (lm.rightHandX, lm.rightHandY);
                g.setColour (avtheme::accentCyan.withAlpha (0.75f));
                g.drawLine (pHead.x, pHead.y, pTorso.x, pTorso.y, 2.0f);
                g.drawLine (pTorso.x, pTorso.y, pLeft.x, pLeft.y, 2.0f);
                g.drawLine (pTorso.x, pTorso.y, pRight.x, pRight.y, 2.0f);
                g.setColour (avtheme::accentAmber);
                g.drawEllipse (pHead.x - 10.0f, pHead.y - 10.0f, 20.0f, 20.0f, 2.0f);
                g.setColour (avtheme::accentGreen);
                g.fillEllipse (pLeft.x - 5.0f, pLeft.y - 5.0f, 10.0f, 10.0f);
                g.fillEllipse (pRight.x - 5.0f, pRight.y - 5.0f, 10.0f, 10.0f);
            };
            in.addRow ({ &poseScope }, 140);

            motionMeter.painter = [this] (juce::Graphics& g, juce::Rectangle<float> r)
            {
                const auto lm = engine.camera.getLandmarks();
                const float mot = engine.bus.get (engine.bus.find ("cam.motion"));
                g.setColour (avtheme::accentGreen.withAlpha (0.5f));
                g.fillRoundedRectangle (r.reduced (2.0f).withWidth ((r.getWidth() - 4.0f) * juce::jlimit (0.0f, 1.0f, mot)), 4.0f);
                g.setColour (avtheme::textBright);
                g.setFont (avtheme::font (11.0f));
                g.drawText ("Motion  |  Spread " + juce::String (lm.handSpread, 2) + "  Lean " + juce::String (lm.bodyLean, 2)
                            + "  Wave " + juce::String (lm.gestureWave, 2),
                            r.toNearestInt(), juce::Justification::centred);
            };
            in.addRow ({ &motionMeter }, 26);
        }

        auto& look = makeGroup ("Look", legacy() ? "Effects applied to the camera picture."
                                                  : "A second look at the camera picture (choose the camera on the main Webcam FX layer).");
        add (look, "mirror", "Mirror horizontally");
        add (look, "edgeDetect", "Neon edges");
        add (look, "posterize", "Posterize");
        add (look, "chromaKey", "Luma / chroma key");
        add (look, "hueShift", "Hue shift");
        add (look, "uvDisplace", "Audio warp");
        add (look, "scale", "Scale");
    }

    // ---- text ----
    void buildText()
    {
        auto& txt = makeGroup ("Text & font", "Type the words, then press Enter (or Apply). Any font installed on this computer works.");
        textInput.setTextToShowWhenEmpty ("Type your text...", avtheme::textMuted);
        textInput.setText (engine.visual.getLayerText (layerId), false);
        auto applyText = [this]
        {
            const auto t = textInput.getText();
            if (t != engine.visual.getLayerText (layerId))
                engine.performUndoable ("Change Text", [this, t] { engine.visual.setLayerText (layerId, t); });
        };
        textInput.onReturnKey = applyText;
        textInput.onFocusLost = applyText;
        applyTextBtn.onClick  = applyText;
        txt.addRow ({ &textInput, &applyTextBtn }, 30, { 3.0f, 1.0f });

        fontBox.addItem ("Default (sans-serif)", 1);
        fontBox.addItem ("Serif", 2);
        fontBox.addItem ("Monospaced", 3);
        fontBox.addSeparator();
        const auto& fonts = avlayers::systemFonts();
        for (int i = 0; i < fonts.size(); ++i)
            fontBox.addItem (fonts[i], 100 + i);
        fontBox.setTooltip ("Font family");
        fontBox.onChange = [this]
        {
            const int id = fontBox.getSelectedId();
            juce::String name;
            if (id == 2)        name = juce::Font::getDefaultSerifFontName();
            else if (id == 3)   name = juce::Font::getDefaultMonospacedFontName();
            else if (id >= 100) name = fontBox.getText();
            if (name != engine.visual.getLayerFont (layerId))
                engine.performUndoable ("Change Font", [this, name] { engine.visual.setLayerFont (layerId, name); });
            shownFont = name;
            fontPreview.repaint();
        };
        txt.addRow ({ &fontBox }, 30);

        fontPreview.painter = [this] (juce::Graphics& g, juce::Rectangle<float> r)
        {
            const auto name = engine.visual.getLayerFont (layerId);
            const bool bold = engine.visual.getBase (prefix + ".bold") >= 0.5f, italic = engine.visual.getBase (prefix + ".italic") >= 0.5f;
            const int style = (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0);
            g.setColour (avtheme::textBright);
            g.setFont (juce::Font (juce::FontOptions (name.isEmpty() ? juce::Font::getDefaultSansSerifFontName() : name, 24.0f, style)));
            g.drawText ("Aa Bb Cc 0123  " + engine.visual.getLayerText (layerId).substring (0, 18), r.reduced (10.0f, 0.0f).toNearestInt(),
                        juce::Justification::centredLeft, true);
        };
        txt.addRow ({ &fontPreview }, 44);

        add (txt, "bold", "Bold");
        add (txt, "italic", "Italic");
        add (txt, "size", "Size");
        add (txt, "posX", "Position X");
        add (txt, "posY", "Position Y");
        add (txt, "rotation", "Rotation");

        auto& sty = makeGroup ("Style & motion", "Bounce follows the beat; Slice Glitch tears the letters on transients.");
        add (sty, "extrude", "3D depth");
        add (sty, "glitch", "Slice glitch");
        add (sty, "hue", "Colour hue");
        add (sty, "audioBounce", "Audio bounce");
    }

    void buildSpectrum()
    {
        auto& st = makeGroup ("Display", "The 16 analysis bands, drawn in the style you choose.");
        add (st, "style", "Style", { "Bars", "Mirrored bars", "Radial burst", "Smooth line", "Dot matrix" });
        add (st, "gain", "Gain");
        add (st, "thickness", "Thickness");
        add (st, "glow", "Glow");

        auto& col = makeGroup ("Colour");
        add (col, "hue", "Hue");
        add (col, "hueSpread", "Hue spread across bands");

        auto& pos = makeGroup ("Placement");
        add (pos, "size", "Size");
        add (pos, "posY", "Vertical position");
        add (pos, "rotation", "Rotation");
    }

    void buildFill()
    {
        auto& st = makeGroup ("Pattern", "Two colours blended by the pattern. Pair it with Multiply or Overlay to tint what's below.");
        add (st, "style", "Style", { "Solid colour", "Linear gradient", "Radial gradient", "Angular sweep", "Stripes", "Checkerboard", "Rings" });
        add (st, "scale", "Scale");
        add (st, "angle", "Angle");
        add (st, "speed", "Scroll / spin speed");

        auto& col = makeGroup ("Colours");
        add (col, "hueA", "Colour A hue");
        add (col, "hueB", "Colour B hue");
        add (col, "saturation", "Saturation");
        add (col, "brightness", "Brightness");
        add (col, "audio", "Audio reaction");
    }

    void refreshCameraDevices()
    {
        camDeviceBox.clear (juce::dontSendNotification);
        camDeviceBox.addItem ("Webcam off", 1);
        const auto devs = CameraInput::getDevices();
        for (int i = 0; i < devs.size(); ++i)
            camDeviceBox.addItem (devs[i], i + 2);
        camDeviceBox.setSelectedId (engine.camera.isOpen() ? 2 : 1, juce::dontSendNotification);
    }

    void selectFontInBox (const juce::String& name)
    {
        if (name.isEmpty())                                         fontBox.setSelectedId (1, juce::dontSendNotification);
        else if (name == juce::Font::getDefaultSerifFontName())      fontBox.setSelectedId (2, juce::dontSendNotification);
        else if (name == juce::Font::getDefaultMonospacedFontName()) fontBox.setSelectedId (3, juce::dontSendNotification);
        else
        {
            const int idx = avlayers::systemFonts().indexOf (name);
            if (idx >= 0) fontBox.setSelectedId (100 + idx, juce::dontSendNotification);
            else          fontBox.setText (name, juce::dontSendNotification);
        }
    }

    Engine& engine;
    int layerId;
    LayerType type;
    juce::String prefix;
    std::function<void()> changed;
    int seenShaderVersion = -1;
    juce::String shownFont { "\x01" }; // forces the first sync to select the stored font

    FlowPanel flow;
    std::vector<std::unique_ptr<ParamGroup>> groups;
    std::vector<ParamControl*> ctrls;

    juce::TextButton loadGltfBtn { "Load glTF / GLB model..." };
    juce::TextEditor shaderEditor;
    juce::TextButton compileShaderBtn { "Compile & apply" }, loadShaderFileBtn { "Load .glsl file..." };
    juce::TextButton recLoopBtn { "Record / stop loop" }, clearLoopBtn { "Clear loop" }, loadVideoBtn { "Load video or image..." };
    LiveStrip loopMeter, poseScope, motionMeter, fontPreview;
    juce::ComboBox camDeviceBox, fontBox;
    juce::TextButton refreshCamBtn { "Refresh cameras" }, loadOnnxBtn { "Load ONNX pose model..." };
    juce::TextEditor textInput;
    juce::TextButton applyTextBtn { "Apply" };
    std::unique_ptr<juce::FileChooser> chooser;
};

// ==============================================================================
// COMPOSE page: layer stack (left) + inspector for the selected layer (right)
// ==============================================================================
class LayersStudioPanel final : public juce::Component
{
public:
    explicit LayersStudioPanel (Engine& e) : engine (e)
    {
        addAndMakeVisible (addBtn);
        addAndMakeVisible (rowViewport);
        rowViewport.setViewedComponent (&rowsContent, false);
        rowViewport.setScrollBarsShown (true, false);
        rowViewport.setScrollBarThickness (8);

        addBtn.setTooltip ("Add another layer. You choose what kind: 3D scene, 2D shader, spectrum analyzer, gradient, video, webcam or text.");
        addBtn.onClick = [this] { showAddMenu(); };

        refreshLayers();
        selectLayer (engine.visual.getLayerStack().ids[0]);
    }

    int getSelectedLayer() const noexcept { return selectedLayer; }

    void selectLayer (int lid)
    {
        if (! engine.visual.isLayerActive (lid))
            return;
        selectedLayer = lid;
        ensureInspector (lid);
        for (int i = 0; i < VisualState::kMaxLayers; ++i)
            if (inspectors[(size_t) i] != nullptr)
                inspectors[(size_t) i]->setVisible (i == lid);
        resized();
        syncAll();
        repaint();
    }

    // Jump to the control for a parameter id (used by the global search). Returns false if not on this page.
    bool revealParam (const juce::String& id)
    {
        const int idx = engine.visual.findParam (id);
        if (idx < 0 || ! engine.visual.isParamAvailable (idx))
            return false;
        const int lid = engine.visual.getParamInfo (idx).layerId;
        if (lid < 0)
            return false;
        selectLayer (lid);
        return inspectors[(size_t) lid] != nullptr && inspectors[(size_t) lid]->reveal (id);
    }

    void syncAll()
    {
        if (engine.visual.layerRevision.load() != seenLayerRev)
            refreshLayers();
        for (auto& r : rows) r->sync();
        if (auto* insp = inspectors[(size_t) selectedLayer].get())
            insp->syncFromEngine();

        const bool on = engine.visual.getBase (engine.visual.getLayerPrefix (selectedLayer) + ".enabled") >= 0.5f;
        if (on != lastEnabledShown)
        {
            lastEnabledShown = on;
            repaint (getInspectorHeaderBounds());
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft (kStackWidth);
        avtheme::drawCard (g, left, "Layer stack", avtheme::accentCyan,
                           "Top row draws on top. Switch layers on/off, reorder with the arrows, and set how each blends with what's below.");

        const auto hdr = getInspectorHeaderBounds();
        const auto type = engine.visual.getLayerType (selectedLayer);
        g.setColour (avlayers::colourFor (type));
        g.fillRoundedRectangle ((float) hdr.getX() + 4.0f, (float) hdr.getY() + 6.0f, 4.0f, 34.0f, 2.0f);
        g.setColour (avtheme::textBright);
        g.setFont (avtheme::font (17.0f, true));
        g.drawText (engine.visual.getLayerName (selectedLayer), hdr.getX() + 18, hdr.getY() + 4, hdr.getWidth() - 220, 22, juce::Justification::centredLeft);
        g.setColour (avtheme::textMuted);
        g.setFont (avtheme::font (12.0f));
        g.drawText (avlayers::blurbFor (type), hdr.getX() + 18, hdr.getY() + 26, hdr.getWidth() - 24, 18, juce::Justification::centredLeft, true);

        if (engine.visual.getBase (engine.visual.getLayerPrefix (selectedLayer) + ".enabled") < 0.5f)
        {
            auto chip = juce::Rectangle<float> ((float) hdr.getRight() - 186.0f, (float) hdr.getY() + 6.0f, 178.0f, 22.0f);
            g.setColour (avtheme::accentAmber.withAlpha (0.16f));
            g.fillRoundedRectangle (chip, 11.0f);
            g.setColour (avtheme::accentAmber);
            g.setFont (avtheme::font (11.5f, true));
            g.drawText ("Layer is OFF - use its switch", chip.toNearestInt(), juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto left = b.removeFromLeft (kStackWidth).reduced (10, 0);
        left.removeFromTop (66); // card header incl. hint
        addBtn.setBounds (left.removeFromTop (30));
        left.removeFromTop (6);
        left.removeFromBottom (8);
        rowViewport.setBounds (left);
        layoutRows();

        auto right = b.withTrimmedLeft (6);
        right.removeFromTop (kInspectorHeader);
        for (auto& i : inspectors)
            if (i != nullptr)
                i->setBounds (right);
    }

private:
    static constexpr int kStackWidth = 300, kInspectorHeader = 52, kRowHeight = 78;

    struct LayerRow final : public juce::Component
    {
        LayerRow (LayersStudioPanel& o, int fromTop)
            : owner (o), rowIdx (fromTop), upBtn ("up", 0.75f, avtheme::textMuted), downBtn ("down", 0.25f, avtheme::textMuted)
        {
            addAndMakeVisible (enableBtn);
            addAndMakeVisible (upBtn);
            addAndMakeVisible (downBtn);
            addAndMakeVisible (removeBtn);
            addAndMakeVisible (blendBox);
            addAndMakeVisible (opacitySlider);

            enableBtn.setTooltip ("Show / hide this layer");
            upBtn.setTooltip ("Move layer up (draws later, on top)");
            downBtn.setTooltip ("Move layer down");
            removeBtn.setTooltip ("Remove this layer");
            blendBox.setTooltip ("Blend mode: how this layer mixes with the layers below it");
            opacitySlider.setTooltip ("Layer opacity");

            blendBox.addItemList ({ "Add", "Screen", "Multiply", "Overlay", "Alpha", "Difference" }, 1);
            opacitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
            opacitySlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            opacitySlider.setRange (0.0, 1.0, 0.01);
            opacitySlider.setScrollWheelEnabled (false);

            enableBtn.onClick = [this]
            {
                const auto pre = prefixOf (getLayerId());
                const float v = enableBtn.getToggleState() ? 1.0f : 0.0f;
                owner.engine.performUndoable ("Toggle Layer", [this, pre, v] { owner.engine.visual.setBase (pre + ".enabled", v); });
            };
            upBtn.onClick = [this]   { move (+1); };
            downBtn.onClick = [this] { move (-1); };
            removeBtn.onClick = [this]
            {
                const int id = getLayerId();
                owner.engine.performUndoable ("Remove Layer", [this, id] { owner.engine.visual.removeLayer (id); });
                owner.refreshLayers();
            };
            blendBox.onChange = [this]
            {
                const auto pre = prefixOf (getLayerId());
                const float v = (float) (blendBox.getSelectedId() - 1);
                if (std::abs (v - owner.engine.visual.getBase (pre + ".blend")) > 0.01f)
                    owner.engine.performUndoable ("Change Blend Mode", [this, pre, v] { owner.engine.visual.setBase (pre + ".blend", v); });
            };
            opacitySlider.onDragStart = [this] { dragStart = (float) opacitySlider.getValue(); };
            opacitySlider.onValueChange = [this]
            {
                owner.engine.visual.setBase (prefixOf (getLayerId()) + ".opacity", (float) opacitySlider.getValue());
            };
            opacitySlider.onDragEnd = [this]
            {
                const auto pre = prefixOf (getLayerId());
                const float endVal = (float) opacitySlider.getValue();
                if (std::abs (endVal - dragStart) > 1.0e-4f)
                {
                    owner.engine.visual.setBase (pre + ".opacity", dragStart);
                    owner.engine.performUndoable ("Layer Opacity", [this, pre, endVal] { owner.engine.visual.setBase (pre + ".opacity", endVal); });
                }
            };
            sync();
        }

        juce::String prefixOf (int id) const { return owner.engine.visual.getLayerPrefix (id); }

        int getLayerId() const
        {
            const auto st = owner.engine.visual.getLayerStack();
            const int slot = st.count - 1 - rowIdx;
            return slot >= 0 && slot < st.count ? st.ids[(size_t) slot] : -1;
        }

        void move (int delta)
        {
            const auto st = owner.engine.visual.getLayerStack();
            const int slot = st.count - 1 - rowIdx, to = slot + delta;
            if (slot < 0 || to < 0 || to >= st.count)
                return;
            owner.engine.performUndoable (delta > 0 ? "Move Layer Up" : "Move Layer Down",
                                          [this, slot, to] { owner.engine.visual.moveLayer (slot, to); });
            owner.syncAll();
        }

        void sync()
        {
            const int lid = getLayerId();
            if (lid < 0)
                return;
            const auto pre = prefixOf (lid);
            const bool on = owner.engine.visual.getBase (pre + ".enabled") >= 0.5f;
            enableBtn.setToggleState (on, juce::dontSendNotification);
            blendBox.setSelectedId ((int) std::round (owner.engine.visual.getBase (pre + ".blend")) + 1, juce::dontSendNotification);
            if (! opacitySlider.isMouseButtonDown())
                opacitySlider.setValue (owner.engine.visual.getBase (pre + ".opacity"), juce::dontSendNotification);
            removeBtn.setVisible (! VisualState::isLegacyLayer (lid));

            // The child widgets repaint themselves; the row only repaints when what it draws itself changes.
            const bool sel = (owner.selectedLayer == lid);
            if (sel != lastSelected || lid != lastLid || on != lastOn)
            {
                lastSelected = sel;
                lastLid = lid;
                lastOn = on;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            const int lid = getLayerId();
            if (lid < 0)
                return;
            const auto type = owner.engine.visual.getLayerType (lid);
            const bool sel = (owner.selectedLayer == lid);
            const auto col = avlayers::colourFor (type);
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (sel ? col.withAlpha (0.13f) : avtheme::bgDark);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (sel ? col.withAlpha (0.8f) : avtheme::borderSoft);
            g.drawRoundedRectangle (r, 8.0f, sel ? 1.5f : 1.0f);

            g.setColour (lastOn ? col : avtheme::textMuted.withAlpha (0.4f));
            g.fillRoundedRectangle (r.getX() + 1.5f, r.getY() + 8.0f, 4.0f, r.getHeight() - 16.0f, 2.0f);

            g.setColour (lastOn ? avtheme::textBright : avtheme::textMuted);
            g.setFont (avtheme::font (13.5f, true));
            g.drawText (owner.engine.visual.getLayerName (lid), titleBounds, juce::Justification::centredLeft, true);
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            const int lid = getLayerId();
            if (lid >= 0)
                owner.selectLayer (lid);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12, 6);
            auto top = r.removeFromTop (juce::jmax (22, r.getHeight() / 2 - 2));
            enableBtn.setBounds (top.removeFromLeft (38));
            removeBtn.setBounds (top.removeFromRight (24).reduced (0, 2));
            top.removeFromRight (2);
            downBtn.setBounds (top.removeFromRight (24).withSizeKeepingCentre (14, 10));
            top.removeFromRight (4);
            upBtn.setBounds (top.removeFromRight (24).withSizeKeepingCentre (14, 10));
            titleBounds = top.withTrimmedLeft (4);
            r.removeFromTop (4);
            blendBox.setBounds (r.removeFromLeft (96).reduced (0, 1));
            r.removeFromLeft (8);
            opacitySlider.setBounds (r);
        }

        LayersStudioPanel& owner;
        int rowIdx;
        bool lastSelected = false, lastOn = false;
        int lastLid = -2;
        float dragStart = 0.0f;
        juce::Rectangle<int> titleBounds;
        juce::ToggleButton enableBtn;
        juce::ArrowButton upBtn, downBtn;
        juce::TextButton removeBtn { "x" };
        juce::ComboBox blendBox;
        juce::Slider opacitySlider;
    };

    struct RowsContent final : public juce::Component {};

    juce::Rectangle<int> getInspectorHeaderBounds() const
    {
        return getLocalBounds().withTrimmedLeft (kStackWidth + 6).withHeight (kInspectorHeader);
    }

    void layoutRows()
    {
        const int w = rowViewport.getWidth() - rowViewport.getScrollBarThickness() - 2;
        rowsContent.setSize (juce::jmax (100, w), (int) rows.size() * kRowHeight);
        for (size_t i = 0; i < rows.size(); ++i)
            rows[i]->setBounds (0, (int) i * kRowHeight, rowsContent.getWidth(), kRowHeight - 4);
    }

    void showAddMenu()
    {
        if (engine.visual.getLayerStack().count >= VisualState::kMaxLayers)
        {
            engine.setStatus ("All " + juce::String (VisualState::kMaxLayers) + " layer slots are in use - remove one first");
            return;
        }
        juce::PopupMenu m;
        m.addSectionHeader ("Add a layer");
        for (int t = 0; t < (int) LayerType::Count; ++t)
            m.addItem (t + 1, avlayers::titleFor ((LayerType) t) + "   -   " + avlayers::blurbFor ((LayerType) t).upToFirstOccurrenceOf (".", false, false));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addBtn), [this] (int res)
        {
            if (res < 1)
                return;
            int newId = -1;
            engine.performUndoable ("Add Layer", [this, res, &newId] { newId = engine.visual.addLayer ((LayerType) (res - 1)); });
            refreshLayers();
            if (newId >= 0)
            {
                selectLayer (newId);
                engine.setStatus ("Added " + engine.visual.getLayerName (newId) + " on top of the stack");
            }
        });
    }

    void ensureInspector (int lid)
    {
        auto& slot = inspectors[(size_t) lid];
        if (slot != nullptr && slot->getType() != engine.visual.getLayerType (lid))
            slot.reset();
        if (slot == nullptr)
        {
            slot = std::make_unique<LayerInspector> (engine, lid, [this] { syncAll(); });
            addChildComponent (*slot);
            slot->setBounds (getLocalBounds().withTrimmedLeft (kStackWidth + 6).withTrimmedTop (kInspectorHeader));
        }
    }

    // Layers were added / removed / reloaded: rebuild the row list and drop inspectors of layers that are gone
    void refreshLayers()
    {
        seenLayerRev = engine.visual.layerRevision.load();
        const int count = engine.visual.getLayerStack().count;
        if ((int) rows.size() != count)
        {
            rows.clear();
            for (int i = 0; i < count; ++i)
            {
                auto r = std::make_unique<LayerRow> (*this, i);
                rowsContent.addAndMakeVisible (*r);
                rows.push_back (std::move (r));
            }
            layoutRows();
        }
        for (int i = 0; i < VisualState::kMaxLayers; ++i)
            if (inspectors[(size_t) i] != nullptr && inspectors[(size_t) i]->getType() != engine.visual.getLayerType (i))
                inspectors[(size_t) i].reset();

        if (! engine.visual.isLayerActive (selectedLayer))
        {
            const auto st = engine.visual.getLayerStack();
            selectedLayer = st.ids[(size_t) juce::jmax (0, st.count - 1)];
        }
        if (selectedLayer >= 0 && inspectors[(size_t) selectedLayer] == nullptr && isShowing())
            selectLayer (selectedLayer);
        repaint();
    }

    Engine& engine;
    int selectedLayer = 0;
    int seenLayerRev = -1;
    bool lastEnabledShown = true;

    juce::TextButton addBtn { "+  Add layer..." };
    juce::Viewport rowViewport;
    RowsContent rowsContent;
    std::vector<std::unique_ptr<LayerRow>> rows;
    std::array<std::unique_ptr<LayerInspector>, VisualState::kMaxLayers> inspectors;
};

// ==============================================================================
// STYLE page: post-processing and colour grade applied to the whole mix
// ==============================================================================
class EffectsRackPanel final : public juce::Component
{
public:
    explicit EffectsRackPanel (Engine& e) : engine (e)
    {
        addAndMakeVisible (flow);

        auto& fb = group ("Feedback & echo", "Each frame is mixed with the previous one for trails and tunnels.", avtheme::accentCyan);
        add (fb, "post.feedback", "Feedback mix");
        add (fb, "post.feedbackZoom", "Zoom per frame");
        add (fb, "post.feedbackRotate", "Spin per frame");

        auto& bl = group ("Bloom & tonemap", "Bright areas glow; ACES tonemap keeps highlights from clipping.", avtheme::accentAmber);
        add (bl, "post.bloom", "Bloom glow");
        add (bl, "post.threshold", "Bloom threshold");
        add (bl, "post.tonemap", "ACES tonemap");

        auto& kal = group ("Kaleidoscope & geometry", "Mirror and fold the picture. Kaleido sides of 0 turns it off.", avtheme::accentPurple);
        add (kal, "post.kaleidoscope", "Kaleido sides");
        add (kal, "post.mirrorQuad", "Quad mirror");
        add (kal, "post.pixelate", "Pixel mosaic");

        auto& warp = group ("Warp & blur", "Bend the whole picture: swirl it, ripple it, or smear it outward from the centre.", avtheme::accentGreen);
        add (warp, "post.twirl", "Twirl / swirl");
        add (warp, "post.ripple", "Ripple warp");
        add (warp, "post.zoomBlur", "Zoom blur");

        auto& gl = group ("Glitch, optics & CRT", "Digital damage and lens effects.", avtheme::accentRed);
        add (gl, "post.glitch", "Digital glitch");
        add (gl, "post.rgbSplit", "Chromatic split");
        add (gl, "post.scanlines", "CRT scanlines");
        add (gl, "post.vignette", "Vignette");
        add (gl, "post.grain", "Film grain");

        auto& look = group ("Colour looks", "Reduce the palette or tint shadows and highlights with two colours.", avtheme::accentPurple);
        add (look, "post.posterize", "Posterize colours");
        add (look, "post.duotone", "Duotone mix");
        add (look, "post.duoHueA", "Duotone shadow hue");
        add (look, "post.duoHueB", "Duotone highlight hue");

        auto& col = group ("Colour grade & output", "Final colour correction. Master Output is the overall brightness.", avtheme::accentGreen);
        add (col, "post.hueShift", "Hue rotate");
        add (col, "post.saturation", "Saturation");
        add (col, "post.contrast", "Contrast");
        add (col, "post.strobe", "Invert strobe");
        add (col, "post.masterDimmer", "Master output");

        auto& glob = group ("Global", "Speed multiplier for every animated layer. Modulate it for tempo-driven slow-downs and surges.", avtheme::accentAmber);
        add (glob, "global.speed", "Animation speed");
    }

    void syncAll() { flow.syncFromEngine(); }

    bool revealParam (const juce::String& id)
    {
        for (auto* c : ctrls)
            if (c->getParamId() == id)
            {
                flow.reveal (*c);
                c->flash();
                return true;
            }
        return false;
    }

    void resized() override { flow.setBounds (getLocalBounds()); }

private:
    ParamGroup& group (const juce::String& title, const juce::String& hint, juce::Colour c)
    {
        groups.push_back (std::make_unique<ParamGroup> (engine, title, hint, c));
        flow.addGroup (*groups.back());
        return *groups.back();
    }

    void add (ParamGroup& g, const char* id, const char* label) { ctrls.push_back (g.addParam (id, label)); }

    Engine& engine;
    FlowPanel flow;
    std::vector<std::unique_ptr<ParamGroup>> groups;
    std::vector<ParamControl*> ctrls;
};
