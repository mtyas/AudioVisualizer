#pragma once
#include <juce_core/juce_core.h>
#include <vector>

class Engine;

struct PresetEntry
{
    juce::String name;
    juce::String category;
    juce::String author;
    juce::String tags;
    juce::String description;
    bool isFactory = true;
    juce::File file;
    juce::var stateData;
};

class PresetManager
{
public:
    explicit PresetManager (Engine& e) : engine (e) {}

    void initialise();

    const std::vector<PresetEntry>& getPresets() const noexcept { return presets; }
    int getCurrentIndex() const noexcept { return currentIndex; }
    juce::String getCurrentPresetName() const;

    void loadPreset (int index);
    void nextPreset();
    void prevPreset();
    bool saveUserPreset (const juce::String& name, const juce::String& category,
                         const juce::String& tags, const juce::String& description);
    void refreshUserPresets();

    static juce::File getUserPresetsFolder();

private:
    void buildFactoryPresets();

    Engine& engine;
    std::vector<PresetEntry> presets;
    int currentIndex = 0;
};
