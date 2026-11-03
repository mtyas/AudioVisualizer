#pragma once
#include "PluginProcessor.h"
#include "OutputWindow.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Shell.h"
#include "ui/LayersAndEffectsPanel.h"
#include "ui/NodeGraphEditor.h"
#include "ui/ModulationStudio.h"
#include "ui/PresetBrowserPanel.h"
#include <memory>

class AVEditor final : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit AVEditor (AVProcessor&);
    ~AVEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    // Page order follows the workflow: compose -> style -> animate -> patch -> library
    enum Page { Compose = 0, Style, Animate, Patch, Library, numPages };

    void timerCallback() override;
    void selectPage (int page);
    void openFloatingModWindow();
    void ensureOutput();
    void rebuildPresetCombo();
    void buildSearchIndex();
    void updateSearch();
    void pickSearchResult (const SearchEntry&);
    void closeSearch();
    juce::Rectangle<int> getSpectrumBounds() const;
    juce::Rectangle<int> getStatusBounds() const;

    AVProcessor& proc;
    Engine& engine;
    AVLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 450 };

    int bandIdx[16] {};
    int transientIdx = -1, midiGateIdx = -1, camActiveIdx = -1;
    int seenUiRev = -1;
    double lastEditorTickTime = 0.0;

    // Top bar
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" };
    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" }, saveBtn { "Save..." }, initBtn { "Init" };
    juce::ComboBox presetCombo;
    SearchField searchBox;
    SearchResultsList searchResults;
    std::vector<SearchEntry> searchIndex;
    juce::TextButton pinBtn { "Always on top" }, openOutBtn { "Open Output" }, fullscreenBtn { "Fullscreen" }, helpBtn { "?" };

    // Left rail
    NavButton navBtns[numPages];
    juce::Slider macroSliders[8];

    // Pages
    LayersStudioPanel layersPanel;
    EffectsRackPanel effectsPanel;
    ModulationStudioPanel modPanel;
    NodeGraphPanel graphPanel;
    PresetBrowserPanel presetPanel;
    juce::Component* pages[numPages] {};
    int activePage = Compose;

    std::unique_ptr<OutputWindow> output;
    std::unique_ptr<FloatingModulationWindow> floatingModWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AVEditor)
};
