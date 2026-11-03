#pragma once
#include "Theme.h"
#include <functional>

// A card that groups related parameter rows (and any extra full-width components such as
// buttons) under a titled header with a one-line explanation.
class ParamGroup final : public juce::Component
{
public:
    ParamGroup (Engine& e, juce::String groupTitle, juce::String groupHint = {},
                juce::Colour accentColour = avtheme::accentCyan)
        : engine (e), title (std::move (groupTitle)), hint (std::move (groupHint)), accent (accentColour) {}

    ParamControl* addParam (const juce::String& id, const juce::String& label = {}, juce::StringArray choices = {})
    {
        auto c = std::make_unique<ParamControl> (engine, id, label, std::move (choices));
        addAndMakeVisible (*c);
        auto* raw = c.get();
        owned.push_back (std::move (c));
        rows.push_back ({ { raw }, {}, rowHeight });
        return raw;
    }

    // Full-width row of existing components (not owned). Weights default to equal widths.
    void addRow (std::vector<juce::Component*> comps, int height = 30, std::vector<float> weights = {})
    {
        for (auto* c : comps)
            addAndMakeVisible (c);
        rows.push_back ({ std::move (comps), std::move (weights), height });
    }

    ParamControl* find (const juce::String& id) const
    {
        for (auto& c : owned)
            if (c->getParamId() == id)
                return c.get();
        return nullptr;
    }

    int getPreferredHeight() const
    {
        int h = headerHeight() + 8;
        for (auto& r : rows)
            h += r.height + rowGap;
        return h + 6;
    }

    void syncFromEngine()
    {
        for (auto& c : owned)
            c->syncFromEngine();
    }

    void paint (juce::Graphics& g) override
    {
        avtheme::drawCard (g, getLocalBounds(), title, accent, hint);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 0);
        r.removeFromTop (headerHeight() + 8);
        for (auto& row : rows)
        {
            auto line = r.removeFromTop (row.height);
            r.removeFromTop (rowGap);
            if (row.comps.size() == 1)
            {
                row.comps[0]->setBounds (line);
                continue;
            }
            float total = 0.0f;
            for (size_t i = 0; i < row.comps.size(); ++i)
                total += i < row.weights.size() ? row.weights[i] : 1.0f;
            const int gap = 8;
            const float avail = (float) (line.getWidth() - gap * ((int) row.comps.size() - 1));
            for (size_t i = 0; i < row.comps.size(); ++i)
            {
                const float wgt = i < row.weights.size() ? row.weights[i] : 1.0f;
                const int w = (i + 1 == row.comps.size()) ? line.getWidth() : (int) std::round (avail * wgt / total);
                row.comps[i]->setBounds (line.removeFromLeft (w));
                line.removeFromLeft (gap);
            }
        }
    }

private:
    struct Row { std::vector<juce::Component*> comps; std::vector<float> weights; int height; };

    int headerHeight() const { return hint.isNotEmpty() ? 58 : 32; }

    static constexpr int rowHeight = 30, rowGap = 2;
    Engine& engine;
    juce::String title, hint;
    juce::Colour accent;
    std::vector<std::unique_ptr<ParamControl>> owned;
    std::vector<Row> rows;
};

// Scrollable page that flows ParamGroups into as many columns as fit (shortest column first).
class FlowPanel final : public juce::Component
{
public:
    FlowPanel()
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (9);
    }

    void addGroup (ParamGroup& g)
    {
        groups.push_back (&g);
        content.addAndMakeVisible (g);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds());
        relayout();
    }

    void relayout()
    {
        const int pad = 6, gap = 8;
        const int w = juce::jmax (200, getWidth() - viewport.getScrollBarThickness() - 2);
        const int cols = juce::jmax (1, (w - pad) / (minColumnWidth + gap));
        const int colW = (w - pad * 2 - gap * (cols - 1)) / cols;

        std::vector<int> heights ((size_t) cols, pad);
        for (auto* g : groups)
        {
            size_t best = 0;
            for (size_t c = 1; c < heights.size(); ++c)
                if (heights[c] < heights[best])
                    best = c;
            const int ph = g->getPreferredHeight();
            g->setBounds (pad + (int) best * (colW + gap), heights[best], colW, ph);
            heights[best] += ph + gap;
        }
        content.setSize (w, *std::max_element (heights.begin(), heights.end()) + pad);
    }

    void syncFromEngine()
    {
        for (auto* g : groups)
            g->syncFromEngine();
    }

    // Scroll so that the given component (a descendant of a group) is comfortably in view.
    void reveal (juce::Component& c)
    {
        relayout();
        const auto r = content.getLocalArea (&c, c.getLocalBounds());
        viewport.setViewPosition (0, juce::jmax (0, r.getY() - 80));
    }

private:
    struct Content final : public juce::Component {};

    static constexpr int minColumnWidth = 340;
    juce::Viewport viewport;
    Content content;
    std::vector<ParamGroup*> groups;
};

// Small custom-painted strip for live read-outs (loop playhead, pose scope, ...).
class LiveStrip final : public juce::Component
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> painter;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (avtheme::bgDark);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (avtheme::borderSoft);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        if (painter)
            painter (g, r);
    }
};
