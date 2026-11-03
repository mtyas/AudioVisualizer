#include "OutputWindow.h"

OutputWindow::OutputWindow (Engine& e)
    : juce::DocumentWindow ("AudioVisualizer Output", juce::Colours::black, juce::DocumentWindow::allButtons),
      engine (e),
      renderer (e, context)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setContentNonOwned (&content, false);
    centreWithSize (960, 540);
    setWantsKeyboardFocus (true);

    context.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    context.setRenderer (&renderer);
    context.setContinuousRepainting (true);
    context.setComponentPaintingEnabled (false);
    context.attachTo (content);
    content.addMouseListener (this, false); // mouse position / button become modulation sources (mouse.x/y/down)
    setAlwaysOnTop (engine.outputPinned.load());
    setVisible (true);
}

OutputWindow::~OutputWindow()
{
    if (juce::Desktop::getInstance().getKioskModeComponent() == this)
        juce::Desktop::getInstance().setKioskModeComponent (nullptr, false);
    context.detach();
}

void OutputWindow::toggleFullscreen()
{
    auto& desktop = juce::Desktop::getInstance();
    desktop.setKioskModeComponent (desktop.getKioskModeComponent() == this ? nullptr : this, false);
}

void OutputWindow::publishMouse (const juce::MouseEvent& e)
{
    const auto p = e.getEventRelativeTo (&content).position;
    const float w = (float) juce::jmax (1, content.getWidth()), h = (float) juce::jmax (1, content.getHeight());
    engine.bus.set (engine.sMouseX, juce::jlimit (0.0f, 1.0f, p.x / w));
    engine.bus.set (engine.sMouseY, juce::jlimit (0.0f, 1.0f, 1.0f - p.y / h));
}

void OutputWindow::mouseMove (const juce::MouseEvent& e) { publishMouse (e); }
void OutputWindow::mouseDrag (const juce::MouseEvent& e) { publishMouse (e); }
void OutputWindow::mouseDown (const juce::MouseEvent& e) { publishMouse (e); engine.bus.set (engine.sMouseDown, 1.0f); }
void OutputWindow::mouseUp (const juce::MouseEvent& e)   { publishMouse (e); engine.bus.set (engine.sMouseDown, 0.0f); }

void OutputWindow::setPinned (bool shouldBePinned)
{
    engine.outputPinned = shouldBePinned;
    setAlwaysOnTop (shouldBePinned);
}

bool OutputWindow::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('t', juce::ModifierKeys::noModifiers, 0)) // T = toggle "always on top"
    {
        setPinned (! engine.outputPinned.load());
        return true;
    }
    const bool fs = juce::Desktop::getInstance().getKioskModeComponent() == this;
    if (k == juce::KeyPress::F11Key
        || k == juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys::altModifier, 0)
        || (fs && k == juce::KeyPress::escapeKey))
    {
        toggleFullscreen();
        return true;
    }
    return false;
}
