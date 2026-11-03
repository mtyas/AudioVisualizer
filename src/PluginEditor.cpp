#include "PluginEditor.h"

namespace
{
constexpr int kTopBar = 52, kRail = 196, kStatus = 28, kPageHeader = 62;

struct PageInfo { const char* num; const char* name; const char* tagline; const char* title; const char* blurb; juce::Colour col; };

const PageInfo& pageInfo (int i)
{
    static const PageInfo infos[] = {
        { "1", "Compose", "Layers, scene & media",
          "Compose your picture",
          "Stack up to 13 layers: 3D scenes, shaders, spectrum analyzers, gradients, video, webcam and text. Pick a layer on the left to edit it; the top layer draws on top.",
          avtheme::accentCyan },
        { "2", "Style", "Effects & colour grade",
          "Style the whole image",
          "Post-effects applied to everything you composed: feedback trails, bloom, kaleidoscope, glitch and colour grading.",
          avtheme::accentAmber },
        { "3", "Animate", "Make it react to sound",
          "Animate with sound and motion",
          "Sources (audio, MIDI, LFOs, envelopes, chaos, sequencers) drive any setting via the matrix below. Shortcut: right-click any slider.",
          avtheme::accentPurple },
        { "4", "Patch", "Node graph (advanced)",
          "Patch the signal graph",
          "Advanced: wire signals through remap, smooth, math and script nodes into parameters for custom behaviour.",
          avtheme::accentGreen },
        { "5", "Library", "Presets & saving",
          "Preset library",
          "Start from a factory look, save your own, or import and export preset files.",
          avtheme::accentRed },
    };
    return infos[juce::jlimit (0, 4, i)];
}

const char* kMacroTip = "Macro knob: a free control you can use as a modulation source (macro.N), "
                        "e.g. to move several settings at once from one knob or your DAW.";
} // namespace

AVEditor::AVEditor (AVProcessor& p)
    : AudioProcessorEditor (p),
      proc (p),
      engine (p.engine),
      navBtns { NavButton (pageInfo (0).num, pageInfo (0).name, pageInfo (0).tagline, pageInfo (0).col),
                NavButton (pageInfo (1).num, pageInfo (1).name, pageInfo (1).tagline, pageInfo (1).col),
                NavButton (pageInfo (2).num, pageInfo (2).name, pageInfo (2).tagline, pageInfo (2).col),
                NavButton (pageInfo (3).num, pageInfo (3).name, pageInfo (3).tagline, pageInfo (3).col),
                NavButton (pageInfo (4).num, pageInfo (4).name, pageInfo (4).tagline, pageInfo (4).col) },
      layersPanel (p.engine),
      effectsPanel (p.engine),
      modPanel (p.engine, [this] { openFloatingModWindow(); }),
      graphPanel (p.engine),
      presetPanel (p.engine)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (1200, 680, 2560, 1600);

    for (int i = 0; i < 16; ++i)
        bandIdx[i] = engine.bus.find ("band." + juce::String (i));
    transientIdx = engine.bus.find ("audio.transient");
    midiGateIdx  = engine.bus.find ("midi.gate");
    camActiveIdx = engine.bus.find ("cam.active");

    // ---- Top bar ----
    for (auto* b : { &undoBtn, &redoBtn, &prevPresetBtn, &nextPresetBtn, &saveBtn, &initBtn,
                     &pinBtn, &openOutBtn, &fullscreenBtn, &helpBtn, &aboutBtn })
        addAndMakeVisible (b);
    aboutBtn.setTooltip ("About AudioVisualizer by mtyas: website, Ko-fi, source code and credits");
    aboutBtn.onClick = [this] { showAbout(); };
    addAndMakeVisible (presetCombo);

    undoBtn.setTooltip ("Undo the last change (Ctrl+Z)");
    redoBtn.setTooltip ("Redo (Ctrl+Y)");
    prevPresetBtn.setTooltip ("Previous preset");
    nextPresetBtn.setTooltip ("Next preset");
    presetCombo.setTooltip ("Choose a preset. Browse and search them all on the Library page.");
    saveBtn.setTooltip ("Save the current look as your own preset (opens the Library page)");
    initBtn.setTooltip ("Reset everything to defaults: parameters, modulation matrix and node graph");
    openOutBtn.setTooltip ("Open the visuals in their own window");
    fullscreenBtn.setTooltip ("Toggle fullscreen for the output window (F11)");
    pinBtn.setTooltip ("Keep the output window above all other windows, including your DAW (press T in the output window)");
    helpBtn.setTooltip ("How AudioVisualizer works");

    undoBtn.onClick = [this] { if (engine.undoManager.canUndo()) engine.undoManager.undo(); };
    redoBtn.onClick = [this] { if (engine.undoManager.canRedo()) engine.undoManager.redo(); };
    prevPresetBtn.onClick = [this] { engine.presets.prevPreset(); };
    nextPresetBtn.onClick = [this] { engine.presets.nextPreset(); };
    saveBtn.onClick = [this] { selectPage (Library); };
    initBtn.onClick = [this]
    {
        engine.performUndoable ("Initialize Patch", [this]
        {
            engine.visual.resetAllToDefaults();
            engine.modulation.resetToDefaultRoutes();
            engine.graph.loadDefault();
        });
        engine.setStatus ("Patch reset to defaults (Undo brings it back)");
    };

    rebuildPresetCombo();
    presetCombo.onChange = [this]
    {
        const int idx = presetCombo.getSelectedId() - 1;
        if (idx >= 0 && idx != engine.presets.getCurrentIndex())
            engine.presets.loadPreset (idx);
    };

    pinBtn.setClickingTogglesState (true);
    pinBtn.setToggleState (engine.outputPinned.load(), juce::dontSendNotification);
    pinBtn.onClick = [this]
    {
        engine.outputPinned = pinBtn.getToggleState();
        if (output != nullptr)
            output->setPinned (pinBtn.getToggleState());
        engine.setStatus (pinBtn.getToggleState() ? "Output window stays on top" : "Output window no longer stays on top");
    };
    openOutBtn.onClick = [this]
    {
        ensureOutput();
        output->setVisible (true);
        output->toFront (true);
    };
    fullscreenBtn.onClick = [this]
    {
        ensureOutput();
        output->setVisible (true);
        output->toggleFullscreen();
    };
    helpBtn.onClick = [this]
    {
        juce::CallOutBox::launchAsynchronously (std::make_unique<HelpContent>(), helpBtn.getScreenBounds(), nullptr);
    };

    // ---- Search ----
    searchBox.setTextToShowWhenEmpty ("Find any setting...  (Ctrl+F)", avtheme::textMuted);
    searchBox.setFont (avtheme::font (13.0f));
    searchBox.setIndents (10, 0);
    searchBox.setBorder ({ 0, 0, 0, 0 });
    searchBox.setJustification (juce::Justification::centredLeft);
    searchBox.setEscapeAndReturnKeysConsumed (true);
    searchBox.onTextChange = [this] { updateSearch(); };
    searchBox.onEscapeKey  = [this] { closeSearch(); };
    searchBox.onFocusLost  = [this] { searchResults.setVisible (false); };
    searchBox.onReturnKey  = [this]
    {
        if (auto* e = searchResults.getSelected())
            pickSearchResult (*e);
    };
    searchBox.onArrow = [this] (int d) { searchResults.moveSelection (d); };
    searchResults.onPick = [this] (const SearchEntry& e) { pickSearchResult (e); };
    addAndMakeVisible (searchBox);
    buildSearchIndex();

    // ---- Left rail ----
    for (int i = 0; i < numPages; ++i)
    {
        navBtns[i].setTooltip (juce::String (pageInfo (i).title) + " - " + pageInfo (i).blurb);
        navBtns[i].onClick = [this, i] { selectPage (i); };
        addAndMakeVisible (navBtns[i]);
    }

    for (int i = 0; i < 8; ++i)
    {
        auto& s = macroSliders[i];
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRange (0.0, 1.0, 0.005);
        s.setDoubleClickReturnValue (true, 0.5);
        s.setValue (engine.modulation.macros[(size_t) i].load(), juce::dontSendNotification);
        s.setTooltip ("Macro " + juce::String (i + 1) + ": " + kMacroTip);
        s.onValueChange = [this, i]
        {
            engine.modulation.macros[(size_t) i].store ((float) macroSliders[i].getValue(), std::memory_order_relaxed);
        };
        addAndMakeVisible (s);
    }

    // ---- Pages ----
    pages[Compose] = &layersPanel;
    pages[Style]   = &effectsPanel;
    pages[Animate] = &modPanel;
    pages[Patch]   = &graphPanel;
    pages[Library] = &presetPanel;
    for (auto* pg : pages)
        addChildComponent (pg);

    addChildComponent (searchResults); // last child => drawn above the pages

    selectPage (Compose);
    setSize (1240, 780);
    lastEditorTickTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (30);
}

AVEditor::~AVEditor()
{
    stopTimer();
    if (floatingModWindow != nullptr)
        floatingModWindow->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void AVEditor::showAbout()
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<AboutContent>(), aboutBtn.getScreenBounds(), nullptr);
}

void AVEditor::mouseDown (const juce::MouseEvent& e)
{
    // The logo / name in the top-left corner opens the About card too
    if (e.getPosition().x < kRail && e.getPosition().y < kTopBar)
        showAbout();
}

void AVEditor::ensureOutput()
{
    if (output == nullptr)
        output = std::make_unique<OutputWindow> (engine);
}

void AVEditor::openFloatingModWindow()
{
    if (floatingModWindow == nullptr)
    {
        floatingModWindow = std::make_unique<FloatingModulationWindow> (engine);
        floatingModWindow->setLookAndFeel (&lookAndFeel); // top-level window: doesn't inherit ours
    }
    floatingModWindow->setVisible (true);
    floatingModWindow->toFront (true);
}

void AVEditor::rebuildPresetCombo()
{
    presetCombo.clear (juce::dontSendNotification);
    const auto& list = engine.presets.getPresets();
    for (int i = 0; i < (int) list.size(); ++i)
        presetCombo.addItem (list[(size_t) i].name, i + 1);
    presetCombo.setSelectedId (engine.presets.getCurrentIndex() + 1, juce::dontSendNotification);
}

void AVEditor::selectPage (int page)
{
    activePage = juce::jlimit (0, numPages - 1, page);
    for (int i = 0; i < numPages; ++i)
    {
        navBtns[i].setToggleState (i == activePage, juce::dontSendNotification);
        pages[i]->setVisible (i == activePage);
    }
    resized();
    if (activePage == Compose) layersPanel.syncAll();
    if (activePage == Style)   effectsPanel.syncAll();
    if (activePage == Animate) modPanel.syncAll();
    repaint();
}

// ---------------------------------------------------------------------------------------------
// Search: every parameter plus the main areas of the app
// ---------------------------------------------------------------------------------------------
void AVEditor::buildSearchIndex()
{
    searchIndex.clear();
    for (int i = 0; i < engine.visual.getNumParams(); ++i)
    {
        const auto& info = engine.visual.getParamInfo (i);
        SearchEntry e;
        e.title = info.name;
        e.subtitle = info.category;
        e.paramId = info.id;
        e.haystack = (info.name + " " + info.id + " " + info.category).toLowerCase();
        searchIndex.push_back (std::move (e));
    }

    auto page = [this] (const char* title, const char* where, int tab, int sub, const char* keywords)
    {
        SearchEntry e;
        e.title = title;
        e.subtitle = where;
        e.tab = tab;
        e.subTab = sub;
        e.haystack = (juce::String (title) + " " + where + " " + keywords).toLowerCase();
        searchIndex.push_back (std::move (e));
    };
    page ("Layer stack, order & blend modes", "Compose", Compose, -1, "layers opacity reorder blend mix visibility");
    page ("Add a layer (3D scene, shader, spectrum, gradient, text...)", "Compose", Compose, -1, "add new layer extra more kinds spectrum analyzer gradient fill");
    page ("Fonts for text layers", "Compose > Text layer", Compose, -1, "font typeface text family bold italic");
    page ("Custom GLSL shader editor", "Compose > 2D Shader", Compose, -1, "glsl shadertoy code custom isf");
    page ("Post effects", "Style", Style, -1, "effects fx post processing");
    page ("Modulation matrix (routing table)", "Animate", Animate, -1, "route assign connect source destination amount");
    page ("Envelopes", "Animate", Animate, 0, "adsr attack decay sustain release trigger");
    page ("LFOs", "Animate", Animate, 1, "oscillator sine tempo sync rate");
    page ("Chaos attractors", "Animate", Animate, 2, "lorenz rossler duffing random drift");
    page ("Step sequencers", "Animate", Animate, 3, "steps pattern glide");
    page ("Audio input, source & filter", "Animate", Animate, 4, "sidechain mid side left right filter cutoff attack release transient sensitivity");
    page ("Node graph", "Patch", Patch, -1, "nodes patch script wasm remap smooth math");
    page ("Presets & saving", "Library", Library, -1, "preset save load import export browse factory user");
}

void AVEditor::updateSearch()
{
    const auto q = searchBox.getText().trim().toLowerCase();
    if (q.isEmpty())
    {
        searchResults.setVisible (false);
        return;
    }
    const auto tokens = juce::StringArray::fromTokens (q, " ", "");
    std::vector<SearchEntry> hits;
    for (const auto& e : searchIndex)
    {
        bool all = true;
        for (const auto& t : tokens)
            if (! e.haystack.contains (t)) { all = false; break; }
        if (! all)
            continue;
        if (e.paramId.isNotEmpty() && ! engine.visual.isParamAvailable (engine.visual.findParam (e.paramId)))
            continue; // belongs to a layer that doesn't exist right now
        hits.push_back (e);
        if (hits.size() >= 9)
            break;
    }
    // Names that start with the query first
    std::stable_sort (hits.begin(), hits.end(), [&] (const SearchEntry& a, const SearchEntry& b)
    {
        return a.title.toLowerCase().startsWith (q) && ! b.title.toLowerCase().startsWith (q);
    });

    if (hits.empty())
    {
        SearchEntry none;
        none.title = "No matching setting";
        none.subtitle = "try another word";
        hits.push_back (none);
    }
    searchResults.setResults (std::move (hits));
    const auto sb = searchBox.getBounds();
    searchResults.setBounds (sb.getX(), sb.getBottom() + 4, juce::jmax (sb.getWidth(), 360),
                             juce::jmin (9, searchResults.size()) * 36 + 12);
    searchResults.setVisible (true);
    searchResults.toFront (false);
}

void AVEditor::pickSearchResult (const SearchEntry& e)
{
    if (e.paramId.isEmpty() && e.tab < 0)
        return;
    closeSearch();
    if (e.paramId.isNotEmpty())
    {
        const bool isPost = e.paramId.startsWith ("post.") || e.paramId.startsWith ("global.");
        selectPage (isPost ? Style : Compose);
        if (isPost) effectsPanel.revealParam (e.paramId);
        else        layersPanel.revealParam (e.paramId);
        engine.setStatus ("Showing: " + e.title);
    }
    else
    {
        selectPage (e.tab);
        if (e.tab == Animate && e.subTab >= 0)
            modPanel.selectSourceTab (e.subTab);
    }
    grabKeyboardFocus();
}

void AVEditor::closeSearch()
{
    searchBox.clear();
    searchResults.setVisible (false);
    grabKeyboardFocus();
}

// ---------------------------------------------------------------------------------------------

bool AVEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        if (engine.undoManager.canUndo()) engine.undoManager.undo();
        return true;
    }
    if (k == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)
        || k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        if (engine.undoManager.canRedo()) engine.undoManager.redo();
        return true;
    }
    if (k == juce::KeyPress ('f', juce::ModifierKeys::commandModifier, 0))
    {
        searchBox.grabKeyboardFocus();
        return true;
    }
    return false;
}

void AVEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.001, 0.1, now - lastEditorTickTime);
    lastEditorTickTime = now;

    // If the floating OpenGL output window is not open yet, tick the modulation engine here
    // so LFOs, Chaos attractors, Step Sequencers, and modulated slider bars animate live!
    if (output == nullptr || ! output->isVisible())
        engine.modulation.tick (dt);

    undoBtn.setEnabled (engine.undoManager.canUndo());
    redoBtn.setEnabled (engine.undoManager.canRedo());
    if (pinBtn.getToggleState() != engine.outputPinned.load())
        pinBtn.setToggleState (engine.outputPinned.load(), juce::dontSendNotification); // changed with T in the output window

    const int curRev = engine.uiRevision.load();
    if (curRev != seenUiRev)
    {
        seenUiRev = curRev;
        rebuildPresetCombo();
        for (int i = 0; i < 8; ++i)
            if (! macroSliders[i].isMouseButtonDown())
                macroSliders[i].setValue (engine.modulation.macros[(size_t) i].load(), juce::dontSendNotification);
        layersPanel.syncAll();
        effectsPanel.syncAll();
        graphPanel.syncAll();
        modPanel.syncAll();
        presetPanel.syncAll();
    }
    else
    {
        switch (activePage)
        {
            case Compose: layersPanel.syncAll(); break;
            case Style:   effectsPanel.syncAll(); break;
            case Animate: modPanel.syncAll(); break;
            case Patch:   graphPanel.repaint(); break;
            default: break;
        }
    }

    if (floatingModWindow != nullptr && floatingModWindow->isVisible())
        floatingModWindow->syncAll();

    repaint (getSpectrumBounds());
    repaint (getStatusBounds());
}

juce::Rectangle<int> AVEditor::getSpectrumBounds() const
{
    // below the nav buttons, above the macros
    const int y = kTopBar + 34 + numPages * 56 + 40;
    return { 14, y, kRail - 28, 56 };
}

juce::Rectangle<int> AVEditor::getStatusBounds() const
{
    return { 0, getHeight() - kStatus, getWidth(), kStatus };
}

void AVEditor::paint (juce::Graphics& g)
{
    using namespace avtheme;
    g.fillAll (bgDark);

    // ---- Top bar ----
    g.setColour (bgHeader);
    g.fillRect (0, 0, getWidth(), kTopBar);
    g.setColour (borderSoft);
    g.drawHorizontalLine (kTopBar - 1, 0.0f, (float) getWidth());

    auto logo = juce::Rectangle<float> (16.0f, 10.0f, 32.0f, 32.0f);
    g.setGradientFill (juce::ColourGradient (accentCyan, logo.getX(), logo.getY(),
                                             accentPurple, logo.getRight(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 9.0f);
    g.setColour (bgDark.withAlpha (0.85f));
    const float bars[] = { 0.45f, 0.8f, 0.6f, 0.95f, 0.5f };
    for (int i = 0; i < 5; ++i)
    {
        const float bh = 20.0f * bars[i];
        g.fillRoundedRectangle (logo.getX() + 6.0f + (float) i * 4.6f, logo.getCentreY() - bh * 0.5f, 3.0f, bh, 1.5f);
    }
    g.setColour (textBright);
    g.setFont (font (15.5f, true));
    g.drawText ("AudioVisualizer", 58, 8, kRail - 60, 22, juce::Justification::centredLeft);
    g.setColour (textMuted);
    g.setFont (font (10.5f));
    g.drawText ("by mtyas", 58, 28, kRail - 60, 16, juce::Justification::centredLeft);

    // ---- Left rail ----
    auto rail = juce::Rectangle<int> (0, kTopBar, kRail, getHeight() - kTopBar - kStatus);
    g.setColour (bgPanel);
    g.fillRect (rail);
    g.setColour (borderSoft);
    g.drawVerticalLine (kRail - 1, (float) rail.getY(), (float) rail.getBottom());

    auto sectionLabel = [&] (const juce::String& text, int y)
    {
        g.setColour (textMuted);
        g.setFont (font (10.5f, true));
        g.drawText (text, 18, y, kRail - 30, 16, juce::Justification::centredLeft);
    };
    sectionLabel ("WORKFLOW", kTopBar + 12);

    const auto spec = getSpectrumBounds();
    sectionLabel ("INPUT LEVELS", spec.getY() - 22);
    auto specRect = spec.toFloat();
    g.setColour (bgDark);
    g.fillRoundedRectangle (specRect, 8.0f);
    const float bw = (specRect.getWidth() - 16.0f) / 16.0f;
    for (int i = 0; i < 16; ++i)
    {
        const float v = juce::jlimit (0.0f, 1.0f, engine.bus.get (bandIdx[i]));
        const float h = juce::jmax (1.5f, v * (specRect.getHeight() - 10.0f));
        g.setColour (juce::Colour::fromHSV (0.52f + (float) i / 48.0f, 0.7f, 0.95f, 0.95f));
        g.fillRoundedRectangle (specRect.getX() + 8.0f + (float) i * bw, specRect.getBottom() - 5.0f - h, bw - 2.0f, h, 1.5f);
    }
    const float tr = juce::jlimit (0.0f, 1.0f, engine.bus.get (transientIdx));
    g.setColour (accentAmber.withAlpha (0.2f + 0.8f * tr));
    g.fillEllipse (specRect.getRight() - 15.0f, specRect.getY() + 6.0f, 8.0f, 8.0f);

    const int macroLabelY = spec.getBottom() + 18;
    sectionLabel ("MACROS", macroLabelY);
    const int cell = (kRail - 24) / 4;
    for (int i = 0; i < 8; ++i)
    {
        g.setColour (textMuted);
        g.setFont (font (10.0f));
        g.drawText ("M" + juce::String (i + 1), 12 + (i % 4) * cell, macroLabelY + 20 + (i / 4) * 58 + 40, cell, 12, juce::Justification::centred);
    }

    // ---- Page header ----
    const auto& pi = pageInfo (activePage);
    g.setColour (textBright);
    g.setFont (font (20.0f, true));
    g.drawText (pi.title, kRail + 18, kTopBar + 10, getWidth() - kRail - 36, 28, juce::Justification::centredLeft);
    g.setColour (textMuted);
    g.setFont (font (12.5f));
    g.drawFittedText (pi.blurb, kRail + 18, kTopBar + 38, getWidth() - kRail - 36, 20, juce::Justification::topLeft, 1);

    // ---- Status bar ----
    auto foot = getStatusBounds();
    g.setColour (bgHeader);
    g.fillRect (foot);
    g.setColour (borderSoft);
    g.drawHorizontalLine (foot.getY(), 0.0f, (float) getWidth());

    const float bpm = engine.bus.get (engine.sBpm);
    const float fps = engine.measuredFps.load (std::memory_order_relaxed);
    const bool midiOn = engine.bus.get (midiGateIdx) > 0.5f;
    const bool camOn  = engine.bus.get (camActiveIdx) > 0.5f;
    const bool outOpen = output != nullptr && output->isVisible();

    struct Chip { juce::String text; bool on; int w; };
    const Chip chips[] = {
        { outOpen ? juce::String ((int) std::round (fps)) + " fps" : juce::String ("Output closed"), outOpen, outOpen ? 70 : 110 },
        { "Cam", camOn, 56 },
        { "MIDI", midiOn, 62 },
        { "BPM " + juce::String (bpm > 1.0f ? bpm : 120.0f, 1), bpm > 1.0f, 100 },
    };
    int cx = getWidth() - 12;
    for (const auto& c : chips)
    {
        cx -= c.w;
        auto cr = juce::Rectangle<float> ((float) cx, (float) foot.getY() + 5.0f, (float) c.w - 6.0f, (float) kStatus - 10.0f);
        g.setColour (c.on ? accentGreen.withAlpha (0.14f) : bgRaised);
        g.fillRoundedRectangle (cr, cr.getHeight() * 0.5f);
        g.setColour (c.on ? accentGreen : textMuted.withAlpha (0.6f));
        g.fillEllipse (cr.getX() + 8.0f, cr.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (c.on ? textBright : textMuted);
        g.setFont (font (11.0f));
        g.drawText (c.text, cr.withTrimmedLeft (18.0f).toNearestInt(), juce::Justification::centredLeft);
    }

    g.setColour (textNormal);
    g.setFont (font (12.0f));
    g.drawText (engine.getStatus(), 14, foot.getY(), cx - 24, kStatus, juce::Justification::centredLeft);
}

void AVEditor::resized()
{
    // ---- Top bar ----
    auto row = juce::Rectangle<int> (kRail, 10, getWidth() - kRail - 14, 32);
    undoBtn.setBounds (row.removeFromLeft (48));
    row.removeFromLeft (4);
    redoBtn.setBounds (row.removeFromLeft (48));
    row.removeFromLeft (14);

    prevPresetBtn.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (3);
    // Right-hand group (364) + the rest of the left group (~190) + at least 190 for search are reserved first.
    const int comboW = juce::jlimit (150, 280, row.getWidth() - 744);
    presetCombo.setBounds (row.removeFromLeft (comboW));
    row.removeFromLeft (3);
    nextPresetBtn.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (6);
    saveBtn.setBounds (row.removeFromLeft (64));
    row.removeFromLeft (4);
    initBtn.setBounds (row.removeFromLeft (40));
    row.removeFromLeft (14);

    // Right-hand output group, laid out from the right edge
    helpBtn.setBounds (row.removeFromRight (32));
    row.removeFromRight (10);
    fullscreenBtn.setBounds (row.removeFromRight (90));
    row.removeFromRight (4);
    openOutBtn.setBounds (row.removeFromRight (100));
    row.removeFromRight (4);
    pinBtn.setBounds (row.removeFromRight (110));
    row.removeFromRight (14);

    searchBox.setBounds (row);
    if (searchResults.isVisible())
        updateSearch();

    // ---- Left rail ----
    int y = kTopBar + 34;
    for (int i = 0; i < numPages; ++i)
    {
        navBtns[i].setBounds (0, y, kRail - 2, 56);
        y += 56;
    }
    const auto spec = getSpectrumBounds();
    const int macroTop = spec.getBottom() + 18 + 20;
    const int cell = (kRail - 24) / 4;
    for (int i = 0; i < 8; ++i)
        macroSliders[i].setBounds (12 + (i % 4) * cell + (cell - 40) / 2, macroTop + (i / 4) * 58, 40, 40);

    // About button at the foot of the rail
    aboutBtn.setBounds (12, getHeight() - kStatus - 36, kRail - 24, 28);

    // ---- Page area ----
    auto content = juce::Rectangle<int> (kRail + 8, kTopBar + kPageHeader, getWidth() - kRail - 16,
                                         getHeight() - kTopBar - kPageHeader - kStatus - 6);
    for (auto* pg : pages)
        if (pg != nullptr) // resized() can run during construction, before the pages are registered
            pg->setBounds (content);
}
