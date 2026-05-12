#include "Arpeggiator.h"
#include <algorithm>
#include <random>

constexpr float Arpeggiator::kRateBeats[];

void Arpeggiator::prepare(double sampleRate)
{
    sr = sampleRate;
    heldNotes.reserve(128);
    latchedNotes.reserve(128);
    reset();
}

void Arpeggiator::setParams(const Params& p) { params = p; }
void Arpeggiator::setHostBPM(double b)        { bpm = juce::jlimit(20.0, 300.0, b); }

void Arpeggiator::reset()
{
    stepPhase    = 0.0;
    noteIsOn     = false;
    currentNoteNumber = -1;
    arpIndex     = 0;
    octaveStep   = 0;
    direction    = 1;
    swingToggle  = 0;
    heldNotes.clear();
    latchedNotes.clear();
}

int Arpeggiator::randInt(int max)
{
    if (max <= 1) return 0;
    rngState = rngState * 1664525u + 1013904223u;
    return (int)(rngState >> 16) % max;
}

std::vector<int>& Arpeggiator::activeNotes()
{
    return params.latch ? latchedNotes : heldNotes;
}

void Arpeggiator::handleMidi(const juce::MidiMessage& msg)
{
    if (msg.isNoteOn())
    {
        int note = msg.getNoteNumber();
        if (std::find(heldNotes.begin(), heldNotes.end(), note) == heldNotes.end())
        {
            heldNotes.push_back(note);
            std::sort(heldNotes.begin(), heldNotes.end());
        }

        if (params.latch)
        {
            // Toggle latch: add if not present, remove if already latched
            auto it = std::find(latchedNotes.begin(), latchedNotes.end(), note);
            if (it == latchedNotes.end())
            {
                latchedNotes.push_back(note);
                std::sort(latchedNotes.begin(), latchedNotes.end());
            }
            else
            {
                latchedNotes.erase(it);
            }
        }
    }
    else if (msg.isNoteOff())
    {
        int note = msg.getNoteNumber();
        heldNotes.erase(std::remove(heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    }
}

int Arpeggiator::nextNote()
{
    auto& notes = activeNotes();
    if (notes.empty()) return -1;

    int totalSteps = (int)notes.size() * params.octaves;
    arpIndex = juce::jlimit(0, totalSteps - 1, arpIndex);

    int noteIdx   = -1;
    int oct       = 0;

    switch (params.pattern)
    {
        case Pattern::Up:
            noteIdx = arpIndex % (int)notes.size();
            oct     = arpIndex / (int)notes.size();
            arpIndex = (arpIndex + 1) % totalSteps;
            break;

        case Pattern::Down:
            noteIdx = (totalSteps - 1 - arpIndex) % (int)notes.size();
            oct     = (totalSteps - 1 - arpIndex) / (int)notes.size();
            arpIndex = (arpIndex + 1) % totalSteps;
            break;

        case Pattern::UpDown:
        {
            if (totalSteps <= 1)
            {
                noteIdx = 0;
                oct = 0;
                break;
            }
            int pos = arpIndex % (2 * totalSteps - 2);
            if (pos >= totalSteps) pos = 2 * totalSteps - 2 - pos;
            noteIdx = pos % (int)notes.size();
            oct     = pos / (int)notes.size();
            arpIndex++;
            break;
        }

        case Pattern::DownUp:
        {
            if (totalSteps <= 1)
            {
                noteIdx = 0;
                oct = 0;
                break;
            }
            int pos = arpIndex % (2 * totalSteps - 2);
            if (pos >= totalSteps) pos = 2 * totalSteps - 2 - pos;
            pos = totalSteps - 1 - pos;
            noteIdx = pos % (int)notes.size();
            oct     = pos / (int)notes.size();
            arpIndex++;
            break;
        }

        case Pattern::AsPlayed:
            noteIdx = arpIndex % (int)notes.size();
            oct     = arpIndex / (int)notes.size();
            arpIndex = (arpIndex + 1) % totalSteps;
            break;

        case Pattern::Random:
        case Pattern::RandomLatch:
        {
            int r = randInt(totalSteps);
            noteIdx = r % (int)notes.size();
            oct     = r / (int)notes.size();
            break;
        }
    }

    if (noteIdx < 0 || noteIdx >= (int)notes.size()) return -1;
    return notes[noteIdx] + oct * 12;
}

void Arpeggiator::sendNoteOff(juce::MidiBuffer& buf, int sample)
{
    if (noteIsOn && currentNoteNumber >= 0)
    {
        buf.addEvent(juce::MidiMessage::noteOff(1, currentNoteNumber), sample);
        noteIsOn = false;
    }
}

void Arpeggiator::sendNoteOn(juce::MidiBuffer& buf, int sample, int note)
{
    currentNoteNumber = note;
    buf.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)100), sample);
    noteIsOn = true;
}

void Arpeggiator::processBlock(juce::MidiBuffer& outputMidi, int startSample, int numSamples)
{
    if (activeNotes().empty())
    {
        sendNoteOff(outputMidi, startSample);
        return;
    }

    float rateBeats    = kRateBeats[juce::jlimit(0, kNumRates - 1, params.rateIdx)];
    double beatsPerSec = bpm / 60.0;
    stepSamples        = (rateBeats / beatsPerSec) * sr;

    // Apply swing: alternate long/short steps
    double swingFactor = 1.0 + (swingToggle == 0 ? params.swing : -params.swing) * 0.5;
    double currentStepSamples = stepSamples * swingFactor;
    gateSamples        = currentStepSamples * (double)params.gate;

    for (int s = 0; s < numSamples; ++s)
    {
        double prevPhase = stepPhase;
        stepPhase += 1.0 / currentStepSamples;

        // Note-off when gate expires
        if (noteIsOn && prevPhase < (double)params.gate && stepPhase >= (double)params.gate)
            sendNoteOff(outputMidi, startSample + s);

        // Step advance
        if (stepPhase >= 1.0)
        {
            stepPhase -= 1.0;
            swingToggle ^= 1;
            currentStepSamples = stepSamples * (1.0 + (swingToggle == 0 ? params.swing : -params.swing) * 0.5);

            sendNoteOff(outputMidi, startSample + s);
            int note = nextNote();
            if (note >= 0 && note <= 127)
                sendNoteOn(outputMidi, startSample + s, note);
        }
    }
}
