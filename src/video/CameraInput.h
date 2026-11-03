#pragma once
#include "../core/SignalBus.h"
#include <juce_video/juce_video.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

struct PoseLandmarks
{
    float headX = 0.5f, headY = 0.72f, headRadius = 0.1f;
    float leftHandX = 0.25f, leftHandY = 0.5f;
    float rightHandX = 0.75f, rightHandY = 0.5f;
    float torsoX = 0.5f, torsoY = 0.35f;
    float handSpread = 0.5f, bodyLean = 0.5f, gestureWave = 0.0f, proximity = 0.3f;
    bool active = false;
};

// Webcam capture + dedicated vision worker thread for body pose, left/right hand gesture,
// head/face tracking, foreground segmentation, and optional dynamic ONNX model loading.
#if JUCE_USE_CAMERA
 using CameraListenerBase = juce::CameraDevice::Listener;
 #define AV_CAMERA_OVERRIDE override
#else
 // JUCE has no camera support on this platform (Linux): the webcam features compile to inert stubs.
 struct CameraListenerBase {};
 #define AV_CAMERA_OVERRIDE
#endif

class CameraInput final : private CameraListenerBase,
                          private juce::Thread
{
public:
    explicit CameraInput (SignalBus&);
    ~CameraInput() override;

    static juce::StringArray getDevices()
    {
       #if JUCE_USE_CAMERA
        return juce::CameraDevice::getAvailableDevices();
       #else
        return {};
       #endif
    }
    void open (int deviceIndex);   // message thread
    void close();                  // message thread
    bool isOpen() const noexcept
    {
       #if JUCE_USE_CAMERA
        return device != nullptr;
       #else
        return false;
       #endif
    }

    // Optional on-demand ONNX model path (for external pose/face models)
    bool loadOnnxModel (const juce::String& modelPath);
    juce::String getOnnxModelStatus() const;

    // Zero-copy shared_ptr fetch of the latest frame if newer than `version`. Safe from any thread.
    bool fetchFrame (uint32_t& version, std::shared_ptr<const std::vector<uint8_t>>& rgba, int& w, int& h);

    PoseLandmarks getLandmarks() const noexcept;

private:
    void imageReceived (const juce::Image&) AV_CAMERA_OVERRIDE;
    void run() override;
    void analyzeVisionFrame (const std::vector<uint8_t>& rgba, int w, int h);

    SignalBus& bus;
    int sActive, sMotion, sX, sY, sBright;
    int sHeadX, sHeadY, sLeftX, sLeftY, sRightX, sRightY, sSpread, sLean, sWave, sProx;

   #if JUCE_USE_CAMERA
    std::unique_ptr<juce::CameraDevice> device;
   #endif
    mutable juce::CriticalSection frameLock;
    std::shared_ptr<const std::vector<uint8_t>> frame;
    int frameW = 0, frameH = 0;
    uint32_t frameVersion = 0, analyzedVersion = 0;

    static constexpr int gw = 64, gh = 36;
    float prevLuma[gw * gh] {};
    bool havePrev = false;
    float trackX = 0.5f, trackY = 0.5f, motionSmooth = 0.0f;
    PoseLandmarks landmarks;
    float prevLeftX = 0.25f, prevRightX = 0.75f, waveEnergy = 0.0f;

    juce::String onnxModelPath, onnxStatus { "Built-in Optical Pose & Face Tracker Active" };
    int openRequestId = 0;
};
