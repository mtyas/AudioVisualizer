#pragma once
#include "core/Engine.h"
#include "render/Renderer.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

// Floating top-level output window (not embedded in the host). F11 / Alt+Enter toggles fullscreen.
class OutputWindow final : public juce::DocumentWindow
{
public:
    explicit OutputWindow (Engine&);
    ~OutputWindow() override;

    void closeButtonPressed() override { setVisible (false); }
    void toggleFullscreen();
    void setPinned (bool shouldBePinned);   // always on top
    bool keyPressed (const juce::KeyPress&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    Engine& engine;
    void publishMouse (const juce::MouseEvent&);
    juce::OpenGLContext context;
    Renderer renderer;
    juce::Component content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputWindow)
};
