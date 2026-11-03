#pragma once
#include "../core/SignalBus.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

// Asynchronous hardware-accelerated video file decoder (Windows Media Foundation with
// MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS for H.264/H.265/MP4/MOV/AVI/WMV, plus still image
// and procedural fallback on other platforms). Decodes on a background worker thread into a
// pre-scaled RGBA ring buffer so the GL thread can scrub, reverse, stutter, and tempo-sync
// with zero stalls!
class VideoDecoder final : private juce::Thread
{
public:
    explicit VideoDecoder (SignalBus& bus);
    ~VideoDecoder() override;

    bool loadFile (const juce::String& path);
    void unload();

    juce::String getLoadedPath() const;
    bool isLoaded() const noexcept { return numFrames.load (std::memory_order_relaxed) > 0; }
    int getFrameCount() const noexcept { return numFrames.load (std::memory_order_relaxed); }
    float getVideoFps() const noexcept { return videoFps.load (std::memory_order_relaxed); }

    // Zero-copy fetch of frame at normalized position `normPos` (0..1) if different from `lastFrameIdx`.
    bool fetchFrameAt (double normPos, int& lastFrameIdx, std::shared_ptr<const std::vector<uint8_t>>& rgbaOut, int& w, int& h);

private:
    void run() override;
    bool decodeVideoFile (const juce::String& path);
    bool decodeStillImage (const juce::File& file);
    static void fitWithin (int srcW, int srcH, int& w, int& h) noexcept; // aspect-preserving downscale to kMaxW x kMaxH

    SignalBus& bus;
    int sVidLoaded = -1, sVidPos = -1;

    mutable juce::CriticalSection lock;
    juce::String currentPath, pendingPath;
    std::atomic<bool> loadRequested { false };

    // Decoded RGBA frame cache (up to kMaxCachedFrames at <= 480x270 for instant reverse/stutter scrubbing)
    static constexpr int kMaxCachedFrames = 180;
    static constexpr int kMaxW = 480, kMaxH = 270;
    std::vector<std::shared_ptr<const std::vector<uint8_t>>> frames;
    int frameW = 0, frameH = 0;
    std::atomic<int> numFrames { 0 };
    std::atomic<float> videoFps { 30.0f };
};
