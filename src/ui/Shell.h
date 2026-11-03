#pragma once
#include "Theme.h"

// Left-rail navigation item: step number, title and one-line description.
class NavButton final : public juce::Button
{
public:
    NavButton (const juce::String& number, const juce::String& title, const juce::String& subtitle, juce::Colour accentColour)
        : juce::Button (title), num (number), heading (title), sub (subtitle), accent (accentColour)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool hot, bool) override
    {
        using namespace avtheme;
        const bool on = getToggleState();
        auto r = getLocalBounds().toFloat().reduced (8.0f, 2.0f);
        if (on)
        {
            g.setColour (accent.withAlpha (0.13f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (r.getX(), r.getY() + 9.0f, 3.0f, r.getHeight() - 18.0f, 1.5f);
        }
        else if (hot)
        {
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRoundedRectangle (r, 8.0f);
        }

        auto badge = juce::Rectangle<float> (r.getX() + 12.0f, r.getCentreY() - 12.0f, 24.0f, 24.0f);
        if (on)
        {
            g.setColour (accent);
            g.fillEllipse (badge);
        }
        else
        {
            g.setColour (border.brighter (0.25f));
            g.drawEllipse (badge.reduced (0.5f), 1.2f);
        }
        g.setColour (on ? bgDark : textMuted);
        g.setFont (font (12.0f, true));
        g.drawText (num, badge.toNearestInt(), juce::Justification::centred);

        auto labels = r.withTrimmedLeft (46.0f).reduced (0.0f, 6.0f);
        g.setColour (on ? textBright : textNormal);
        g.setFont (font (14.0f, true));
        g.drawText (heading, labels.removeFromTop (labels.getHeight() * 0.5f).toNearestInt(), juce::Justification::bottomLeft);
        g.setColour (textMuted);
        g.setFont (font (11.0f));
        g.drawText (sub, labels.toNearestInt(), juce::Justification::topLeft);
    }

private:
    juce::String num, heading, sub;
    juce::Colour accent;
};

// Text field whose Up/Down keys steer the results list below it.
class SearchField final : public juce::TextEditor
{
public:
    std::function<void (int)> onArrow;

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::upKey)   { if (onArrow) onArrow (-1); return true; }
        if (k == juce::KeyPress::downKey) { if (onArrow) onArrow (1);  return true; }
        return juce::TextEditor::keyPressed (k);
    }
};

struct SearchEntry
{
    juce::String title, subtitle, haystack;
    juce::String paramId;     // set for a parameter
    int tab = -1;             // page to open (0..4), for non-parameter entries
    int subTab = -1;          // Animate page sub-tab (0..4)
};

// Drop-down list of search matches.
class SearchResultsList final : public juce::Component,
                                private juce::ListBoxModel
{
public:
    std::function<void (const SearchEntry&)> onPick;

    SearchResultsList()
    {
        list.setModel (this);
        list.setRowHeight (36);
        list.setMouseMoveSelectsRows (true);
        list.setMouseClickGrabsKeyboardFocus (false);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setOutlineThickness (0);
        addAndMakeVisible (list);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void setResults (std::vector<SearchEntry> r)
    {
        results = std::move (r);
        list.updateContent();
        if (! results.empty())
            list.selectRow (0);
        list.repaint();
    }

    int size() const noexcept { return (int) results.size(); }

    void moveSelection (int delta)
    {
        if (results.empty())
            return;
        list.selectRow (juce::jlimit (0, (int) results.size() - 1, list.getSelectedRow() + delta));
    }

    const SearchEntry* getSelected() const
    {
        const int row = list.getSelectedRow();
        return row >= 0 && row < (int) results.size() ? &results[(size_t) row] : nullptr;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 10.0f);
        g.setColour (avtheme::bgRaised);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (avtheme::border.brighter (0.2f));
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
    }

    void resized() override { list.setBounds (getLocalBounds().reduced (6)); }

private:
    int getNumRows() override { return (int) results.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) results.size())
            return;
        const auto& e = results[(size_t) row];
        if (selected)
        {
            g.setColour (avtheme::accentCyan.withAlpha (0.16f));
            g.fillRoundedRectangle (0.0f, 1.0f, (float) w, (float) h - 2.0f, 6.0f);
        }
        g.setColour (avtheme::textBright);
        g.setFont (avtheme::font (13.0f, true));
        g.drawText (e.title, 10, 0, w - 150, h, juce::Justification::centredLeft);
        g.setColour (avtheme::textMuted);
        g.setFont (avtheme::font (11.5f));
        g.drawText (e.subtitle, w - 150, 0, 140, h, juce::Justification::centredRight);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < (int) results.size() && onPick)
            onPick (results[(size_t) row]);
    }

    juce::ListBox list;
    std::vector<SearchEntry> results;
};

// "How it works" card shown from the ? button.
class HelpContent final : public juce::Component
{
public:
    HelpContent() { setSize (560, 520); }

    void paint (juce::Graphics& g) override
    {
        using namespace avtheme;
        g.fillAll (bgRaised);
        auto r = getLocalBounds().reduced (22, 18);
        g.setColour (textBright);
        g.setFont (font (18.0f, true));
        g.drawText ("How AudioVisualizer works", r.removeFromTop (28), juce::Justification::centredLeft);
        g.setColour (textMuted);
        g.setFont (font (12.5f));
        g.drawFittedText ("Sound and motion go in, get turned into control signals, and those move the visuals. "
                          "Work through the pages on the left in order.",
                          r.removeFromTop (38), juce::Justification::topLeft, 2);
        r.removeFromTop (6);

        struct Step { const char* n; const char* title; const char* body; juce::Colour c; };
        const Step steps[] = {
            { "1", "Compose", "Stack up to 13 layers of your choice: 3D scene, shader, spectrum, gradient, video, webcam, text. Reorder and blend them.", accentCyan },
            { "2", "Style", "Effects on the whole picture: feedback trails, bloom, kaleidoscope, glitch and colour grade.", accentAmber },
            { "3", "Animate", "Audio, MIDI, LFOs, envelopes, chaos and sequencers can drive any setting. Quick way: right-click a slider.", accentPurple },
            { "4", "Patch", "Advanced. Wire signals through remap, smooth, math and script nodes for custom behaviour.", accentGreen },
            { "5", "Library", "42 factory looks to start from. Save your own, import and export files.", accentRed },
        };
        for (const auto& s : steps)
        {
            auto row = r.removeFromTop (58);
            auto badge = juce::Rectangle<float> ((float) row.getX(), (float) row.getY() + 4.0f, 26.0f, 26.0f);
            g.setColour (s.c);
            g.fillEllipse (badge);
            g.setColour (bgDark);
            g.setFont (font (13.0f, true));
            g.drawText (s.n, badge.toNearestInt(), juce::Justification::centred);
            row.removeFromLeft (38);
            g.setColour (textBright);
            g.setFont (font (14.0f, true));
            g.drawText (s.title, row.removeFromTop (20), juce::Justification::centredLeft);
            g.setColour (textNormal);
            g.setFont (font (12.0f));
            g.drawFittedText (s.body, row, juce::Justification::topLeft, 2);
        }

        r.removeFromTop (6);
        g.setColour (border);
        g.drawHorizontalLine (r.getY(), (float) r.getX(), (float) r.getRight());
        r.removeFromTop (10);
        g.setColour (textBright);
        g.setFont (font (13.0f, true));
        g.drawText ("Handy to know", r.removeFromTop (20), juce::Justification::centredLeft);
        g.setColour (textNormal);
        g.setFont (font (12.0f));
        const char* tips[] = {
            "Open Output shows the visuals in their own window. \"Always on top\" keeps it above your DAW; press T in that window to toggle it, F11 for fullscreen.",
            "Double-click any slider to reset it. Ctrl+Z / Ctrl+Y undo and redo everything. Ctrl+F searches every setting.",
            "The amber bar on a slider is the live modulated value, so you can see what your sources are doing.",
        };
        for (auto* t : tips)
            g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa2  ")) + t, r.removeFromTop (38), juce::Justification::topLeft, 2);
    }
};
