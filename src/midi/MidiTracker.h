#pragma once
#include "../core/SignalBus.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// Turns incoming MIDI into signals:
//   midi.gate/note/velocity/count/pitchbend/trigger, note.0..127 (velocity), cc.0..127 (0..1)
class MidiTracker
{
public:
    explicit MidiTracker (SignalBus& b) : bus (b)
    {
        sGate = b.add ("midi.gate");
        sNote = b.add ("midi.note");
        sVel = b.add ("midi.velocity");
        sCount = b.add ("midi.count");
        sBend = b.add ("midi.pitchbend");
        sTrig = b.add ("midi.trigger");
        sAfter = b.add ("midi.aftertouch");
        sMod = b.add ("midi.modwheel");
        sSustain = b.add ("midi.sustain");
        for (int i = 0; i < 128; ++i)
        {
            sNotes[(size_t) i] = b.add ("note." + juce::String (i));
            sCc[(size_t) i] = b.add ("cc." + juce::String (i));
        }
    }

    void prepare (double sr) { sampleRate = sr; }

    void process (const juce::MidiBuffer& midi, int numSamples)
    {
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                const auto n = (size_t) m.getNoteNumber();
                noteVel[n] = m.getFloatVelocity();
                lastNote = (float) n / 127.0f;
                lastVel = m.getFloatVelocity();
                trig = 1.0f;
            }
            else if (m.isNoteOff())
            {
                noteVel[(size_t) m.getNoteNumber()] = 0.0f;
            }
            else if (m.isController())
            {
                const int num = m.getControllerNumber();
                cc[(size_t) num] = (float) m.getControllerValue() / 127.0f;
                if (num == 123 || num == 120)
                    noteVel.fill (0.0f);
            }
            else if (m.isChannelPressure())
            {
                aftertouch = (float) m.getChannelPressureValue() / 127.0f;
            }
            else if (m.isAftertouch())
            {
                aftertouch = (float) m.getAfterTouchValue() / 127.0f;
            }
            else if (m.isPitchWheel())
            {
                bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
            }
        }

        int held = 0;
        for (int i = 0; i < 128; ++i)
        {
            if (noteVel[(size_t) i] > 0.0f)
                ++held;
            bus.set (sNotes[(size_t) i], noteVel[(size_t) i]);
            bus.set (sCc[(size_t) i], cc[(size_t) i]);
        }

        bus.set (sGate, held > 0 ? 1.0f : 0.0f);
        bus.set (sCount, (float) held);
        bus.set (sNote, lastNote);
        bus.set (sVel, held > 0 ? lastVel : 0.0f);
        bus.set (sBend, bend);
        bus.set (sTrig, trig);
        bus.set (sAfter, aftertouch);
        bus.set (sMod, cc[1]);
        bus.set (sSustain, cc[64] >= 0.5f ? 1.0f : 0.0f);
        if (sampleRate > 0.0)
            trig *= std::exp (-(float) numSamples / ((float) sampleRate * 0.15f));
    }

private:
    SignalBus& bus;
    double sampleRate = 44100.0;
    int sGate, sNote, sVel, sCount, sBend, sTrig, sAfter, sMod, sSustain;
    std::array<int, 128> sNotes {}, sCc {};
    std::array<float, 128> noteVel {}, cc {};
    float lastNote = 0, lastVel = 0, bend = 0, trig = 0, aftertouch = 0;
};
