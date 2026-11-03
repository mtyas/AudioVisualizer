#pragma once
#include "../core/Engine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>

namespace avtheme
{
// Surfaces, darkest to lightest
inline const juce::Colour bgDark       { 0xff0c0d12 };   // window / canvas
inline const juce::Colour bgHeader     { 0xff10121a };   // top bar, status bar
inline const juce::Colour bgPanel      { 0xff13151e };   // navigation rail
inline const juce::Colour bgCard       { 0xff181b26 };   // cards / groups
inline const juce::Colour bgRaised     { 0xff202433 };   // inputs, buttons
inline const juce::Colour border       { 0xff2a2f42 };
inline const juce::Colour borderSoft   { 0xff1f2332 };

// One primary accent (cyan) + semantic colours used to colour-code sections
inline const juce::Colour accentCyan   { 0xff40d9ee };
inline const juce::Colour accentPurple { 0xffa98bfb };
inline const juce::Colour accentAmber  { 0xfff7b93e };
inline const juce::Colour accentGreen  { 0xff36d39b };
inline const juce::Colour accentRed    { 0xfff4617b };

inline const juce::Colour textBright   { 0xfff0f3fa };
inline const juce::Colour textNormal   { 0xffc3c9dc };
inline const juce::Colour textMuted    { 0xff8a92ad };

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

// A rounded card with an optional header (coloured dot + title + optional subtitle line).
inline void drawCard (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title = {},
                      juce::Colour accent = accentCyan, const juce::String& subtitle = {})
{
    auto r = bounds.toFloat().reduced (2.0f);
    g.setColour (bgCard);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (borderSoft);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    if (title.isNotEmpty())
    {
        auto top = r.reduced (14.0f, 0.0f).removeFromTop (subtitle.isNotEmpty() ? 58.0f : 32.0f);
        g.setColour (accent);
        g.fillEllipse (top.getX(), top.getY() + 13.0f, 7.0f, 7.0f);
        g.setColour (textBright);
        g.setFont (font (13.5f, true));
        g.drawText (title, top.withTrimmedLeft (16.0f).withHeight (32.0f).toNearestInt(), juce::Justification::centredLeft);
        if (subtitle.isNotEmpty())
        {
            g.setColour (textMuted);
            g.setFont (font (11.5f));
            g.drawFittedText (subtitle, top.withTrimmedLeft (16.0f).withTrimmedTop (28.0f).withHeight (30.0f).toNearestInt(),
                              juce::Justification::topLeft, 2);
        }
    }
}
} // namespace avtheme

class AVLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    AVLookAndFeel()
    {
        using namespace avtheme;
        setColour (juce::ResizableWindow::backgroundColourId, bgDark);
        setColour (juce::DocumentWindow::textColourId, textBright);

        setColour (juce::TextButton::buttonColourId, bgRaised);
        setColour (juce::TextButton::buttonOnColourId, accentCyan.withAlpha (0.22f));
        setColour (juce::TextButton::textColourOffId, textBright);
        setColour (juce::TextButton::textColourOnId, accentCyan);
        setColour (juce::ToggleButton::textColourId, textNormal);
        setColour (juce::ToggleButton::tickColourId, accentCyan);

        setColour (juce::ComboBox::backgroundColourId, bgRaised);
        setColour (juce::ComboBox::outlineColourId, border);
        setColour (juce::ComboBox::textColourId, textBright);
        setColour (juce::ComboBox::arrowColourId, textMuted);
        setColour (juce::ComboBox::focusedOutlineColourId, accentCyan);
        setColour (juce::PopupMenu::backgroundColourId, bgRaised);
        setColour (juce::PopupMenu::textColourId, textBright);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentCyan.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, textBright);
        setColour (juce::PopupMenu::headerTextColourId, textMuted);

        setColour (juce::Slider::thumbColourId, textBright);
        setColour (juce::Slider::trackColourId, accentCyan);
        setColour (juce::Slider::backgroundColourId, border);
        setColour (juce::Slider::rotarySliderFillColourId, accentCyan);
        setColour (juce::Slider::rotarySliderOutlineColourId, border);
        setColour (juce::Slider::textBoxTextColourId, textNormal);
        setColour (juce::Slider::textBoxBackgroundColourId, bgDark);
        setColour (juce::Slider::textBoxOutlineColourId, borderSoft);
        setColour (juce::Slider::textBoxHighlightColourId, accentCyan.withAlpha (0.35f));

        setColour (juce::Label::textColourId, textNormal);
        setColour (juce::TextEditor::backgroundColourId, bgDark);
        setColour (juce::TextEditor::textColourId, textBright);
        setColour (juce::TextEditor::outlineColourId, border);
        setColour (juce::TextEditor::focusedOutlineColourId, accentCyan);
        setColour (juce::TextEditor::highlightColourId, accentCyan.withAlpha (0.3f));
        setColour (juce::CaretComponent::caretColourId, accentCyan);

        setColour (juce::ScrollBar::thumbColourId, border.brighter (0.4f));
        setColour (juce::ListBox::backgroundColourId, bgDark);
        setColour (juce::TooltipWindow::backgroundColourId, bgRaised.brighter (0.1f));
        setColour (juce::TooltipWindow::textColourId, textBright);
        setColour (juce::TooltipWindow::outlineColourId, border.brighter (0.3f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        using namespace avtheme;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto fill = on ? accentCyan.withAlpha (0.18f) : bgRaised;
        if (down)             fill = fill.brighter (0.18f);
        else if (highlighted) fill = fill.brighter (0.09f);
        if (! b.isEnabled())  fill = fill.withMultipliedAlpha (0.5f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? accentCyan.withAlpha (0.75f) : (highlighted ? border.brighter (0.3f) : border));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return avtheme::font (juce::jlimit (11.0f, 13.5f, (float) buttonHeight * 0.46f));
    }

    // Toggle buttons are drawn as a switch (with optional label).
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        using namespace avtheme;
        auto r = b.getLocalBounds().toFloat();
        const bool hasText = b.getButtonText().isNotEmpty();
        float h = juce::jmin (16.0f, r.getHeight() - 4.0f);
        float w = h * 1.8f;
        if (! hasText && w > r.getWidth() - 2.0f)
        {
            w = juce::jmax (8.0f, r.getWidth() - 2.0f);
            h = w / 1.8f;
        }

        juce::Rectangle<float> sw (hasText ? r.getX() + 1.0f : r.getCentreX() - w * 0.5f, r.getCentreY() - h * 0.5f, w, h);
        const bool on = b.getToggleState();
        auto track = on ? accentCyan : bgRaised.brighter (0.12f);
        if (highlighted) track = track.brighter (0.12f);
        if (! b.isEnabled()) track = track.withMultipliedAlpha (0.5f);
        g.setColour (track);
        g.fillRoundedRectangle (sw, h * 0.5f);

        const float knob = h - 4.0f;
        g.setColour (on ? bgDark : textMuted);
        g.fillEllipse (on ? sw.getRight() - knob - 2.0f : sw.getX() + 2.0f, sw.getCentreY() - knob * 0.5f, knob, knob);

        if (hasText)
        {
            g.setColour (b.findColour (juce::ToggleButton::textColourId).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
            g.setFont (font (13.0f));
            g.drawText (b.getButtonText(), r.withTrimmedLeft (w + 8.0f).toNearestInt(), juce::Justification::centredLeft);
        }
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace avtheme;
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (bgRaised);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.hasKeyboardFocus (true) ? accentCyan : border);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        const float cx = (float) width - 14.0f, cy = (float) height * 0.5f;
        juce::Path p;
        p.startNewSubPath (cx - 3.5f, cy - 1.5f);
        p.lineTo (cx, cy + 2.0f);
        p.lineTo (cx + 3.5f, cy - 1.5f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return avtheme::font (12.5f); }

    // Slider value boxes: dark rounded field instead of the default square grey outline.
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        using namespace avtheme;
        if (dynamic_cast<juce::ComboBox*> (label.getParentComponent()) != nullptr)
        {
            // Dropdown text: one line, ellipsis when too long (the default wraps onto two lines).
            g.setColour (label.findColour (juce::Label::textColourId));
            g.setFont (label.getFont());
            g.drawText (label.getText(), label.getLocalBounds().reduced (2, 0), juce::Justification::centredLeft, true);
            return;
        }
        if (dynamic_cast<juce::Slider*> (label.getParentComponent()) == nullptr)
        {
            LookAndFeel_V4::drawLabel (g, label);
            return;
        }
        auto r = label.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (bgDark);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (label.isBeingEdited() ? accentCyan : borderSoft);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        if (! label.isBeingEdited())
        {
            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f));
            g.setFont (font (12.0f));
            g.drawText (label.getText(), label.getLocalBounds().reduced (2, 0), juce::Justification::centred);
        }
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    // Thin-track horizontal slider. A slider carrying a "mod" property (the live modulated value,
    // set by ParamControl) additionally shows an amber span from the base value to the modulated one.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                           juce::Slider& s) override
    {
        using namespace avtheme;
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, s);
            return;
        }

        const float cy = (float) y + (float) height * 0.5f;
        const float th = 4.0f;
        const bool active = s.isEnabled();
        g.setColour (border);
        g.fillRoundedRectangle ((float) x, cy - th * 0.5f, (float) width, th, th * 0.5f);

        // Fill from the minimum, or from zero for bipolar ranges
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bipolar ? s.getPositionOfValue (0.0) : minSliderPos;
        const float a = juce::jmin (from, sliderPos), bPos = juce::jmax (from, sliderPos);
        g.setColour (active ? accentCyan : textMuted.withAlpha (0.5f));
        g.fillRoundedRectangle (a, cy - th * 0.5f, juce::jmax (0.0f, bPos - a), th, th * 0.5f);

        if (s.getProperties().contains ("mod"))
        {
            const float mp = s.getPositionOfValue ((double) s.getProperties()["mod"]);
            const float ma = juce::jmin (sliderPos, mp), mb = juce::jmax (sliderPos, mp);
            g.setColour (accentAmber.withAlpha (0.9f));
            g.fillRoundedRectangle (ma, cy - th * 0.5f - 1.0f, juce::jmax (2.0f, mb - ma), th + 2.0f, 2.0f);
            g.fillEllipse (mp - 3.0f, cy - 3.0f, 6.0f, 6.0f);
        }

        const bool hot = s.isMouseOverOrDragging();
        const float tr = hot ? 7.0f : 6.0f;
        if (hot)
        {
            g.setColour (accentCyan.withAlpha (0.22f));
            g.fillEllipse (sliderPos - tr - 3.0f, cy - tr - 3.0f, (tr + 3.0f) * 2.0f, (tr + 3.0f) * 2.0f);
        }
        g.setColour (active ? textBright : textMuted);
        g.fillEllipse (sliderPos - tr, cy - tr, tr * 2.0f, tr * 2.0f);
        g.setColour (bgDark.withAlpha (0.5f));
        g.drawEllipse (sliderPos - tr, cy - tr, tr * 2.0f, tr * 2.0f, 1.0f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace avtheme;
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre();
        const float arcR = radius - 2.0f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path bg;
        bg.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (border);
        g.strokePath (bg, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float inner = arcR - 6.0f;
        g.setColour (bgRaised);
        g.fillEllipse (c.x - inner, c.y - inner, inner * 2.0f, inner * 2.0f);
        juce::Path tick;
        tick.startNewSubPath (c.x + std::sin (angle) * (inner * 0.35f), c.y - std::cos (angle) * (inner * 0.35f));
        tick.lineTo (c.x + std::sin (angle) * inner * 0.92f, c.y - std::cos (angle) * inner * 0.92f);
        g.setColour (s.isMouseOverOrDragging() ? accentCyan : textBright);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

// Slider that reports right-clicks (used for "assign modulation") instead of treating them as a drag.
class ParamSlider final : public juce::Slider
{
public:
    std::function<void()> onRightClick;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu()) juce::Slider::mouseUp (e); }
};

// A smart parameter row linked to a VisualState parameter ID:
//   [label] [slider | dropdown | switch]
// - shows the live modulated value as an amber span on the slider
// - every edit is an undoable transaction (drag, double-click reset, typing a value)
// - right-click to assign a modulation source; hover for a tooltip
class ParamControl final : public juce::Component,
                           private juce::Timer
{
public:
    ParamControl (Engine& e, const juce::String& paramId, const juce::String& customLabel = {},
                  juce::StringArray enumChoices = {})
        : engine (e), id (paramId), choices (std::move (enumChoices))
    {
        paramIdx = engine.visual.findParam (id);
        const auto info = paramIdx >= 0 ? engine.visual.getParamInfo (paramIdx)
                                        : ParamInfo { id, id, {}, 0.0f, 1.0f, 0.0f, 0.0f };
        displayName = customLabel.isNotEmpty() ? customLabel : info.name;

        tip = displayName + "\n" + (choices.isEmpty()
            ? "Range " + juce::String (info.minVal, 2) + " to " + juce::String (info.maxVal, 2)
                  + "  |  default " + juce::String (info.defaultVal, 2)
                  + "\nDouble-click: reset  |  Right-click: modulate this from audio, LFOs, MIDI..."
            : "Choose a value from the list.");

        label.setText (displayName, juce::dontSendNotification);
        label.setFont (avtheme::font (12.5f));
        label.setColour (juce::Label::textColourId, avtheme::textNormal);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
        setTooltip (tip);

        if (! choices.isEmpty())
        {
            isCombo = true;
            for (int i = 0; i < choices.size(); ++i)
                combo.addItem (choices[i], i + 1);
            combo.setSelectedId ((int) std::round (engine.visual.getBase (paramIdx)) + 1, juce::dontSendNotification);
            combo.setTooltip (tip);
            combo.onChange = [this, labelStr = info.name]
            {
                const float newVal = (float) (combo.getSelectedId() - 1);
                if (std::abs (newVal - engine.visual.getBase (paramIdx)) > 0.001f)
                    engine.performUndoable ("Set " + labelStr, [this, newVal] { engine.visual.setBase (paramIdx, newVal); });
            };
            addAndMakeVisible (combo);
        }
        else if (info.step == 1.0f && info.minVal == 0.0f && info.maxVal == 1.0f)
        {
            isToggle = true;
            toggle.setButtonText (displayName);
            toggle.setTooltip (tip);
            label.setVisible (false);
            toggle.setToggleState (engine.visual.getBase (paramIdx) >= 0.5f, juce::dontSendNotification);
            toggle.onClick = [this, labelStr = info.name]
            {
                const float newVal = toggle.getToggleState() ? 1.0f : 0.0f;
                engine.performUndoable ("Toggle " + labelStr, [this, newVal] { engine.visual.setBase (paramIdx, newVal); });
            };
            addAndMakeVisible (toggle);
        }
        else
        {
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
            slider.setRange (info.minVal, info.maxVal, info.step > 0.0f ? info.step : 0.001f);
            slider.setValue (engine.visual.getBase (paramIdx), juce::dontSendNotification);
            slider.setDoubleClickReturnValue (true, info.defaultVal);
            slider.setScrollWheelEnabled (false); // these live in scrollable pages
            slider.setTooltip (tip);
            slider.onRightClick = [this] { showQuickModMenu(); };
            slider.onDragStart = [this] { dragging = true; dragStartVal = engine.visual.getBase (paramIdx); };
            slider.onValueChange = [this, labelStr = info.name]
            {
                const float v = (float) slider.getValue();
                if (dragging)
                {
                    engine.visual.setBase (paramIdx, v); // live while dragging; one undo step on release
                    return;
                }
                // double-click reset / typed value
                if (std::abs (v - engine.visual.getBase (paramIdx)) > 1.0e-5f)
                    engine.performUndoable ("Adjust " + labelStr, [this, v] { engine.visual.setBase (paramIdx, v); });
            };
            slider.onDragEnd = [this, labelStr = info.name]
            {
                dragging = false;
                const float endVal = (float) slider.getValue();
                if (std::abs (endVal - dragStartVal) > 1.0e-4f)
                {
                    engine.visual.setBase (paramIdx, dragStartVal);
                    engine.performUndoable ("Adjust " + labelStr, [this, endVal] { engine.visual.setBase (paramIdx, endVal); });
                }
            };
            addAndMakeVisible (slider);
        }
    }

    const juce::String& getParamId() const noexcept { return id; }
    const juce::String& getDisplayName() const noexcept { return displayName; }

    void setTooltip (const juce::String& t) { tip = t; label.setTooltip (t); }

    // Briefly highlight this row (used when jumping here from search).
    void flash()
    {
        flashAlpha = 1.0f;
        startTimerHz (30);
        repaint();
    }

    void syncFromEngine()
    {
        if (paramIdx < 0)
            return;
        const float b = engine.visual.getBase (paramIdx);
        if (isCombo)
            combo.setSelectedId ((int) std::round (b) + 1, juce::dontSendNotification);
        else if (isToggle)
            toggle.setToggleState (b >= 0.5f, juce::dontSendNotification);
        else
        {
            if (! slider.isMouseButtonDown())
                slider.setValue (b, juce::dontSendNotification);

            // Live modulation overlay: only touch the slider when the modulated value actually moved.
            const float m = engine.visual.getModulated (paramIdx);
            const bool modulated = std::abs (m - b) > 1.0e-4f * juce::jmax (1.0f, std::abs (b));
            if (modulated != lastModulated || (modulated && m != lastPaintedMod))
            {
                lastModulated = modulated;
                lastPaintedMod = m;
                if (modulated) slider.getProperties().set ("mod", (double) m);
                else           slider.getProperties().remove ("mod");
                slider.repaint();
            }
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (flashAlpha > 0.0f)
        {
            g.setColour (avtheme::accentCyan.withAlpha (0.28f * flashAlpha));
            g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.0f, 1.0f), 6.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && ! isToggle)
            showQuickModMenu();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4, 1);
        if (isToggle)
        {
            toggle.setBounds (r);
            return;
        }
        label.setBounds (r.removeFromLeft (juce::jlimit (92, 150, (int) (getWidth() * 0.4f))));
        if (isCombo) combo.setBounds (r.reduced (0, 2));
        else         slider.setBounds (r);
    }

private:
    void timerCallback() override
    {
        flashAlpha -= 0.035f;
        if (flashAlpha <= 0.0f)
        {
            flashAlpha = 0.0f;
            stopTimer();
        }
        repaint();
    }

    static juce::String prettyGroup (const juce::String& prefix)
    {
        static const std::map<juce::String, juce::String> names {
            { "audio", "Audio analysis" }, { "band", "Spectrum bands" }, { "env", "Envelopes" },
            { "lfo", "LFOs" }, { "chaos", "Chaos" }, { "seq", "Step sequencers" }, { "macro", "Macros" },
            { "midi", "MIDI" }, { "cc", "MIDI controllers" }, { "pose", "Body pose (webcam)" },
            { "cam", "Camera" }, { "host", "Host transport" }, { "loop", "Live loop" }, { "video", "Video" },
            { "clock", "Tempo clocks" }, { "noise", "Noise & random" }, { "time", "Time" }, { "mouse", "Mouse (output window)" } };
        const auto it = names.find (prefix);
        return it != names.end() ? it->second : prefix;
    }

    void showQuickModMenu()
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Modulate \"" + displayName + "\" with...");

        const auto sources = engine.modulation.getAllSourceNames();
        std::vector<std::pair<juce::String, juce::PopupMenu>> groups;
        for (int i = 0; i < sources.size(); ++i)
        {
            const auto prefix = sources[i].upToFirstOccurrenceOf (".", false, false);
            auto it = std::find_if (groups.begin(), groups.end(), [&] (const auto& gp) { return gp.first == prefix; });
            if (it == groups.end())
            {
                groups.emplace_back (prefix, juce::PopupMenu());
                it = groups.end() - 1;
            }
            it->second.addItem (i + 1, sources[i]);
        }
        for (auto& gp : groups)
            m.addSubMenu (prettyGroup (gp.first), gp.second);

        m.addSeparator();
        m.addItem (999, "Reset to default");

        m.showMenuAsync (juce::PopupMenu::Options(), [this, sources] (int res)
        {
            if (res == 999 && paramIdx >= 0)
            {
                const float def = engine.visual.getParamInfo (paramIdx).defaultVal;
                engine.performUndoable ("Reset " + displayName, [this, def] { engine.visual.setBase (paramIdx, def); });
                syncFromEngine();
            }
            else if (res >= 1 && res <= sources.size())
            {
                ModRoute rt;
                rt.enabled     = true;
                rt.source      = sources[res - 1];
                rt.destination = id;
                rt.amount      = 0.5f;
                engine.performUndoable ("Add Mod Route " + rt.source + " -> " + id, [this, rt]
                {
                    engine.modulation.addRoute (rt);
                });
                engine.setStatus ("Now modulating " + displayName + " from " + rt.source + " - fine-tune it on the Animate page");
            }
        });
    }

    Engine& engine;
    juce::String id, displayName, tip;
    int paramIdx = -1;
    juce::StringArray choices;
    bool isCombo = false, isToggle = false, dragging = false, lastModulated = false;
    float dragStartVal = 0.0f, flashAlpha = 0.0f;
    float lastPaintedMod = std::numeric_limits<float>::quiet_NaN();

    juce::Label label;
    ParamSlider slider;
    juce::ComboBox combo;
    juce::ToggleButton toggle;
};
