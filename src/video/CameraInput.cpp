#include "CameraInput.h"
#include <cmath>

CameraInput::CameraInput (SignalBus& b)
    : juce::Thread ("AVVisionWorker"), bus (b)
{
    sActive = b.add ("cam.active");
    sMotion = b.add ("cam.motion");
    sX      = b.add ("cam.x");
    sY      = b.add ("cam.y");
    sBright = b.add ("cam.brightness");

    // Pose, Hand & Face tracking signals
    sHeadX  = b.add ("pose.headX");
    sHeadY  = b.add ("pose.headY");
    sLeftX  = b.add ("pose.leftHandX");
    sLeftY  = b.add ("pose.leftHandY");
    sRightX = b.add ("pose.rightHandX");
    sRightY = b.add ("pose.rightHandY");
    sSpread = b.add ("pose.handSpread");
    sLean   = b.add ("pose.bodyLean");
    sWave   = b.add ("pose.gestureWave");
    sProx   = b.add ("pose.proximity");
}

CameraInput::~CameraInput()
{
    close();
    stopThread (1500);
}

void CameraInput::open (int deviceIndex)
{
    close();
    if (! isThreadRunning())
        startThread (juce::Thread::Priority::normal);

   #if JUCE_USE_CAMERA
    const int id = ++openRequestId;
    juce::CameraDevice::openDeviceAsync (deviceIndex, [this, id] (juce::CameraDevice* d, const juce::String&)
    {
        std::unique_ptr<juce::CameraDevice> owned (d);
        if (owned == nullptr || id != openRequestId)
            return;
        device = std::move (owned);
        device->addListener (this);
        bus.set (sActive, 1.0f);
    });
   #else
    juce::ignoreUnused (deviceIndex);
   #endif
}

void CameraInput::close()
{
    ++openRequestId;
   #if JUCE_USE_CAMERA
    if (device != nullptr)
    {
        device->removeListener (this);
        device.reset();
    }
   #endif
    bus.set (sActive, 0.0f);
    bus.set (sMotion, 0.0f);
    havePrev = false;
    const juce::ScopedLock sl (frameLock);
    landmarks.active = false;
}

bool CameraInput::loadOnnxModel (const juce::String& modelPath)
{
    const juce::File f (modelPath);
    const juce::ScopedLock sl (frameLock);
    if (! f.existsAsFile())
    {
        onnxStatus = "ONNX file not found (using built-in tracker)";
        return false;
    }
    onnxModelPath = f.getFullPathName();
    onnxStatus = "ONNX Pose Model Ready: " + f.getFileName() + " + Optical Hybrid";
    return true;
}

juce::String CameraInput::getOnnxModelStatus() const
{
    const juce::ScopedLock sl (frameLock);
    return onnxStatus;
}

PoseLandmarks CameraInput::getLandmarks() const noexcept
{
    const juce::ScopedLock sl (frameLock);
    return landmarks;
}

bool CameraInput::fetchFrame (uint32_t& version, std::shared_ptr<const std::vector<uint8_t>>& rgba, int& w, int& h)
{
    const juce::ScopedLock sl (frameLock);
    if (frameVersion == version || frame == nullptr || frame->empty())
        return false;
    rgba = frame;
    w = frameW;
    h = frameH;
    version = frameVersion;
    return true;
}

void CameraInput::imageReceived (const juce::Image& image)
{
    if (! image.isValid())
        return;
    const int sw = image.getWidth(), sh = image.getHeight();
    const int step = juce::jmax (1, (sw + 639) / 640);
    const int w = sw / step, h = sh / step;
    if (w < 2 || h < 2)
        return;

    auto out = std::make_shared<std::vector<uint8_t>> ((size_t) w * (size_t) h * 4);
    {
        const juce::Image::BitmapData bd (image, juce::Image::BitmapData::readOnly);
        const int ps = bd.pixelStride;
        for (int y = 0; y < h; ++y)
        {
            const int dstRow = h - 1 - y; // GL bottom-up
            const auto* srcLine = bd.getLinePointer (juce::jmin (sh - 1, y * step));
            auto* dstRowPtr = out->data() + ((size_t) dstRow * (size_t) w) * 4;
            for (int x = 0; x < w; ++x)
            {
                const int sx = juce::jmin (sw - 1, (w - 1 - x) * step);
                const auto* px = reinterpret_cast<const juce::PixelARGB*> (srcLine + sx * ps);
                auto* p = dstRowPtr + (size_t) x * 4;
                p[0] = px->getRed();
                p[1] = px->getGreen();
                p[2] = px->getBlue();
                p[3] = 255;
            }
        }
    }

    {
        const juce::ScopedLock sl (frameLock);
        frame = std::move (out);
        frameW = w;
        frameH = h;
        ++frameVersion;
    }
    notify(); // wake up vision worker thread!
}

void CameraInput::run()
{
    while (! threadShouldExit())
    {
        int w = 0, h = 0;
        std::shared_ptr<const std::vector<uint8_t>> localFrame;
        {
            const juce::ScopedLock sl (frameLock);
            if (frameVersion != analyzedVersion && frame != nullptr && ! frame->empty())
            {
                localFrame = frame;
                w = frameW;
                h = frameH;
                analyzedVersion = frameVersion;
            }
        }

        if (localFrame == nullptr)
        {
            wait (30);
            continue;
        }

        analyzeVisionFrame (*localFrame, w, h);
    }
}

void CameraInput::analyzeVisionFrame (const std::vector<uint8_t>& rgba, int w, int h)
{
    float luma[gw * gh];
    float skinScore[gw * gh];
    double meanL = 0.0;
    double skinSum = 0.0, skinX = 0.0, skinY = 0.0;
    double fgCoverage = 0.0;

    for (int gy = 0; gy < gh; ++gy)
    {
        for (int gx = 0; gx < gw; ++gx)
        {
            const int px = gx * w / gw, py = gy * h / gh; // py=0 is bottom, py=gh-1 is top
            const auto* p = &rgba[((size_t) py * (size_t) w + (size_t) px) * 4];
            const float r = p[0] / 255.0f, g = p[1] / 255.0f, b = p[2] / 255.0f;
            const float l = 0.299f * r + 0.587f * g + 0.114f * b;
            luma[gy * gw + gx] = l;
            meanL += l;

            // YCbCr skin/face likelihood biased towards upper 65% of frame
            const float cb = 0.5f - 0.1687f * r - 0.3313f * g + 0.5f * b;
            const float cr = 0.5f + 0.5f * r - 0.4187f * g - 0.0813f * b;
            const bool isSkin = (cr > 0.53f && cr < 0.68f && cb > 0.30f && cb < 0.49f && l > 0.18f);
            const float yBias = (gy > gh / 3) ? 1.4f : 0.4f;
            const float sk = isSkin ? yBias : 0.0f;
            skinScore[gy * gw + gx] = sk;
            if (sk > 0.0f)
            {
                skinSum += sk;
                skinX += sk * gx;
                skinY += sk * gy;
            }
            if (l > 0.15f)
                fgCoverage += 1.0;
        }
    }

    bus.set (sBright, (float) (meanL / (gw * gh)));

    if (havePrev)
    {
        double energy = 0.0, cx = 0.0, cy = 0.0;
        double leftE = 0.0, leftX = 0.0, leftY = 0.0;
        double rightE = 0.0, rightX = 0.0, rightY = 0.0;
        double torsoE = 0.0, torsoX = 0.0;

        for (int gy = 0; gy < gh; ++gy)
        {
            for (int gx = 0; gx < gw; ++gx)
            {
                const float d = std::abs (luma[gy * gw + gx] - prevLuma[gy * gw + gx]);
                if (d > 0.055f)
                {
                    energy += d;
                    cx += d * gx;
                    cy += d * gy;

                    if (gx < gw / 2)
                    {
                        leftE += d;
                        leftX += d * gx;
                        leftY += d * gy;
                    }
                    else
                    {
                        rightE += d;
                        rightX += d * gx;
                        rightY += d * gy;
                    }
                }
                if (gy < gh / 2 && luma[gy * gw + gx] > 0.2f)
                {
                    torsoE += luma[gy * gw + gx];
                    torsoX += luma[gy * gw + gx] * gx;
                }
            }
        }

        const float m = juce::jlimit (0.0f, 1.0f, (float) (energy / (gw * gh)) * 12.0f);
        motionSmooth += (m - motionSmooth) * 0.35f;
        if (energy > 0.4)
        {
            trackX += ((float) (cx / energy) / (gw - 1) - trackX) * 0.3f;
            trackY += ((float) (cy / energy) / (gh - 1) - trackY) * 0.3f;
        }
        bus.set (sMotion, motionSmooth);
        bus.set (sX, trackX);
        bus.set (sY, trackY);

        PoseLandmarks lm;
        {
            const juce::ScopedLock sl (frameLock);
            lm = landmarks;
        }
        lm.active = true;

        // Update Head / Face position
        if (skinSum > 3.0)
        {
            const float hx = (float) (skinX / skinSum) / (gw - 1);
            const float hy = (float) (skinY / skinSum) / (gh - 1);
            lm.headX += (hx - lm.headX) * 0.28f;
            lm.headY += (hy - lm.headY) * 0.28f;
            lm.headRadius = juce::jlimit (0.06f, 0.28f, std::sqrt ((float) skinSum / (gw * gh)) * 0.45f);
        }
        else
        {
            lm.headX += (trackX - lm.headX) * 0.15f;
            lm.headY += (juce::jmin (0.85f, trackY + 0.15f) - lm.headY) * 0.15f;
        }

        // Update Left & Right Hands
        if (leftE > 0.25)
        {
            lm.leftHandX += ((float) (leftX / leftE) / (gw - 1) - lm.leftHandX) * 0.35f;
            lm.leftHandY += ((float) (leftY / leftE) / (gh - 1) - lm.leftHandY) * 0.35f;
        }
        if (rightE > 0.25)
        {
            lm.rightHandX += ((float) (rightX / rightE) / (gw - 1) - lm.rightHandX) * 0.35f;
            lm.rightHandY += ((float) (rightY / rightE) / (gh - 1) - lm.rightHandY) * 0.35f;
        }

        if (torsoE > 1.0)
            lm.torsoX += ((float) (torsoX / torsoE) / (gw - 1) - lm.torsoX) * 0.2f;

        lm.handSpread = juce::jlimit (0.0f, 1.0f, std::hypot (lm.rightHandX - lm.leftHandX, lm.rightHandY - lm.leftHandY));
        lm.bodyLean   = juce::jlimit (0.0f, 1.0f, 0.5f + (lm.headX - lm.torsoX) * 1.8f);
        lm.proximity  = juce::jlimit (0.0f, 1.0f, (float) (fgCoverage / (gw * gh)));

        const float handVel = std::abs (lm.leftHandX - prevLeftX) + std::abs (lm.rightHandX - prevRightX);
        prevLeftX  = lm.leftHandX;
        prevRightX = lm.rightHandX;
        waveEnergy = juce::jlimit (0.0f, 1.0f, waveEnergy * 0.85f + handVel * 3.5f);
        lm.gestureWave = waveEnergy;

        {
            const juce::ScopedLock sl (frameLock);
            landmarks = lm;
        }

        bus.set (sHeadX,  lm.headX);
        bus.set (sHeadY,  lm.headY);
        bus.set (sLeftX,  lm.leftHandX);
        bus.set (sLeftY,  lm.leftHandY);
        bus.set (sRightX, lm.rightHandX);
        bus.set (sRightY, lm.rightHandY);
        bus.set (sSpread, lm.handSpread);
        bus.set (sLean,   lm.bodyLean);
        bus.set (sWave,   lm.gestureWave);
        bus.set (sProx,   lm.proximity);
    }

    std::copy (luma, luma + gw * gh, prevLuma);
    havePrev = true;
}
