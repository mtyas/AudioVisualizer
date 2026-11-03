#pragma once
#include "Theme.h"

class NodeGraphPanel final : public juce::Component
{
public:
    explicit NodeGraphPanel (Engine& e) : engine (e)
    {
        auto makeAddBtn = [this] (juce::TextButton& b, ModGraph::Type t, const char* defaultName = "")
        {
            addAndMakeVisible (b);
            b.onClick = [this, t, defaultName]
            {
                ModGraph::Node n;
                n.type = t;
                n.name = defaultName;
                n.posX = 120.0f + (float) ((engine.graph.getNodes().size() * 47) % 360);
                n.posY = 80.0f  + (float) ((engine.graph.getNodes().size() * 63) % 240);
                if (t == ModGraph::Type::Smooth) { n.a = 0.02f; n.b = 0.18f; }
                if (t == ModGraph::Type::Lfo)    { n.a = 0.5f;  n.b = 0.0f; }
                engine.performUndoable ("Add Graph Node", [this, n]
                {
                    selectedNode = engine.graph.addNode (n);
                });
                syncInspector();
            };
        };

        makeAddBtn (addSrcBtn,    ModGraph::Type::Source,   "audio.env");
        makeAddBtn (addRemapBtn,  ModGraph::Type::Remap);
        makeAddBtn (addSmoothBtn, ModGraph::Type::Smooth);
        makeAddBtn (addMathBtn,   ModGraph::Type::Math);
        makeAddBtn (addLfoBtn,    ModGraph::Type::Lfo);
        makeAddBtn (addScriptBtn, ModGraph::Type::Script);
        makeAddBtn (addWasmBtn,   ModGraph::Type::Wasm);
        makeAddBtn (addSinkBtn,   ModGraph::Type::Sink,     "scene.pulse");

        for (auto* b : { &delBtn, &resetBtn, &saveBtn, &loadBtn })
            addAndMakeVisible (b);

        delBtn.onClick = [this]
        {
            if (selectedNode >= 0)
            {
                const int idx = selectedNode;
                selectedNode = -1;
                engine.performUndoable ("Delete Graph Node", [this, idx] { engine.graph.removeNode (idx); });
                syncInspector();
            }
        };
        resetBtn.onClick = [this]
        {
            engine.performUndoable ("Reset Default Graph", [this] { engine.graph.loadDefault(); });
            selectedNode = 0;
            syncInspector();
        };
        saveBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Export Node Graph JSON", juce::File(), "*.json");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() != juce::File())
                        fc.getResult().replaceWithText (juce::JSON::toString (engine.graph.toVar(), true));
                });
        };
        loadBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Import Node Graph JSON", juce::File(), "*.json");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() != juce::File())
                    {
                        const auto v = juce::JSON::parse (fc.getResult());
                        engine.performUndoable ("Import Node Graph", [this, v] { engine.graph.fromVar (v); });
                        syncInspector();
                    }
                });
        };

        // Inspector widgets
        addAndMakeVisible (inspTitle);
        inspTitle.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addChildComponent (lName);
        addChildComponent (nameBox);
        addChildComponent (lOp);
        addChildComponent (opBox);
        opBox.addItemList ({ "Add (A + B)", "Sub (A - B)", "Multiply (A * B)", "Min (A, B)", "Max (A, B)" }, 1);

        nameBox.onChange = [this] { commitInspectorChange(); };
        opBox.onChange   = [this] { commitInspectorChange(); };

        scriptBox.setMultiLine (true);
        scriptBox.setReturnKeyStartsNewLine (true);
        scriptBox.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.5f, juce::Font::plain));
        addChildComponent (scriptBox);
        addChildComponent (compileScriptBtn);
        addChildComponent (loadWasmBtn);

        compileScriptBtn.onClick = [this] { commitInspectorChange(); };
        loadWasmBtn.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Select Sandboxed WebAssembly (.wasm) Node Module", juce::File(), "*.wasm");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() != juce::File() && selectedNode >= 0)
                    {
                        auto nodes = engine.graph.getNodes();
                        if (selectedNode < (int) nodes.size())
                        {
                            auto n = nodes[(size_t) selectedNode];
                            n.wasmPath = fc.getResult().getFullPathName();
                            juce::String err;
                            engine.graph.updateNode (selectedNode, n, &err);
                            engine.setStatus (err.isEmpty() ? ("Loaded WASM node: " + fc.getResult().getFileName())
                                                            : ("WASM error: " + err));
                            syncInspector();
                        }
                    }
                });
        };

        for (int i = 0; i < 5; ++i)
        {
            addChildComponent (lParam[i]);
            sParam[i].setSliderStyle (juce::Slider::LinearHorizontal);
            sParam[i].setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
            sParam[i].setRange (-4.0, 4.0, 0.005);
            sParam[i].onValueChange = [this] { if (! syncing) commitInspectorChange(); };
            addChildComponent (sParam[i]);
        }
        syncInspector();
    }

    void syncAll()
    {
        syncInspector();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        b.removeFromTop (36);
        auto inspRect = b.removeFromRight (255);
        canvasRect = b.reduced (4);

        avtheme::drawCard (g, canvasRect, "Node graph  -  drag from an output dot to an input dot to connect; right-click a pin to disconnect", avtheme::accentCyan);
        avtheme::drawCard (g, inspRect, "Node inspector", avtheme::accentPurple);

        auto clip = canvasRect.reduced (4);
        clip.removeFromTop (24);
        g.saveState();
        g.reduceClipRegion (clip);

        // Subtle background grid
        g.setColour (avtheme::border.withAlpha (0.35f));
        for (int x = clip.getX(); x < clip.getRight(); x += 28)
            g.drawVerticalLine (x, (float) clip.getY(), (float) clip.getBottom());
        for (int y = clip.getY(); y < clip.getBottom(); y += 28)
            g.drawHorizontalLine (y, (float) clip.getX(), (float) clip.getRight());

        const auto nodes = engine.graph.getNodes();

        // Draw Bezier patch cables
        for (int i = 0; i < (int) nodes.size(); ++i)
        {
            const auto& dst = nodes[(size_t) i];
            const int numIn = getInputPinCount (dst.type);
            for (int p = 0; p < numIn; ++p)
            {
                const int srcIdx = dst.in[(size_t) p];
                if (srcIdx >= 0 && srcIdx < (int) nodes.size())
                {
                    const auto p0 = getOutputPinPos (nodes[(size_t) srcIdx]);
                    const auto p1 = getInputPinPos (dst, p);
                    drawCable (g, p0, p1, avtheme::accentCyan);
                }
            }
        }

        if (draggingCable && cableSrcNode >= 0 && cableSrcNode < (int) nodes.size())
            drawCable (g, getOutputPinPos (nodes[(size_t) cableSrcNode]), cableMousePos, avtheme::accentAmber);

        // Draw Nodes
        for (int i = 0; i < (int) nodes.size(); ++i)
            drawNode (g, i, nodes[(size_t) i]);

        g.restoreState();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pt = e.position;
        if (! canvasRect.toFloat().contains (pt))
            return;

        const auto nodes = engine.graph.getNodes();

        // Check output pins first (start cable drag)
        for (int i = (int) nodes.size() - 1; i >= 0; --i)
        {
            if (nodes[(size_t) i].type != ModGraph::Type::Sink)
            {
                if (getOutputPinPos (nodes[(size_t) i]).getDistanceFrom (pt) < 10.0f)
                {
                    draggingCable = true;
                    cableSrcNode  = i;
                    cableMousePos = pt;
                    selectedNode  = i;
                    syncInspector();
                    return;
                }
            }
            const int numIn = getInputPinCount (nodes[(size_t) i].type);
            for (int p = 0; p < numIn; ++p)
            {
                if (getInputPinPos (nodes[(size_t) i], p).getDistanceFrom (pt) < 10.0f)
                {
                    if (e.mods.isPopupMenu())
                    {
                        engine.performUndoable ("Disconnect Pin", [this, i, p] { engine.graph.disconnect (i, p); });
                        repaint();
                        return;
                    }
                }
            }
        }

        // Check node body hit
        for (int i = (int) nodes.size() - 1; i >= 0; --i)
        {
            const auto nr = getNodeBounds (nodes[(size_t) i]);
            if (nr.contains (pt))
            {
                selectedNode = i;
                draggingNode = true;
                dragOffset   = pt - nr.getTopLeft();
                syncInspector();
                repaint();
                return;
            }
        }

        selectedNode = -1;
        syncInspector();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingCable)
        {
            cableMousePos = e.position;
            repaint();
        }
        else if (draggingNode && selectedNode >= 0)
        {
            auto nodes = engine.graph.getNodes();
            if (selectedNode < (int) nodes.size())
            {
                auto n = nodes[(size_t) selectedNode];
                n.posX = juce::jlimit (10.0f, (float) canvasRect.getWidth() - 165.0f, e.position.x - (float) canvasRect.getX() - dragOffset.x);
                n.posY = juce::jlimit (30.0f, (float) canvasRect.getHeight() - 80.0f, e.position.y - (float) canvasRect.getY() - dragOffset.y);
                engine.graph.updateNode (selectedNode, n);
                repaint();
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (draggingCable)
        {
            const auto nodes = engine.graph.getNodes();
            for (int i = 0; i < (int) nodes.size(); ++i)
            {
                const int numIn = getInputPinCount (nodes[(size_t) i].type);
                for (int p = 0; p < numIn; ++p)
                {
                    if (getInputPinPos (nodes[(size_t) i], p).getDistanceFrom (e.position) < 14.0f)
                    {
                        const int src = cableSrcNode;
                        engine.performUndoable ("Connect Graph Nodes", [this, src, i, p]
                        {
                            engine.graph.connect (src, i, p);
                        });
                        break;
                    }
                }
            }
        }
        draggingCable = false;
        draggingNode  = false;
        cableSrcNode  = -1;
        repaint();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto top = b.removeFromTop (34).reduced (4, 2);
        for (auto* btn : { &addSrcBtn, &addRemapBtn, &addSmoothBtn, &addMathBtn, &addLfoBtn, &addScriptBtn, &addWasmBtn, &addSinkBtn })
            btn->setBounds (top.removeFromLeft (72).reduced (2, 0));
        top.removeFromLeft (8);
        delBtn.setBounds (top.removeFromLeft (86).reduced (2, 0));
        resetBtn.setBounds (top.removeFromLeft (90).reduced (2, 0));
        saveBtn.setBounds (top.removeFromLeft (74).reduced (2, 0));
        loadBtn.setBounds (top.removeFromLeft (74).reduced (2, 0));

        auto insp = b.removeFromRight (255).reduced (12);
        insp.removeFromTop (24);
        inspTitle.setBounds (insp.removeFromTop (24));
        insp.removeFromTop (6);
        if (nameBox.isVisible())
        {
            lName.setBounds (insp.removeFromTop (18));
            nameBox.setBounds (insp.removeFromTop (26));
            insp.removeFromTop (6);
        }
        if (opBox.isVisible())
        {
            lOp.setBounds (insp.removeFromTop (18));
            opBox.setBounds (insp.removeFromTop (26));
            insp.removeFromTop (6);
        }
        for (int i = 0; i < 5; ++i)
        {
            if (sParam[i].isVisible())
            {
                lParam[i].setBounds (insp.removeFromTop (18));
                sParam[i].setBounds (insp.removeFromTop (24));
                insp.removeFromTop (4);
            }
        }
        if (scriptBox.isVisible())
        {
            scriptBox.setBounds (insp.removeFromTop (120));
            insp.removeFromTop (4);
            compileScriptBtn.setBounds (insp.removeFromTop (26));
        }
        if (loadWasmBtn.isVisible())
            loadWasmBtn.setBounds (insp.removeFromTop (28));
        canvasRect = b.reduced (4);
    }

private:
    static constexpr float nodeW = 148.0f, nodeH = 68.0f;

    juce::Rectangle<float> getNodeBounds (const ModGraph::Node& n) const
    {
        return { (float) canvasRect.getX() + n.posX, (float) canvasRect.getY() + n.posY, nodeW, nodeH };
    }

    static int getInputPinCount (ModGraph::Type t)
    {
        switch (t)
        {
            case ModGraph::Type::Source:
            case ModGraph::Type::Constant:
            case ModGraph::Type::Lfo:    return 0;
            case ModGraph::Type::Math:
            case ModGraph::Type::Script:
            case ModGraph::Type::Wasm:   return 2;
            default:                     return 1;
        }
    }

    juce::Point<float> getInputPinPos (const ModGraph::Node& n, int pin) const
    {
        const auto r = getNodeBounds (n);
        const int count = getInputPinCount (n.type);
        const float y = count == 1 ? r.getCentreY() + 4.0f : (r.getY() + 30.0f + (float) pin * 20.0f);
        return { r.getX(), y };
    }

    juce::Point<float> getOutputPinPos (const ModGraph::Node& n) const
    {
        const auto r = getNodeBounds (n);
        return { r.getRight(), r.getCentreY() + 4.0f };
    }

    static void drawCable (juce::Graphics& g, juce::Point<float> p0, juce::Point<float> p1, juce::Colour col)
    {
        juce::Path path;
        path.startNewSubPath (p0);
        const float dx = juce::jmax (40.0f, std::abs (p1.x - p0.x) * 0.5f);
        path.cubicTo (p0.x + dx, p0.y, p1.x - dx, p1.y, p1.x, p1.y);
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (path, juce::PathStrokeType (5.0f));
        g.setColour (col);
        g.strokePath (path, juce::PathStrokeType (2.2f));
    }

    void drawNode (juce::Graphics& g, int idx, const ModGraph::Node& n) const
    {
        const auto r = getNodeBounds (n);
        const bool sel = (idx == selectedNode);

        juce::Colour hdrCol = avtheme::accentCyan;
        juce::String typeTitle = "NODE";
        switch (n.type)
        {
            case ModGraph::Type::Source:   hdrCol = avtheme::accentGreen;  typeTitle = "SOURCE"; break;
            case ModGraph::Type::Constant: hdrCol = avtheme::textMuted;    typeTitle = "CONST"; break;
            case ModGraph::Type::Remap:    hdrCol = avtheme::accentCyan;   typeTitle = "REMAP"; break;
            case ModGraph::Type::Smooth:   hdrCol = avtheme::accentPurple; typeTitle = "SMOOTH"; break;
            case ModGraph::Type::Math:     hdrCol = avtheme::accentAmber;  typeTitle = "MATH"; break;
            case ModGraph::Type::Lfo:      hdrCol = avtheme::accentGreen;  typeTitle = "GRAPH LFO"; break;
            case ModGraph::Type::Script:   hdrCol = avtheme::accentAmber;  typeTitle = "LUAU SCRIPT"; break;
            case ModGraph::Type::Wasm:     hdrCol = avtheme::accentPurple; typeTitle = "WASM MODULE"; break;
            case ModGraph::Type::Sink:     hdrCol = avtheme::accentRed;    typeTitle = "SINK OUT"; break;
        }

        g.setColour (avtheme::bgDark.withAlpha (0.95f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (hdrCol.withAlpha (0.28f));
        g.fillRoundedRectangle (r.getX(), r.getY(), r.getWidth(), 20.0f, 6.0f);
        g.setColour (sel ? juce::Colours::white : hdrCol.withAlpha (0.75f));
        g.drawRoundedRectangle (r, 6.0f, sel ? 2.0f : 1.0f);

        g.setColour (avtheme::textBright);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (typeTitle, r.withHeight (20.0f).reduced (8.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);

        juce::String sub = n.name;
        if (n.type == ModGraph::Type::Math)
        {
            static const char* ops[] = { "A + B", "A - B", "A * B", "Min(A,B)", "Max(A,B)" };
            sub = ops[(int) n.op];
        }
        else if (n.type == ModGraph::Type::Remap)
            sub = juce::String (n.a, 1) + ".." + juce::String (n.b, 1) + " -> " + juce::String (n.c, 1) + ".." + juce::String (n.d, 1);
        else if (n.type == ModGraph::Type::Smooth)
            sub = "Atk " + juce::String (n.a, 2) + "s Rel " + juce::String (n.b, 2) + "s";
        else if (n.type == ModGraph::Type::Lfo)
            sub = juce::String (n.a, 2) + " Hz";
        else if (n.type == ModGraph::Type::Constant)
            sub = "Val = " + juce::String (n.a, 2);
        else if (n.type == ModGraph::Type::Script)
            sub = "Luau expr (A=" + juce::String (n.a, 1) + " B=" + juce::String (n.b, 1) + ")";
        else if (n.type == ModGraph::Type::Wasm)
            sub = n.wasmPath.isNotEmpty() ? juce::File (n.wasmPath).getFileName() : "Load .wasm...";

        g.setColour (avtheme::textMuted);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (sub, juce::Rectangle<float> (r.getX() + 8.0f, r.getY() + 22.0f, r.getWidth() - 16.0f, 20.0f).toNearestInt(),
                    juce::Justification::centredLeft);

        // Live signal bar
        auto bar = juce::Rectangle<float> (r.getX() + 8.0f, r.getBottom() - 16.0f, r.getWidth() - 16.0f, 8.0f);
        g.setColour (avtheme::bgPanel);
        g.fillRoundedRectangle (bar, 3.0f);
        const float normV = juce::jlimit (0.0f, 1.0f, std::abs (n.value));
        g.setColour (hdrCol);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * normV), 3.0f);

        // Input pins
        const int numIn = getInputPinCount (n.type);
        for (int p = 0; p < numIn; ++p)
        {
            const auto pos = getInputPinPos (n, p);
            g.setColour (avtheme::bgDark);
            g.fillEllipse (pos.x - 5.0f, pos.y - 5.0f, 10.0f, 10.0f);
            g.setColour (avtheme::accentCyan);
            g.drawEllipse (pos.x - 5.0f, pos.y - 5.0f, 10.0f, 10.0f, 2.0f);
        }

        // Output pin
        if (n.type != ModGraph::Type::Sink)
        {
            const auto pos = getOutputPinPos (n);
            g.setColour (hdrCol);
            g.fillEllipse (pos.x - 5.0f, pos.y - 5.0f, 10.0f, 10.0f);
        }
    }

    void syncInspector()
    {
        syncing = true;
        lName.setVisible (false); nameBox.setVisible (false);
        lOp.setVisible (false);   opBox.setVisible (false);
        scriptBox.setVisible (false); compileScriptBtn.setVisible (false); loadWasmBtn.setVisible (false);
        for (int i = 0; i < 5; ++i) { lParam[i].setVisible (false); sParam[i].setVisible (false); }

        const auto nodes = engine.graph.getNodes();
        if (selectedNode < 0 || selectedNode >= (int) nodes.size())
        {
            inspTitle.setText ("Click a node to inspect", juce::dontSendNotification);
            syncing = false;
            resized();
            return;
        }

        const auto& n = nodes[(size_t) selectedNode];
        inspTitle.setText ("Node #" + juce::String (selectedNode), juce::dontSendNotification);

        auto showSlider = [this] (int idx, const juce::String& title, float val, float lo, float hi)
        {
            lParam[idx].setText (title, juce::dontSendNotification);
            lParam[idx].setVisible (true);
            sParam[idx].setRange (lo, hi, 0.005);
            sParam[idx].setValue (val, juce::dontSendNotification);
            sParam[idx].setVisible (true);
        };

        if (n.type == ModGraph::Type::Source)
        {
            lName.setText ("Signal Source", juce::dontSendNotification);
            lName.setVisible (true);
            nameBox.clear (juce::dontSendNotification);
            const auto srcs = engine.modulation.getAllSourceNames();
            for (int i = 0; i < srcs.size(); ++i)
                nameBox.addItem (srcs[i], i + 1);
            nameBox.setText (n.name, juce::dontSendNotification);
            nameBox.setVisible (true);
        }
        else if (n.type == ModGraph::Type::Sink)
        {
            lName.setText ("Target Destination", juce::dontSendNotification);
            lName.setVisible (true);
            nameBox.clear (juce::dontSendNotification);
            for (int i = 0; i < engine.visual.getNumParams(); ++i)
                if (engine.visual.isParamAvailable (i))
                    nameBox.addItem (engine.visual.getParamInfo (i).id, i + 1);
            nameBox.setText (n.name, juce::dontSendNotification);
            nameBox.setVisible (true);
        }
        else if (n.type == ModGraph::Type::Constant)
        {
            showSlider (0, "Constant Value", n.a, -2.0f, 2.0f);
        }
        else if (n.type == ModGraph::Type::Remap)
        {
            showSlider (0, "Input Min",  n.a, -2.0f, 2.0f);
            showSlider (1, "Input Max",  n.b, -2.0f, 2.0f);
            showSlider (2, "Output Min", n.c, -3.0f, 3.0f);
            showSlider (3, "Output Max", n.d, -3.0f, 3.0f);
            showSlider (4, "Curve Exp",  n.curve, 0.2f, 4.0f);
        }
        else if (n.type == ModGraph::Type::Smooth)
        {
            showSlider (0, "Attack (s)",  n.a, 0.001f, 1.5f);
            showSlider (1, "Release (s)", n.b, 0.005f, 2.5f);
        }
        else if (n.type == ModGraph::Type::Math)
        {
            lOp.setText ("Math Operation", juce::dontSendNotification);
            lOp.setVisible (true);
            opBox.setSelectedId ((int) n.op + 1, juce::dontSendNotification);
            opBox.setVisible (true);
        }
        else if (n.type == ModGraph::Type::Lfo)
        {
            showSlider (0, "Frequency (Hz)", n.a, 0.02f, 12.0f);
            showSlider (1, "Shape (0..3)",   n.b, 0.0f, 3.0f);
        }
        else if (n.type == ModGraph::Type::Script)
        {
            showSlider (0, "Param A", n.a, -4.0f, 4.0f);
            showSlider (1, "Param B", n.b, -4.0f, 4.0f);
            scriptBox.setText (n.scriptCode, false);
            scriptBox.setVisible (true);
            compileScriptBtn.setVisible (true);
        }
        else if (n.type == ModGraph::Type::Wasm)
        {
            showSlider (0, "Param A (local 3)", n.a, -4.0f, 4.0f);
            showSlider (1, "Param B (local 4)", n.b, -4.0f, 4.0f);
            loadWasmBtn.setVisible (true);
        }

        syncing = false;
        resized();
    }

    void commitInspectorChange()
    {
        if (syncing || selectedNode < 0)
            return;
        auto nodes = engine.graph.getNodes();
        if (selectedNode >= (int) nodes.size())
            return;
        auto n = nodes[(size_t) selectedNode];
        if (nameBox.isVisible())   n.name  = nameBox.getText();
        if (opBox.isVisible())     n.op    = (ModGraph::Op) juce::jlimit (0, 4, opBox.getSelectedId() - 1);
        if (sParam[0].isVisible()) n.a     = (float) sParam[0].getValue();
        if (sParam[1].isVisible()) n.b     = (float) sParam[1].getValue();
        if (sParam[2].isVisible()) n.c     = (float) sParam[2].getValue();
        if (sParam[3].isVisible()) n.d     = (float) sParam[3].getValue();
        if (sParam[4].isVisible()) n.curve = (float) sParam[4].getValue();
        if (scriptBox.isVisible()) n.scriptCode = scriptBox.getText();
        juce::String err;
        engine.graph.updateNode (selectedNode, n, &err);
        if (scriptBox.isVisible())
            engine.setStatus (err.isEmpty() ? "Script node compiled OK" : ("Script error: " + err));
        repaint();
    }

    Engine& engine;
    juce::Rectangle<int> canvasRect;
    int selectedNode = 0;
    bool draggingNode = false, draggingCable = false, syncing = false;
    int cableSrcNode = -1;
    juce::Point<float> dragOffset, cableMousePos;

    juce::TextButton addSrcBtn { "+ Source" }, addRemapBtn { "+ Remap" }, addSmoothBtn { "+ Smooth" },
        addMathBtn { "+ Math" }, addLfoBtn { "+ LFO" }, addScriptBtn { "+ Script" }, addWasmBtn { "+ WASM" }, addSinkBtn { "+ Sink" },
        delBtn { "Delete Node" }, resetBtn { "Default Graph" }, saveBtn { "Export..." }, loadBtn { "Import..." };

    juce::Label inspTitle, lName, lOp, lParam[5];
    juce::ComboBox nameBox, opBox;
    juce::Slider sParam[5];
    juce::TextEditor scriptBox;
    juce::TextButton compileScriptBtn { "Compile Script" }, loadWasmBtn { "Load .wasm File..." };
    std::unique_ptr<juce::FileChooser> chooser;
};
