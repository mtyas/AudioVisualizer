#pragma once
#include "../core/Engine.h"
#include "GltfLoader.h"
#include "ShaderProg.h"
#include <juce_opengl/juce_opengl.h>
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

// Multi-layer OpenGL 3.2 core pipeline:
//   5 composable layers (3D Scene, 2D Shader/Custom GLSL, Video/Loop, Webcam FX, 3D Text)
//   -> ordered blend compositor -> Feedback & Post-FX -> HDR Bloom -> Master Grading.
class Renderer final : public juce::OpenGLRenderer
{
public:
    Renderer (Engine&, juce::OpenGLContext&);

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    struct Fbo
    {
        unsigned int fbo = 0, tex = 0, depth = 0;
        int w = 0, h = 0;
        void create (int width, int height, bool withDepth);
        void bind() const;
        void release();
    };
    struct GpuMesh
    {
        unsigned int vbo = 0, ebo = 0;
        int indexCount = 0;
        void upload (const MeshData&);
        void release();
    };
    struct Uniform { const char* glName; const char* signal; int index; };

    std::unique_ptr<juce::OpenGLShaderProgram> build (const char* vs, const juce::String& fs, juce::String& err);
    void rebuildLayer();
    void loadModel();
    void updateTextTextures();
    void resize (int w, int h);
    void checkReload();
    void drawFullscreen();
    void updateCameraAndVideo (float dt, float speed, float inPt, float outPt, bool rev, bool tempoSync, float syncBeats, float stutter);

    static constexpr int kLoopW = 320, kLoopH = 180, kLoopFrames = 120;
    unsigned int camTex = 0, loopTex = 0, loopFbo = 0, vidFileTex = 0;
    int camW = 0, camH = 0, vidW = 0, vidH = 0, lastVidFrameIdx = -1;
    uint32_t camVersion = 0;
    double vidNormPos = 0.0;
    // One rasterised texture per text layer (keyed by text + font + style so it only re-renders on change)
    struct TextTex { unsigned int tex = 0; juce::String key; };
    TextTex textTexs[VisualState::kMaxLayers];

    ShaderProg sceneProg, proc2dProg, customLayerProg,
        videoProg, webcamProg, textProg, spectrumProg, fillProg, blendProg, postFxProg, blitProg, brightProg, blurProg, compProg;

    // Param index cache keyed on literal pointer: P("scene.pulse") resolves the string once.
    std::unordered_map<const char*, int> paramIdxCache;
    // Per-layer parameter lookup: LP("pulse") resolves "<prefix of the current layer>.pulse" and caches the index.
    std::array<std::unordered_map<const char*, int>, VisualState::kMaxLayers> layerParamCache;
    std::array<juce::String, VisualState::kMaxLayers> layerPrefixes;
    int curLayer = 0, seenLayerRev = -1;
    float LP (const char* suffix)
    {
        auto& cache = layerParamCache[(size_t) curLayer];
        auto it = cache.find (suffix);
        if (it == cache.end())
            it = cache.emplace (suffix, engine.visual.findParam (layerPrefixes[(size_t) curLayer] + "." + suffix)).first;
        return engine.visual.getModulated (it->second);
    }

    double animTime = 0.0; // advances by dt * global.speed

    float P (const char* id)
    {
        auto it = paramIdxCache.find (id);
        if (it == paramIdxCache.end())
            it = paramIdxCache.emplace (id, engine.visual.findParam (id)).first;
        return engine.visual.getModulated (it->second);
    }

    std::shared_ptr<const std::vector<uint8_t>> camFrame, vidFrame;

    bool loopRecording = false;
    int loopLen = 0, loopWrite = 0;
    double loopPos = 0.0, lastCamTime = 0.0;
    float camFps = 30.0f;

    Engine& engine;
    juce::OpenGLContext& context;

    unsigned int vao = 0;
    static constexpr int kNumMeshes = 12, kGltfMesh = 11;
    std::array<GpuMesh, kNumMeshes> meshes; // see makeMeshes(): 0 Sphere .. 10 Disc, 11 = glTF
    Fbo layerFbo, canvas[2], feedback[2], bloom[2];
    int fbCur = 0, width = 0, height = 0;
    long long frame = 0;
    double lastTime = 0.0, startTime = 0.0;
    float fpsSmooth = 60.0f;

    bool userFileShader = false;
    juce::Time shaderMTime;
    int seenShaderVersion = -1, seenInlineVersion = -1, seenModelVersion = 0;

    std::vector<Uniform> floatUniforms;
    int bandIdx[16] {}, noteIdx[128] {}, ccIdx[128] {};
    int idxRms = -1, idxEnv = -1, idxTrans = -1, idxCent = -1, idxBeat = -1;
};
