#pragma once
#include "Theme.h"

class PresetBrowserPanel final : public juce::Component,
                                 private juce::ListBoxModel
{
public:
    explicit PresetBrowserPanel (Engine& e) : engine (e)
    {
        searchBox.setTextToShowWhenEmpty ("Search presets by name, tag, shader, webcam, chaos...", avtheme::textMuted);
        searchBox.onTextChange = [this] { rebuildFilteredList(); };
        addAndMakeVisible (searchBox);

        static const char* cats[] = {
            "All", "3D & Geometry", "Shaders & 2D", "Webcam & Interactive",
            "Live Looper & Glitch", "Text & VJ", "Chaos & Generative", "MIDI Performance", "User"
        };
        for (int i = 0; i < 9; ++i)
        {
            catBtns[i].setButtonText (cats[i]);
            catBtns[i].onClick = [this, i, c = juce::String (cats[i])]
            {
                selectedCategory = (i == 0) ? "" : c;
                for (int k = 0; k < 9; ++k)
                    catBtns[k].setToggleState (k == i, juce::dontSendNotification);
                rebuildFilteredList();
            };
            addAndMakeVisible (catBtns[i]);
        }
        catBtns[0].setToggleState (true, juce::dontSendNotification);

        listBox.setModel (this);
        listBox.setRowHeight (32);
        listBox.setColour (juce::ListBox::backgroundColourId, avtheme::bgDark);
        addAndMakeVisible (listBox);

        // Save user preset form
        nameEdit.setTextToShowWhenEmpty ("My Custom Visual Preset", avtheme::textMuted);
        catEdit.setText ("User", false);
        tagsEdit.setTextToShowWhenEmpty ("3d, shader, custom", avtheme::textMuted);
        descEdit.setMultiLine (true);
        descEdit.setReturnKeyStartsNewLine (true);
        descEdit.setTextToShowWhenEmpty ("Describe this preset...", avtheme::textMuted);

        for (auto* c : { (juce::Component*) &nameEdit, (juce::Component*) &catEdit,
                         (juce::Component*) &tagsEdit, (juce::Component*) &descEdit,
                         (juce::Component*) &saveUserBtn, (juce::Component*) &exportBtn, (juce::Component*) &importBtn })
            addAndMakeVisible (c);

        saveUserBtn.onClick = [this]
        {
            if (engine.presets.saveUserPreset (nameEdit.getText(), catEdit.getText(), tagsEdit.getText(), descEdit.getText()))
                rebuildFilteredList();
        };
        exportBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Export Full Preset (.avpreset)", juce::File(), "*.avpreset;*.json");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() != juce::File())
                        fc.getResult().replaceWithText (juce::JSON::toString (engine.saveFullState(), true));
                });
        };
        importBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Import Full Preset (.avpreset)", juce::File(), "*.avpreset;*.json");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() != juce::File())
                    {
                        const auto v = juce::JSON::parse (fc.getResult());
                        engine.performUndoable ("Import Preset File", [this, v] { engine.loadFullState (v); });
                    }
                });
        };

        rebuildFilteredList();
    }

    void syncAll()
    {
        listBox.repaint();
        repaint();
    }

    int getNumRows() override { return (int) filteredIndices.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= (int) filteredIndices.size())
            return;
        const int pIdx = filteredIndices[(size_t) row];
        const auto& p  = engine.presets.getPresets()[(size_t) pIdx];
        const bool isCurrent = (pIdx == engine.presets.getCurrentIndex());

        g.setColour (selected || isCurrent ? avtheme::accentCyan.withAlpha (0.22f)
                                           : (row % 2 == 0 ? avtheme::bgDark : avtheme::bgPanel));
        g.fillRect (0, 0, width, height);

        g.setColour (isCurrent ? avtheme::accentCyan : avtheme::textBright);
        g.setFont (juce::FontOptions (13.0f, isCurrent ? juce::Font::bold : juce::Font::plain));
        g.drawText (p.name, 12, 0, width / 2 - 16, height, juce::Justification::centredLeft);

        g.setColour (avtheme::accentPurple.brighter (0.3f));
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (p.category, width / 2, 0, 150, height, juce::Justification::centredLeft);

        g.setColour (avtheme::textMuted);
        g.drawText (p.tags, width / 2 + 155, 0, width - (width / 2 + 165), height, juce::Justification::centredLeft);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row >= 0 && row < (int) filteredIndices.size())
        {
            const int pIdx = filteredIndices[(size_t) row];
            engine.presets.loadPreset (pIdx);
            const auto& p = engine.presets.getPresets()[(size_t) pIdx];
            nameEdit.setText (p.name, false);
            catEdit.setText (p.category, false);
            tagsEdit.setText (p.tags, false);
            descEdit.setText (p.description, false);
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        b.removeFromTop (64);
        auto right = b.removeFromRight (320);
        avtheme::drawCard (g, b, "Presets  -  " + juce::String (filteredIndices.size()) + " shown. Click one to load it", avtheme::accentCyan);
        avtheme::drawCard (g, right, "Details and saving", avtheme::accentAmber);

        const auto& all = engine.presets.getPresets();
        const int cur = engine.presets.getCurrentIndex();
        if (cur >= 0 && cur < (int) all.size())
        {
            const auto& p = all[(size_t) cur];
            auto info = right.reduced (14);
            info.removeFromTop (24);
            g.setColour (avtheme::accentCyan);
            g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            g.drawText (p.name, info.removeFromTop (22), juce::Justification::centredLeft);

            g.setColour (avtheme::textMuted);
            g.setFont (juce::FontOptions (11.5f));
            g.drawText ("Category: " + p.category + "   |   Author: " + p.author, info.removeFromTop (18), juce::Justification::centredLeft);
            g.drawText ("Tags: " + p.tags, info.removeFromTop (18), juce::Justification::centredLeft);
            info.removeFromTop (4);
            g.setColour (avtheme::textBright);
            g.drawFittedText (p.description, info.removeFromTop (52), juce::Justification::topLeft, 3);
        }

        g.setColour (avtheme::textMuted);
        g.setFont (juce::FontOptions (11.0f));
        const int x = right.getX() + 14;
        int y = right.getY() + 148;
        g.drawText ("Preset Name:", x, y, 90, 22, juce::Justification::centredLeft); y += 28;
        g.drawText ("Category:",    x, y, 90, 22, juce::Justification::centredLeft); y += 28;
        g.drawText ("Tags:",        x, y, 90, 22, juce::Justification::centredLeft); y += 28;
        g.drawText ("Description:", x, y, 90, 22, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto searchRow = b.removeFromTop (30).reduced (4, 2);
        searchBox.setBounds (searchRow);

        auto catRow = b.removeFromTop (30).reduced (4, 2);
        const int cw = catRow.getWidth() / 9;
        for (int i = 0; i < 9; ++i)
            catBtns[i].setBounds (catRow.removeFromLeft (cw).reduced (2, 0));

        b.removeFromTop (4);
        auto right = b.removeFromRight (320).reduced (14);
        listBox.setBounds (b.reduced (10, 28).withTrimmedTop (2));

        right.removeFromTop (144);
        nameEdit.setBounds (right.removeFromTop (24).withTrimmedLeft (86)); right.removeFromTop (4);
        catEdit.setBounds  (right.removeFromTop (24).withTrimmedLeft (86)); right.removeFromTop (4);
        tagsEdit.setBounds (right.removeFromTop (24).withTrimmedLeft (86)); right.removeFromTop (4);
        descEdit.setBounds (right.removeFromTop (52).withTrimmedLeft (86)); right.removeFromTop (8);

        saveUserBtn.setBounds (right.removeFromTop (28));
        right.removeFromTop (6);
        auto expRow = right.removeFromTop (26);
        exportBtn.setBounds (expRow.removeFromLeft (expRow.getWidth() / 2 - 3));
        importBtn.setBounds (expRow.withTrimmedLeft (6));
    }

private:
    void rebuildFilteredList()
    {
        filteredIndices.clear();
        const auto q = searchBox.getText().trim().toLowerCase();
        const auto& all = engine.presets.getPresets();
        for (int i = 0; i < (int) all.size(); ++i)
        {
            const auto& p = all[(size_t) i];
            if (selectedCategory.isNotEmpty())
            {
                if (selectedCategory == "User")
                {
                    if (p.isFactory) continue;
                }
                else if (p.category != selectedCategory)
                    continue;
            }
            if (q.isNotEmpty())
            {
                const auto hay = (p.name + " " + p.category + " " + p.tags + " " + p.description).toLowerCase();
                if (! hay.contains (q))
                    continue;
            }
            filteredIndices.push_back (i);
        }
        listBox.updateContent();
        listBox.repaint();
        repaint();
    }

    Engine& engine;
    juce::String selectedCategory;
    std::vector<int> filteredIndices;

    juce::TextEditor searchBox;
    juce::TextButton catBtns[9];
    juce::ListBox listBox;

    juce::TextEditor nameEdit, catEdit, tagsEdit, descEdit;
    juce::TextButton saveUserBtn { "Save User Preset" }, exportBtn { "Export .avpreset..." }, importBtn { "Import .avpreset..." };
    std::unique_ptr<juce::FileChooser> chooser;
};
