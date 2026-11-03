#pragma once
#include "Theme.h"

class ModulationStudioPanel final : public juce::Component
{
public:
    explicit ModulationStudioPanel (Engine& e, std::function<void()> onPopOut = {})
        : engine (e), popOutCallback (std::move (onPopOut)),
          envView (e), lfoView (e), chaosView (e), seqView (e), audioView (e),
          matrixContent (e, *this)
    {
        const char* tabNames[5] = { "Envelopes", "LFOs", "Chaos", "Sequencers", "Audio input" };
        for (int i = 0; i < 5; ++i)
        {
            subTabBtns[i].setButtonText (tabNames[i]);
            subTabBtns[i].onClick = [this, i] { selectSourceTab (i); };
            addAndMakeVisible (subTabBtns[i]);
        }

        addChildComponent (envView);
        addChildComponent (lfoView);
        addChildComponent (chaosView);
        addChildComponent (seqView);
        addChildComponent (audioView);

        // Matrix toolbar
        for (auto* b : { &addRouteBtn, &defaultRoutesBtn, &clearRoutesBtn })
            addAndMakeVisible (b);
        if (popOutCallback)
            addAndMakeVisible (popOutBtn);

        addRouteBtn.onClick = [this]
        {
            ModRoute r;
            r.enabled     = true;
            r.source      = "lfo.1";
            r.destination = "scene.pulse";
            r.amount      = 0.5f;
            engine.performUndoable ("Add Modulation Route", [this, r] { engine.modulation.addRoute (r); });
            matrixContent.rebuildRows();
        };
        defaultRoutesBtn.onClick = [this]
        {
            engine.performUndoable ("Restore Default Mod Matrix", [this] { engine.modulation.resetToDefaultRoutes(); });
            matrixContent.rebuildRows();
        };
        clearRoutesBtn.onClick = [this]
        {
            engine.performUndoable ("Clear Mod Matrix", [this] { engine.modulation.clearRoutes(); });
            matrixContent.rebuildRows();
        };
        popOutBtn.onClick = [this] { if (popOutCallback) popOutCallback(); };

        matrixViewport.setViewedComponent (&matrixContent, false);
        matrixViewport.setScrollBarsShown (true, false);
        addAndMakeVisible (matrixViewport);

        selectSourceTab (0);
        matrixContent.rebuildRows();
    }

    void selectSourceTab (int idx)
    {
        activeSubTab = idx;
        for (int i = 0; i < 5; ++i)
            subTabBtns[i].setToggleState (i == idx, juce::dontSendNotification);
        envView.setVisible (idx == 0);
        lfoView.setVisible (idx == 1);
        chaosView.setVisible (idx == 2);
        seqView.setVisible (idx == 3);
        audioView.setVisible (idx == 4);
        resized();
        repaint();
    }

    void syncAll()
    {
        envView.sync();
        lfoView.sync();
        chaosView.sync();
        seqView.sync();
        audioView.sync();
        matrixContent.syncRows();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        auto topHalf = b.removeFromTop (242);
        topHalf.removeFromTop (28);
        avtheme::drawCard (g, topHalf, {}, avtheme::accentCyan);
        avtheme::drawCard (g, b, "Modulation matrix - each row sends one source to one setting", avtheme::accentAmber);

        // Column captions, aligned with MatrixContent::RowComp::resized()
        if (! matHeader.isEmpty())
        {
            const int x0 = matHeader.getX(), W = matrixViewport.getWidth() - 14;
            g.setColour (avtheme::textMuted);
            g.setFont (avtheme::font (10.5f, true));
            auto cap = [&] (const char* t, int x, int w)
            {
                g.drawText (t, x0 + x, matHeader.getY(), w, matHeader.getHeight(), juce::Justification::centredLeft);
            };
            cap ("ON", 4, 26);
            cap ("SOURCE", 34, 140);
            cap ("SCALED BY (OPTIONAL)", 179, 115);
            cap ("DESTINATION SETTING", 294, 230);
            cap ("AMOUNT", 530, 100);
            cap ("CURVE", W - 308 + 4, 100);
            cap ("LEVEL", W - 107 + 4, 70);
        }
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto topHalf = b.removeFromTop (242);
        auto tabRow = topHalf.removeFromTop (26).reduced (4, 0);
        const int tw = tabRow.getWidth() / 5;
        for (int i = 0; i < 5; ++i)
            subTabBtns[i].setBounds (tabRow.removeFromLeft (tw).reduced (2, 0));

        auto srcArea = topHalf.reduced (8);
        envView.setBounds (srcArea);
        lfoView.setBounds (srcArea);
        chaosView.setBounds (srcArea);
        seqView.setBounds (srcArea);
        audioView.setBounds (srcArea);

        auto matArea = b.reduced (8);
        matArea.removeFromTop (22);
        auto matBar = matArea.removeFromTop (26);
        addRouteBtn.setBounds (matBar.removeFromLeft (135).reduced (2, 0));
        defaultRoutesBtn.setBounds (matBar.removeFromLeft (135).reduced (2, 0));
        clearRoutesBtn.setBounds (matBar.removeFromLeft (115).reduced (2, 0));
        if (popOutCallback)
            popOutBtn.setBounds (matBar.removeFromRight (195).reduced (2, 0));

        matArea.removeFromTop (2);
        matHeader = matArea.removeFromTop (18);
        matrixViewport.setBounds (matArea);
        matrixContent.setWidthAndResize (matArea.getWidth() - 14);
    }

private:
    // ---------------- Sub-View 0: 4x Envelopes ----------------
    struct EnvSubView final : public juce::Component
    {
        explicit EnvSubView (Engine& e) : engine (e)
        {
            for (int i = 0; i < 4; ++i)
            {
                trig[i].addItemList ({ "Audio Transient", "MIDI Gate", "Host Beat", "Bass Threshold", "Auto Loop" }, 1);
                trig[i].onChange = [this, i]
                {
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    engine.modulation.envs[(size_t) i].triggerMode = trig[i].getSelectedItemIndex();
                };
                addAndMakeVisible (trig[i]);

                auto cfg = [this, i] (juce::Slider& s, double lo, double hi, int which)
                {
                    s.setSliderStyle (juce::Slider::LinearHorizontal);
                    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
                    s.setRange (lo, hi, 0.005);
                    s.onValueChange = [this, i, which, &s]
                    {
                        const juce::ScopedLock sl (engine.modulation.getLock());
                        auto& ev = engine.modulation.envs[(size_t) i];
                        if (which == 0) ev.attack  = (float) s.getValue();
                        if (which == 1) ev.decay   = (float) s.getValue();
                        if (which == 2) ev.sustain = (float) s.getValue();
                        if (which == 3) ev.release = (float) s.getValue();
                    };
                    addAndMakeVisible (s);
                };
                cfg (atk[i], 0.002, 1.5, 0);
                cfg (dec[i], 0.01,  2.0, 1);
                cfg (sus[i], 0.0,   1.0, 2);
                cfg (rel[i], 0.01,  2.5, 3);
            }
            sync();
        }

        void sync()
        {
            const juce::ScopedLock sl (engine.modulation.getLock());
            for (int i = 0; i < 4; ++i)
            {
                const auto& ev = engine.modulation.envs[(size_t) i];
                trig[i].setSelectedItemIndex (ev.triggerMode, juce::dontSendNotification);
                if (! atk[i].isMouseButtonDown()) atk[i].setValue (ev.attack, juce::dontSendNotification);
                if (! dec[i].isMouseButtonDown()) dec[i].setValue (ev.decay, juce::dontSendNotification);
                if (! sus[i].isMouseButtonDown()) sus[i].setValue (ev.sustain, juce::dontSendNotification);
                if (! rel[i].isMouseButtonDown()) rel[i].setValue (ev.release, juce::dontSendNotification);
            }
        }

        void paint (juce::Graphics& g) override
        {
            const int w = getWidth() / 4;
            for (int i = 0; i < 4; ++i)
            {
                auto card = juce::Rectangle<int> (i * w + 2, 2, w - 4, getHeight() - 4);
                avtheme::drawCard (g, card, "ENV " + juce::String (i + 1) + " (env." + juce::String (i + 1) + ")", avtheme::accentCyan);
                const float v = engine.bus.get (engine.bus.find ("env." + juce::String (i + 1)));
                auto bar = juce::Rectangle<float> ((float) card.getRight() - 68.0f, (float) card.getY() + 8.0f, 56.0f, 10.0f);
                g.setColour (avtheme::bgDark);
                g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (avtheme::accentCyan);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v)), 3.0f);

                g.setColour (avtheme::textMuted);
                g.setFont (juce::FontOptions (10.5f));
                const int x = card.getX() + 8;
                int y = card.getY() + 58;
                for (const char* lbl : { "Atk", "Dec", "Sus", "Rel" })
                {
                    g.drawText (lbl, x, y, 30, 22, juce::Justification::centredLeft);
                    y += 26;
                }
            }
        }

        void resized() override
        {
            const int w = getWidth() / 4;
            for (int i = 0; i < 4; ++i)
            {
                auto r = juce::Rectangle<int> (i * w + 10, 30, w - 20, getHeight() - 36);
                trig[i].setBounds (r.removeFromTop (24));
                r.removeFromTop (4);
                atk[i].setBounds (r.removeFromTop (22).withTrimmedLeft (30)); r.removeFromTop (4);
                dec[i].setBounds (r.removeFromTop (22).withTrimmedLeft (30)); r.removeFromTop (4);
                sus[i].setBounds (r.removeFromTop (22).withTrimmedLeft (30)); r.removeFromTop (4);
                rel[i].setBounds (r.removeFromTop (22).withTrimmedLeft (30));
            }
        }

        Engine& engine;
        juce::ComboBox trig[4];
        juce::Slider atk[4], dec[4], sus[4], rel[4];
    };

    // ---------------- Sub-View 1: 4x LFOs ----------------
    struct LfoSubView final : public juce::Component
    {
        explicit LfoSubView (Engine& e) : engine (e)
        {
            for (int i = 0; i < 4; ++i)
            {
                shape[i].addItemList ({ "Sine", "Triangle", "Saw Down", "Ramp Up", "Square", "Sample & Hold", "Smooth Random", "Exp Pulse" }, 1);
                shape[i].onChange = [this, i]
                {
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    engine.modulation.lfos[(size_t) i].shape = shape[i].getSelectedItemIndex();
                };
                addAndMakeVisible (shape[i]);

                syncBtn[i].setButtonText ("Sync");
                syncBtn[i].onClick = [this, i]
                {
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    engine.modulation.lfos[(size_t) i].tempoSync = syncBtn[i].getToggleState();
                };
                addAndMakeVisible (syncBtn[i]);

                divBox[i].addItemList ({ "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars" }, 1);
                divBox[i].onChange = [this, i]
                {
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    engine.modulation.lfos[(size_t) i].syncDiv = divBox[i].getSelectedItemIndex();
                };
                addAndMakeVisible (divBox[i]);

                auto cfg = [this, i] (juce::Slider& s, double lo, double hi, int which)
                {
                    s.setSliderStyle (juce::Slider::LinearHorizontal);
                    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 16);
                    s.setRange (lo, hi, 0.01);
                    s.onValueChange = [this, i, which, &s]
                    {
                        const juce::ScopedLock sl (engine.modulation.getLock());
                        auto& l = engine.modulation.lfos[(size_t) i];
                        if (which == 0) l.rateHz = (float) s.getValue();
                        if (which == 1) l.depth  = (float) s.getValue();
                        if (which == 2) l.smooth = (float) s.getValue();
                    };
                    addAndMakeVisible (s);
                };
                cfg (rate[i],   0.02, 16.0, 0);
                cfg (depth[i],  0.0,  1.0,  1);
                cfg (smooth[i], 0.002, 0.5, 2);
            }
            sync();
        }

        void sync()
        {
            const juce::ScopedLock sl (engine.modulation.getLock());
            for (int i = 0; i < 4; ++i)
            {
                const auto& l = engine.modulation.lfos[(size_t) i];
                shape[i].setSelectedItemIndex (l.shape, juce::dontSendNotification);
                syncBtn[i].setToggleState (l.tempoSync, juce::dontSendNotification);
                divBox[i].setSelectedItemIndex (l.syncDiv, juce::dontSendNotification);
                if (! rate[i].isMouseButtonDown())   rate[i].setValue (l.rateHz, juce::dontSendNotification);
                if (! depth[i].isMouseButtonDown())  depth[i].setValue (l.depth, juce::dontSendNotification);
                if (! smooth[i].isMouseButtonDown()) smooth[i].setValue (l.smooth, juce::dontSendNotification);
            }
        }

        void paint (juce::Graphics& g) override
        {
            const int w = getWidth() / 4;
            for (int i = 0; i < 4; ++i)
            {
                auto card = juce::Rectangle<int> (i * w + 2, 2, w - 4, getHeight() - 4);
                avtheme::drawCard (g, card, "LFO " + juce::String (i + 1) + " (lfo." + juce::String (i + 1) + ")", avtheme::accentGreen);
                const float v = engine.bus.get (engine.bus.find ("lfo." + juce::String (i + 1)));
                auto bar = juce::Rectangle<float> ((float) card.getRight() - 68.0f, (float) card.getY() + 8.0f, 56.0f, 10.0f);
                g.setColour (avtheme::bgDark);
                g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (avtheme::accentGreen);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v)), 3.0f);

                g.setColour (avtheme::textMuted);
                g.setFont (juce::FontOptions (10.5f));
                const int x = card.getX() + 8;
                int y = card.getY() + 84;
                for (const char* lbl : { "Rate", "Depth", "Smth" })
                {
                    g.drawText (lbl, x, y, 36, 22, juce::Justification::centredLeft);
                    y += 26;
                }
            }
        }

        void resized() override
        {
            const int w = getWidth() / 4;
            for (int i = 0; i < 4; ++i)
            {
                auto r = juce::Rectangle<int> (i * w + 10, 30, w - 20, getHeight() - 36);
                shape[i].setBounds (r.removeFromTop (24));
                r.removeFromTop (3);
                auto sRow = r.removeFromTop (24);
                syncBtn[i].setBounds (sRow.removeFromLeft (62));
                divBox[i].setBounds (sRow);
                r.removeFromTop (3);
                rate[i].setBounds (r.removeFromTop (22).withTrimmedLeft (36)); r.removeFromTop (4);
                depth[i].setBounds (r.removeFromTop (22).withTrimmedLeft (36)); r.removeFromTop (4);
                smooth[i].setBounds (r.removeFromTop (22).withTrimmedLeft (36));
            }
        }

        Engine& engine;
        juce::ComboBox shape[4], divBox[4];
        juce::ToggleButton syncBtn[4];
        juce::Slider rate[4], depth[4], smooth[4];
    };

    // ---------------- Sub-View 2: Chaos Attractors + Live 2D Phase Portrait ----------------
    struct ChaosSubView final : public juce::Component
    {
        explicit ChaosSubView (Engine& e) : engine (e)
        {
            auto cfg = [this] (juce::Slider& s, double lo, double hi, int which)
            {
                s.setSliderStyle (juce::Slider::LinearHorizontal);
                s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
                s.setRange (lo, hi, 0.01);
                s.onValueChange = [this, which, &s]
                {
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    auto& ch = engine.modulation.chaos;
                    if (which == 0) ch.speed        = (float) s.getValue();
                    if (which == 1) ch.sigma        = (float) s.getValue();
                    if (which == 2) ch.rho          = (float) s.getValue();
                    if (which == 3) ch.duffingDrive = (float) s.getValue();
                };
                addAndMakeVisible (s);
            };
            cfg (speed, 0.05, 4.0,  0);
            cfg (sigma, 2.0,  25.0, 1);
            cfg (rho,   8.0,  50.0, 2);
            cfg (drive, 0.05, 1.5,  3);
            sync();
        }

        void sync()
        {
            const juce::ScopedLock sl (engine.modulation.getLock());
            const auto& ch = engine.modulation.chaos;
            if (! speed.isMouseButtonDown()) speed.setValue (ch.speed, juce::dontSendNotification);
            if (! sigma.isMouseButtonDown()) sigma.setValue (ch.sigma, juce::dontSendNotification);
            if (! rho.isMouseButtonDown())   rho.setValue (ch.rho, juce::dontSendNotification);
            if (! drive.isMouseButtonDown()) drive.setValue (ch.duffingDrive, juce::dontSendNotification);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds();
            auto scope = b.removeFromLeft (240).reduced (4);
            avtheme::drawCard (g, scope, "LORENZ 2D PHASE SCOPE", avtheme::accentPurple);

            auto plot = scope.reduced (10, 28).toFloat();
            g.setColour (avtheme::bgDark);
            g.fillRoundedRectangle (plot, 4.0f);

            {
                const juce::ScopedLock sl (engine.modulation.getLock());
                const auto& ch = engine.modulation.chaos;
                juce::Path p;
                for (int i = 0; i < ChaosParams::kTrailLen; ++i)
                {
                    const int idx = (ch.trailHead + i) % ChaosParams::kTrailLen;
                    const float x = plot.getX() + ch.trailX[(size_t) idx] * plot.getWidth();
                    const float y = plot.getBottom() - ch.trailY[(size_t) idx] * plot.getHeight();
                    if (i == 0) p.startNewSubPath (x, y);
                    else        p.lineTo (x, y);
                }
                g.setColour (avtheme::accentCyan);
                g.strokePath (p, juce::PathStrokeType (1.8f));
            }

            auto mid = b.removeFromLeft (310).reduced (8);
            g.setColour (avtheme::textMuted);
            g.setFont (juce::FontOptions (11.5f));
            int y = mid.getY() + 14;
            for (const char* lbl : { "Chaos Speed", "Lorenz Sigma", "Lorenz Rho", "Duffing Drive" })
            {
                g.drawText (lbl, mid.getX(), y, 95, 24, juce::Justification::centredLeft);
                y += 34;
            }

            // Live meters for all 7 chaos outputs
            auto right = b.reduced (8);
            avtheme::drawCard (g, right, "LIVE CHAOS SIGNALS", avtheme::accentAmber);
            auto mr = right.reduced (12);
            mr.removeFromTop (24);
            static const char* sigs[7] = {
                "chaos.lorenzX", "chaos.lorenzY", "chaos.lorenzZ",
                "chaos.rossler", "chaos.duffing", "chaos.drift1", "chaos.drift2"
            };
            for (int i = 0; i < 7; ++i)
            {
                auto row = mr.removeFromTop (20);
                g.setColour (avtheme::textMuted);
                g.drawText (sigs[i], row.removeFromLeft (110), juce::Justification::centredLeft);
                auto bar = row.reduced (2, 4).toFloat();
                g.setColour (avtheme::bgDark);
                g.fillRoundedRectangle (bar, 3.0f);
                const float v = engine.bus.get (engine.bus.find (sigs[i]));
                g.setColour (avtheme::accentPurple);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v)), 3.0f);
            }
        }

        void resized() override
        {
            auto b = getLocalBounds();
            b.removeFromLeft (240);
            auto mid = b.removeFromLeft (310).reduced (8);
            mid.removeFromTop (14);
            speed.setBounds (mid.removeFromTop (24).withTrimmedLeft (95)); mid.removeFromTop (10);
            sigma.setBounds (mid.removeFromTop (24).withTrimmedLeft (95)); mid.removeFromTop (10);
            rho.setBounds   (mid.removeFromTop (24).withTrimmedLeft (95)); mid.removeFromTop (10);
            drive.setBounds (mid.removeFromTop (24).withTrimmedLeft (95));
        }

        Engine& engine;
        juce::Slider speed, sigma, rho, drive;
    };

    // ---------------- Sub-View 3: 2x 16-Step Sequencers ----------------
    struct SeqSubView final : public juce::Component
    {
        explicit SeqSubView (Engine& e) : engine (e)
        {
            for (int s = 0; s < 2; ++s)
            {
                syncBtn[s].setButtonText ("Sync");
                syncBtn[s].onClick = [this, s]
                {
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    engine.modulation.seqs[(size_t) s].tempoSync = syncBtn[s].getToggleState();
                };
                addAndMakeVisible (syncBtn[s]);

                divBox[s].addItemList ({ "1/16", "1/8", "1/4", "1/2", "1 Bar" }, 1);
                divBox[s].onChange = [this, s]
                {
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    engine.modulation.seqs[(size_t) s].syncDiv = divBox[s].getSelectedItemIndex();
                };
                addAndMakeVisible (divBox[s]);

                dirBox[s].addItemList ({ "Forward", "Reverse", "Ping-Pong", "Random" }, 1);
                dirBox[s].onChange = [this, s]
                {
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    engine.modulation.seqs[(size_t) s].direction = dirBox[s].getSelectedItemIndex();
                };
                addAndMakeVisible (dirBox[s]);

                glide[s].setSliderStyle (juce::Slider::LinearHorizontal);
                glide[s].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                glide[s].setRange (0.0, 1.0, 0.01);
                glide[s].onValueChange = [this, s]
                {
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    engine.modulation.seqs[(size_t) s].glide = (float) glide[s].getValue();
                };
                addAndMakeVisible (glide[s]);

                rndBtn[s].setButtonText ("Rand");
                rndBtn[s].onClick = [this, s]
                {
                    juce::Random r;
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    for (float& v : engine.modulation.seqs[(size_t) s].steps)
                        v = r.nextFloat();
                    repaint();
                };
                addAndMakeVisible (rndBtn[s]);
            }
            sync();
        }

        void sync()
        {
            const juce::ScopedLock sl (engine.modulation.getLock());
            for (int s = 0; s < 2; ++s)
            {
                const auto& sq = engine.modulation.seqs[(size_t) s];
                syncBtn[s].setToggleState (sq.tempoSync, juce::dontSendNotification);
                divBox[s].setSelectedItemIndex (sq.syncDiv, juce::dontSendNotification);
                dirBox[s].setSelectedItemIndex (sq.direction, juce::dontSendNotification);
                if (! glide[s].isMouseButtonDown())
                    glide[s].setValue (sq.glide, juce::dontSendNotification);
            }
        }

        void paint (juce::Graphics& g) override
        {
            const int w = getWidth() / 2;
            const juce::ScopedLock sl (engine.modulation.getLock());
            for (int s = 0; s < 2; ++s)
            {
                auto card = juce::Rectangle<int> (s * w + 2, 2, w - 4, getHeight() - 4);
                avtheme::drawCard (g, card, "16-STEP SEQUENCER " + juce::String (s + 1) + " (seq." + juce::String (s + 1) + ")", avtheme::accentAmber);
                const auto& sq = engine.modulation.seqs[(size_t) s];
                const auto bars = barArea[s];
                const float bw = (float) bars.getWidth() / 16.0f;
                for (int i = 0; i < 16; ++i)
                {
                    auto cell = juce::Rectangle<float> ((float) bars.getX() + i * bw + 1.5f, (float) bars.getY(), bw - 3.0f, (float) bars.getHeight());
                    g.setColour (avtheme::bgDark);
                    g.fillRoundedRectangle (cell, 3.0f);
                    const float v = juce::jlimit (0.0f, 1.0f, sq.steps[(size_t) i]);
                    const float h = v * cell.getHeight();
                    const bool active = (i == sq.currentStep);
                    g.setColour (active ? avtheme::accentCyan : avtheme::accentAmber.withAlpha (0.75f));
                    g.fillRoundedRectangle (cell.withTop (cell.getBottom() - h), 3.0f);
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override { editStepAt (e.position); }
        void mouseDrag (const juce::MouseEvent& e) override { editStepAt (e.position); }

        void editStepAt (juce::Point<float> pt)
        {
            for (int s = 0; s < 2; ++s)
            {
                const auto r = barArea[s].toFloat();
                if (r.contains (pt))
                {
                    const int step = juce::jlimit (0, 15, (int) ((pt.x - r.getX()) / r.getWidth() * 16.0f));
                    const float val = juce::jlimit (0.0f, 1.0f, 1.0f - (pt.y - r.getY()) / r.getHeight());
                    const juce::ScopedLock sl (engine.modulation.getLock());
                    engine.modulation.seqs[(size_t) s].steps[(size_t) step] = val;
                    repaint();
                }
            }
        }

        void resized() override
        {
            const int w = getWidth() / 2;
            for (int s = 0; s < 2; ++s)
            {
                auto r = juce::Rectangle<int> (s * w + 10, 30, w - 20, getHeight() - 36);
                auto top = r.removeFromTop (24);
                syncBtn[s].setBounds (top.removeFromLeft (56));
                divBox[s].setBounds (top.removeFromLeft (72).reduced (2, 0));
                dirBox[s].setBounds (top.removeFromLeft (95).reduced (2, 0));
                rndBtn[s].setBounds (top.removeFromRight (52));
                glide[s].setBounds (top.reduced (4, 0));
                r.removeFromTop (6);
                barArea[s] = r;
            }
        }

        Engine& engine;
        juce::Rectangle<int> barArea[2];
        juce::ToggleButton syncBtn[2];
        juce::ComboBox divBox[2], dirBox[2];
        juce::Slider glide[2];
        juce::TextButton rndBtn[2];
    };

    // ---------------- Sub-View 4: Audio Input Pre-Processor & Spectrum ----------------
    struct AudioPreSubView final : public juce::Component
    {
        explicit AudioPreSubView (Engine& e) : engine (e)
        {
            for (int i = 0; i < 16; ++i)
                bandIdx[i] = engine.bus.find ("band." + juce::String (i));

            sourceBox.addItemList ({ "Mid (L+R)", "Side (L-R)", "Left", "Right", "Sidechain 1", "Sidechain 2" }, 1);
            sourceBox.onChange = [this] { engine.analyzer.source = sourceBox.getSelectedItemIndex(); };
            addAndMakeVisible (sourceBox);

            filterBox.addItemList ({ "Filter Off", "Low-Pass", "High-Pass", "Band-Pass" }, 1);
            filterBox.onChange = [this] { engine.analyzer.filterType = filterBox.getSelectedItemIndex(); };
            addAndMakeVisible (filterBox);

            auto cfg = [this] (juce::Slider& s, double lo, double hi, double mid)
            {
                s.setSliderStyle (juce::Slider::LinearHorizontal);
                s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
                s.setRange (lo, hi, 0.1);
                if (mid > 0.0) s.setSkewFactorFromMidPoint (mid);
                addAndMakeVisible (s);
            };
            cfg (cutoff, 20.0, 20000.0, 1000.0);
            cfg (attack, 0.5, 200.0, 20.0);
            cfg (release, 10.0, 2000.0, 200.0);
            cfg (sens, 1.05, 4.0, 0.0);

            cutoff.onValueChange  = [this] { engine.analyzer.cutoffHz = (float) cutoff.getValue(); };
            attack.onValueChange  = [this] { engine.analyzer.attackMs = (float) attack.getValue(); };
            release.onValueChange = [this] { engine.analyzer.releaseMs = (float) release.getValue(); };
            sens.onValueChange    = [this] { engine.analyzer.transientSensitivity = (float) sens.getValue(); };
            sync();
        }

        void sync()
        {
            sourceBox.setSelectedItemIndex (engine.analyzer.source.load(), juce::dontSendNotification);
            filterBox.setSelectedItemIndex (engine.analyzer.filterType.load(), juce::dontSendNotification);
            if (! cutoff.isMouseButtonDown())  cutoff.setValue (engine.analyzer.cutoffHz.load(), juce::dontSendNotification);
            if (! attack.isMouseButtonDown())  attack.setValue (engine.analyzer.attackMs.load(), juce::dontSendNotification);
            if (! release.isMouseButtonDown()) release.setValue (engine.analyzer.releaseMs.load(), juce::dontSendNotification);
            if (! sens.isMouseButtonDown())    sens.setValue (engine.analyzer.transientSensitivity.load(), juce::dontSendNotification);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds();
            auto left = b.removeFromLeft (370).reduced (6);
            g.setColour (avtheme::textMuted);
            g.setFont (juce::FontOptions (11.5f));
            int y = left.getY() + 4;
            for (const char* lbl : { "Input Source", "Pre-Filter", "Cutoff (Hz)", "Env Attack", "Env Release", "Trans Sens" })
            {
                g.drawText (lbl, left.getX(), y, 90, 24, juce::Justification::centredLeft);
                y += 28;
            }

            auto spec = b.reduced (6);
            avtheme::drawCard (g, spec, "16-BAND FFT SPECTRUM & AUDIO FEATURES", avtheme::accentCyan);
            auto plot = spec.reduced (12, 28).toFloat();
            const float bw = plot.getWidth() / 16.0f;
            for (int i = 0; i < 16; ++i)
            {
                const float v = juce::jlimit (0.0f, 1.0f, engine.bus.get (bandIdx[i]));
                const float h = v * plot.getHeight();
                g.setColour (juce::Colour::fromHSV (0.52f + (float) i / 36.0f, 0.8f, 0.95f, 1.0f));
                g.fillRoundedRectangle (plot.getX() + i * bw + 2.0f, plot.getBottom() - h, bw - 4.0f, h, 2.0f);
            }
        }

        void resized() override
        {
            auto left = getLocalBounds().removeFromLeft (370).reduced (6);
            left.removeFromTop (4);
            auto row = [&] (juce::Component& c)
            {
                c.setBounds (left.removeFromTop (24).withTrimmedLeft (95));
                left.removeFromTop (4);
            };
            row (sourceBox);
            row (filterBox);
            row (cutoff);
            row (attack);
            row (release);
            row (sens);
        }

        Engine& engine;
        int bandIdx[16] {};
        juce::ComboBox sourceBox, filterBox;
        juce::Slider cutoff, attack, release, sens;
    };

    // ---------------- Bottom Half: Scrollable Modulation Matrix Table ----------------
    struct MatrixContent final : public juce::Component
    {
        MatrixContent (Engine& e, ModulationStudioPanel& p) : engine (e), parent (p) {}

        struct RowComp final : public juce::Component
        {
            RowComp (Engine& e, MatrixContent& mc, int idx) : engine (e), owner (mc), rowIndex (idx)
            {
                addAndMakeVisible (enBtn);
                addAndMakeVisible (srcBox);
                addAndMakeVisible (viaBox);
                addAndMakeVisible (dstBox);
                addAndMakeVisible (amtSlider);
                addAndMakeVisible (crvSlider);
                addAndMakeVisible (bipBtn);
                addAndMakeVisible (delBtn);

                const auto srcs = engine.modulation.getAllSourceNames();
                viaBox.addItem ("(Direct)", 1);
                for (int i = 0; i < srcs.size(); ++i)
                {
                    srcBox.addItem (srcs[i], i + 1);
                    viaBox.addItem (srcs[i], i + 2);
                }
                const auto myRoutes = engine.modulation.getRoutes();
                const int currentDst = rowIndex < (int) myRoutes.size() ? engine.visual.findParam (myRoutes[(size_t) rowIndex].destination) : -1;
                for (int i = 0; i < engine.visual.getNumParams(); ++i)
                {
                    if (i != currentDst && ! engine.visual.isParamAvailable (i))
                        continue;
                    const auto& info = engine.visual.getParamInfo (i);
                    dstBox.addItem (info.category + ": " + info.name + " (" + info.id + ")", i + 1);
                }

                amtSlider.setSliderStyle (juce::Slider::LinearHorizontal);
                amtSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 18);
                amtSlider.setRange (-2.0, 2.0, 0.01);

                crvSlider.setSliderStyle (juce::Slider::LinearHorizontal);
                crvSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 42, 18);
                crvSlider.setRange (0.25, 4.0, 0.05);

                bipBtn.setButtonText ("Bipolar");

                auto commit = [this]
                {
                    if (syncing) return;
                    auto routes = engine.modulation.getRoutes();
                    if (rowIndex < 0 || rowIndex >= (int) routes.size()) return;
                    auto r = routes[(size_t) rowIndex];
                    r.enabled = enBtn.getToggleState();
                    r.source  = srcBox.getText();
                    r.via     = viaBox.getSelectedId() <= 1 ? "" : viaBox.getText();
                    const int dstId = dstBox.getSelectedId() - 1;
                    if (dstId >= 0 && dstId < engine.visual.getNumParams())
                        r.destination = engine.visual.getParamInfo (dstId).id;
                    r.amount  = (float) amtSlider.getValue();
                    r.curve   = (float) crvSlider.getValue();
                    r.bipolar = bipBtn.getToggleState();
                    engine.modulation.updateRoute (rowIndex, r);
                };

                enBtn.onClick           = commit;
                srcBox.onChange         = commit;
                viaBox.onChange         = commit;
                dstBox.onChange         = commit;
                amtSlider.onValueChange = commit;
                crvSlider.onValueChange = commit;
                bipBtn.onClick          = commit;

                delBtn.onClick = [this]
                {
                    const int idx = rowIndex;
                    engine.performUndoable ("Remove Mod Route", [this, idx] { engine.modulation.removeRoute (idx); });
                    juce::MessageManager::callAsync ([&o = owner] { o.rebuildRows(); });
                };

                syncFromRoute();
            }

            void syncFromRoute()
            {
                const auto routes = engine.modulation.getRoutes();
                if (rowIndex < 0 || rowIndex >= (int) routes.size())
                    return;
                syncing = true;
                const auto& r = routes[(size_t) rowIndex];
                enBtn.setToggleState (r.enabled, juce::dontSendNotification);
                srcBox.setText (r.source, juce::dontSendNotification);
                if (r.via.isEmpty()) viaBox.setSelectedId (1, juce::dontSendNotification);
                else                 viaBox.setText (r.via, juce::dontSendNotification);
                const int dstIdx = engine.visual.findParam (r.destination);
                if (dstIdx >= 0)
                    dstBox.setSelectedId (dstIdx + 1, juce::dontSendNotification);
                if (! amtSlider.isMouseButtonDown()) amtSlider.setValue (r.amount, juce::dontSendNotification);
                if (! crvSlider.isMouseButtonDown()) crvSlider.setValue (r.curve, juce::dontSendNotification);
                bipBtn.setToggleState (r.bipolar, juce::dontSendNotification);
                lastContrib = r.lastContribution;
                syncing = false;
                repaint();
            }

            void paint (juce::Graphics& g) override
            {
                auto r = getLocalBounds().toFloat().reduced (1.0f);
                g.setColour (rowIndex % 2 == 0 ? avtheme::bgDark : avtheme::bgPanel);
                g.fillRoundedRectangle (r, 4.0f);

                // Live contribution meter
                auto m = meterBounds.toFloat().reduced (2.0f, 6.0f);
                g.setColour (avtheme::bgCard);
                g.fillRoundedRectangle (m, 3.0f);
                const float mid = m.getCentreX();
                const float w = juce::jlimit (-1.0f, 1.0f, lastContrib) * (m.getWidth() * 0.5f);
                g.setColour (w >= 0.0f ? avtheme::accentCyan : avtheme::accentAmber);
                g.fillRect (w >= 0.0f ? mid : mid + w, m.getY(), std::abs (w), m.getHeight());
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (4, 2);
                enBtn.setBounds (r.removeFromLeft (26));
                srcBox.setBounds (r.removeFromLeft (145).reduced (2, 0));
                viaBox.setBounds (r.removeFromLeft (115).reduced (2, 0));
                dstBox.setBounds (r.removeFromLeft (235).reduced (2, 0));
                delBtn.setBounds (r.removeFromRight (28).reduced (2, 0));
                meterBounds = r.removeFromRight (75);
                bipBtn.setBounds (r.removeFromRight (86));
                crvSlider.setBounds (r.removeFromRight (115).reduced (2, 0));
                amtSlider.setBounds (r.reduced (2, 0));
            }

            Engine& engine;
            MatrixContent& owner;
            int rowIndex;
            bool syncing = false;
            float lastContrib = 0.0f;
            juce::Rectangle<int> meterBounds;
            juce::ToggleButton enBtn, bipBtn;
            juce::ComboBox srcBox, viaBox, dstBox;
            juce::Slider amtSlider, crvSlider;
            juce::TextButton delBtn { "X" };
        };

        void rebuildRows()
        {
            rows.clear();
            const auto routes = engine.modulation.getRoutes();
            for (int i = 0; i < (int) routes.size(); ++i)
            {
                auto r = std::make_unique<RowComp> (engine, *this, i);
                addAndMakeVisible (*r);
                rows.push_back (std::move (r));
            }
            setWidthAndResize (juce::jmax (600, getWidth()));
        }

        void syncRows()
        {
            if (rows.size() != engine.modulation.getRoutes().size() || seenLayerRev != engine.visual.layerRevision.load())
            {
                seenLayerRev = engine.visual.layerRevision.load();
                rebuildRows();
            }
            else
                for (auto& r : rows) r->syncFromRoute();
        }

        void setWidthAndResize (int w)
        {
            const int h = juce::jmax (80, (int) rows.size() * 30 + 6);
            setSize (w, h);
            for (int i = 0; i < (int) rows.size(); ++i)
                rows[(size_t) i]->setBounds (0, i * 30, w, 28);
        }

        Engine& engine;
        ModulationStudioPanel& parent;
        int seenLayerRev = -1;
        std::vector<std::unique_ptr<RowComp>> rows;
    };

    Engine& engine;
    std::function<void()> popOutCallback;
    int activeSubTab = 0;
    juce::TextButton subTabBtns[5];
    EnvSubView envView;
    LfoSubView lfoView;
    ChaosSubView chaosView;
    SeqSubView seqView;
    AudioPreSubView audioView;

    juce::TextButton addRouteBtn { "+ Add Mod Route" }, defaultRoutesBtn { "Default Matrix" },
        clearRoutesBtn { "Clear All" }, popOutBtn { "Pop Out Modulation Window" };
    juce::Rectangle<int> matHeader;
    juce::Viewport matrixViewport;
    MatrixContent matrixContent;
};

// Separate floating window for the Modulation Studio & Matrix so users can place it side-by-side!
class FloatingModulationWindow final : public juce::DocumentWindow
{
public:
    explicit FloatingModulationWindow (Engine& e)
        : juce::DocumentWindow ("AudioVisualizer - Modulation Matrix & Sources Studio",
                                avtheme::bgDark, juce::DocumentWindow::allButtons),
          panel (e)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, true);
        panel.setSize (920, 560);
        setContentNonOwned (&panel, true);
        centreWithSize (920, 560);
        setVisible (true);
    }

    void closeButtonPressed() override { setVisible (false); }
    void syncAll() { if (isVisible()) panel.syncAll(); }

private:
    ModulationStudioPanel panel;
};
