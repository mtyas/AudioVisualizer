#include "VideoDecoder.h"
#include <cmath>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <mfapi.h>
 #include <mfidl.h>
 #include <mfreadwrite.h>
 #pragma comment(lib, "mfplat.lib")
 #pragma comment(lib, "mfreadwrite.lib")
 #pragma comment(lib, "mfuuid.lib")
#endif

VideoDecoder::VideoDecoder (SignalBus& b)
    : juce::Thread ("AVVideoDecoder"), bus (b)
{
    sVidLoaded = bus.add ("video.loaded");
    sVidPos    = bus.add ("video.pos");
}

VideoDecoder::~VideoDecoder()
{
    stopThread (2000);
}

bool VideoDecoder::loadFile (const juce::String& path)
{
    if (path.isEmpty())
    {
        unload();
        return false;
    }
    {
        const juce::ScopedLock sl (lock);
        pendingPath = path;
        loadRequested = true;
    }
    if (! isThreadRunning())
        startThread (juce::Thread::Priority::normal);
    notify();
    return true;
}

void VideoDecoder::unload()
{
    const juce::ScopedLock sl (lock);
    pendingPath.clear();
    currentPath.clear();
    loadRequested = false;
    frames.clear();
    numFrames.store (0, std::memory_order_relaxed);
    bus.set (sVidLoaded, 0.0f);
}

juce::String VideoDecoder::getLoadedPath() const
{
    // Report a requested-but-still-decoding file too, so state snapshots taken right after
    // loadFile() (undo, preset save) record it.
    const juce::ScopedLock sl (lock);
    return pendingPath.isNotEmpty() ? pendingPath : currentPath;
}

void VideoDecoder::fitWithin (int srcW, int srcH, int& w, int& h) noexcept
{
    const double s = juce::jmin (1.0, (double) kMaxW / srcW, (double) kMaxH / srcH);
    w = juce::jmax (1, (int) std::round (srcW * s));
    h = juce::jmax (1, (int) std::round (srcH * s));
}

bool VideoDecoder::fetchFrameAt (double normPos, int& lastFrameIdx, std::shared_ptr<const std::vector<uint8_t>>& rgbaOut, int& w, int& h)
{
    const int count = numFrames.load (std::memory_order_relaxed);
    if (count <= 0)
        return false;

    normPos = normPos - std::floor (normPos);
    const int idx = juce::jlimit (0, count - 1, (int) (normPos * count));
    bus.set (sVidPos, (float) normPos);
    if (idx == lastFrameIdx)
        return false;

    const juce::ScopedLock sl (lock);
    if (idx < 0 || idx >= (int) frames.size())
        return false;
    rgbaOut = frames[(size_t) idx];
    w = frameW;
    h = frameH;
    lastFrameIdx = idx;
    return true;
}

void VideoDecoder::run()
{
    while (! threadShouldExit())
    {
        if (! loadRequested.exchange (false))
        {
            wait (200);
            continue;
        }

        juce::String path;
        {
            const juce::ScopedLock sl (lock);
            path = pendingPath;
        }
        if (path.isEmpty())
            continue;

        const juce::File f (path);
        if (! f.existsAsFile())
            continue;

        const auto ext = f.getFileExtension().toLowerCase();
        bool ok = false;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".bmp")
            ok = decodeStillImage (f);
        else
            ok = decodeVideoFile (path);

        if (! ok)
            ok = decodeStillImage (f);

        bus.set (sVidLoaded, ok ? 1.0f : 0.0f);
    }
}

bool VideoDecoder::decodeStillImage (const juce::File& file)
{
    auto img = juce::ImageFileFormat::loadFrom (file);
    if (! img.isValid())
        return false;

    const int sw = img.getWidth(), sh = img.getHeight();
    int w = 0, h = 0;
    fitWithin (sw, sh, w, h);
    auto scaled = img.rescaled (w, h, juce::Graphics::mediumResamplingQuality);

    auto rgba = std::make_shared<std::vector<uint8_t>> ((size_t) w * (size_t) h * 4);
    const juce::Image::BitmapData bd (scaled, juce::Image::BitmapData::readOnly);
    const int ps = bd.pixelStride;
    for (int y = 0; y < h; ++y)
    {
        const int dstY = h - 1 - y;
        const auto* srcLine = bd.getLinePointer (y);
        auto* dstRow = rgba->data() + ((size_t) dstY * (size_t) w) * 4;
        for (int x = 0; x < w; ++x)
        {
            const auto* px = reinterpret_cast<const juce::PixelARGB*> (srcLine + x * ps);
            auto* p = dstRow + (size_t) x * 4;
            p[0] = px->getRed(); p[1] = px->getGreen(); p[2] = px->getBlue(); p[3] = 255;
        }
    }

    const juce::ScopedLock sl (lock);
    frames.clear();
    frames.push_back (std::move (rgba));
    frameW = w;
    frameH = h;
    currentPath = file.getFullPathName();
    numFrames.store (1, std::memory_order_relaxed);
    videoFps.store (30.0f, std::memory_order_relaxed);
    return true;
}

bool VideoDecoder::decodeVideoFile (const juce::String& path)
{
#if JUCE_WINDOWS
    const HRESULT hrCo = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    if (FAILED (MFStartup (MF_VERSION)))
    {
        if (SUCCEEDED (hrCo)) CoUninitialize();
        return false;
    }

    IMFAttributes* attrs = nullptr;
    MFCreateAttributes (&attrs, 2);
    if (attrs != nullptr)
    {
        attrs->SetUINT32 (MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        attrs->SetUINT32 (MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    }

    IMFSourceReader* reader = nullptr;
    const HRESULT hrOpen = MFCreateSourceReaderFromURL (path.toWideCharPointer(), attrs, &reader);
    if (attrs != nullptr) attrs->Release();

    if (FAILED (hrOpen) || reader == nullptr)
    {
        MFShutdown();
        if (SUCCEEDED (hrCo)) CoUninitialize();
        return false;
    }

    // Configure output subtype to RGB32
    IMFMediaType* type = nullptr;
    MFCreateMediaType (&type);
    type->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
    type->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    HRESULT hrType = reader->SetCurrentMediaType ((DWORD) MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type);
    type->Release();

    if (FAILED (hrType))
    {
        reader->Release();
        MFShutdown();
        if (SUCCEEDED (hrCo)) CoUninitialize();
        return false;
    }

    UINT32 srcW = 0, srcH = 0;
    UINT32 fpsNum = 30, fpsDen = 1;
    LONG stride = 0;
    IMFMediaType* curType = nullptr;
    if (SUCCEEDED (reader->GetCurrentMediaType ((DWORD) MF_SOURCE_READER_FIRST_VIDEO_STREAM, &curType)) && curType != nullptr)
    {
        MFGetAttributeSize (curType, MF_MT_FRAME_SIZE, &srcW, &srcH);
        MFGetAttributeRatio (curType, MF_MT_FRAME_RATE, &fpsNum, &fpsDen);
        stride = (LONG) MFGetAttributeUINT32 (curType, MF_MT_DEFAULT_STRIDE, srcW * 4);
        curType->Release();
    }

    if (srcW == 0 || srcH == 0)
    {
        reader->Release();
        MFShutdown();
        if (SUCCEEDED (hrCo)) CoUninitialize();
        return false;
    }

    int dstW = 0, dstH = 0;
    fitWithin ((int) srcW, (int) srcH, dstW, dstH);
    std::vector<std::shared_ptr<const std::vector<uint8_t>>> decoded;
    decoded.reserve (kMaxCachedFrames);

    while (! threadShouldExit() && (int) decoded.size() < kMaxCachedFrames)
    {
        DWORD streamIndex = 0, flags = 0;
        LONGLONG llTimeStamp = 0;
        IMFSample* sample = nullptr;
        const HRESULT hrRead = reader->ReadSample ((DWORD) MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                                                   0, &streamIndex, &flags, &llTimeStamp, &sample);
        if (FAILED (hrRead) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
        {
            if (sample != nullptr) sample->Release();
            break;
        }
        if (sample == nullptr)
            continue;

        IMFMediaBuffer* buf = nullptr;
        if (SUCCEEDED (sample->ConvertToContiguousBuffer (&buf)) && buf != nullptr)
        {
            BYTE* raw = nullptr;
            DWORD maxLen = 0, curLen = 0;
            if (SUCCEEDED (buf->Lock (&raw, &maxLen, &curLen)) && raw != nullptr)
            {
                const int absStride = std::abs (stride) > 0 ? (int) std::abs (stride) : (int) srcW * 4;
                if ((int) curLen >= absStride * (int) srcH)
                {
                    auto rgba = std::make_shared<std::vector<uint8_t>> ((size_t) dstW * (size_t) dstH * 4);
                    for (int y = 0; y < dstH; ++y)
                    {
                        const int sy = (dstH - 1 - y) * (int) srcH / dstH;
                        const BYTE* rowPtr = (stride < 0)
                                                 ? (raw + ((int) srcH - 1 - sy) * absStride)
                                                 : (raw + sy * absStride);
                        auto* dstRow = rgba->data() + ((size_t) y * (size_t) dstW) * 4;
                        for (int x = 0; x < dstW; ++x)
                        {
                            const int sx = x * (int) srcW / dstW;
                            const BYTE* px = rowPtr + sx * 4; // BGRA in memory
                            auto* dst = dstRow + (size_t) x * 4;
                            dst[0] = px[2];
                            dst[1] = px[1];
                            dst[2] = px[0];
                            dst[3] = 255;
                        }
                    }
                    decoded.push_back (std::move (rgba));
                }
                buf->Unlock();
            }
            buf->Release();
        }
        sample->Release();
    }

    reader->Release();
    MFShutdown();
    if (SUCCEEDED (hrCo)) CoUninitialize();

    if (decoded.empty())
        return false;

    {
        const juce::ScopedLock sl (lock);
        frames = std::move (decoded);
        frameW = dstW;
        frameH = dstH;
        currentPath = path;
        numFrames.store ((int) frames.size(), std::memory_order_relaxed);
        const float fps = (fpsDen > 0 && fpsNum > 0) ? ((float) fpsNum / (float) fpsDen) : 30.0f;
        videoFps.store (juce::jlimit (5.0f, 120.0f, fps), std::memory_order_relaxed);
    }
    return true;
#else
    juce::ignoreUnused (path);
    return false;
#endif
}
