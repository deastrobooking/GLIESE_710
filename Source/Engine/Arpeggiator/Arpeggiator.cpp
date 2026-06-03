#include "Arpeggiator.h"
#include <algorithm>
#include <random>

void Arpeggiator::prepare(double sampleRate)
{
    sr = sampleRate;
    reset();
}

void Arpeggiator::setParams(const Params& p)
{
    bool latchModeChanged = params.latch != p.latch;
    params = p;

    if (latchModeChanged)
        markNotesChanged();
}
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
    sortedNotes.clear();
    randomLatchOrder.clear();
    markNotesChanged();
}

int Arpeggiator::randInt(int max)
{
    if (max <= 1) return 0;
    rngState = rngState * 1664525u + 1013904223u;
    return (int)(rngState >> 16) % max;
}

bool Arpeggiator::NoteList::contains(int note) const noexcept
{
    for (int i = 0; i < count; ++i)
        if (data[(size_t) i] == note)
            return true;

    return false;
}

bool Arpeggiator::NoteList::push(int note) noexcept
{
    if (contains(note) || count >= capacity)
        return false;

    data[(size_t) count++] = note;
    return true;
}

bool Arpeggiator::NoteList::remove(int note) noexcept
{
    for (int i = 0; i < count; ++i)
    {
        if (data[(size_t) i] != note)
            continue;

        for (int j = i + 1; j < count; ++j)
            data[(size_t) (j - 1)] = data[(size_t) j];

        --count;
        return true;
    }

    return false;
}

void Arpeggiator::NoteList::copyFrom(const NoteList& other) noexcept
{
    count = other.count;
    for (int i = 0; i < count; ++i)
        data[(size_t) i] = other.data[(size_t) i];
}

Arpeggiator::NoteList& Arpeggiator::activeNotes()
{
    return params.latch ? latchedNotes : heldNotes;
}

void Arpeggiator::markNotesChanged()
{
    sortedNotesDirty = true;
    randomLatchDirty = true;
    arpIndex = 0;
}

const Arpeggiator::NoteList& Arpeggiator::orderedNotesForPattern()
{
    auto& notes = activeNotes();

    if (params.pattern == Pattern::Up || params.pattern == Pattern::Down
        || params.pattern == Pattern::UpDown || params.pattern == Pattern::DownUp)
    {
        if (sortedNotesDirty)
        {
            sortedNotes.copyFrom(notes);
            std::sort(sortedNotes.data.begin(), sortedNotes.data.begin() + sortedNotes.count);
            sortedNotesDirty = false;
        }
        return sortedNotes;
    }

    if (params.pattern == Pattern::RandomLatch)
    {
        if (randomLatchDirty)
        {
            randomLatchOrder.copyFrom(notes);
            for (int i = (int)randomLatchOrder.size() - 1; i > 0; --i)
                std::swap(randomLatchOrder.data[(size_t)i], randomLatchOrder.data[(size_t)randInt(i + 1)]);
            randomLatchDirty = false;
        }
        return randomLatchOrder;
    }

    return notes;
}

void Arpeggiator::handleMidi(const juce::MidiMessage& msg)
{
    if (msg.isNoteOn())
    {
        int note = msg.getNoteNumber();
        if (heldNotes.push(note) && !params.latch)
            markNotesChanged();

        if (params.latch)
        {
            if (!latchedNotes.remove(note))
                latchedNotes.push(note);
            markNotesChanged();
        }
    }
    else if (msg.isNoteOff())
    {
        int note = msg.getNoteNumber();
        if (heldNotes.remove(note) && !params.latch)
            markNotesChanged();
    }
}

int Arpeggiator::nextNote()
{
    const auto& notes = orderedNotesForPattern();
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
        {
            int r = randInt(totalSteps);
            noteIdx = r % (int)notes.size();
            oct     = r / (int)notes.size();
            break;
        }

        case Pattern::RandomLatch:
            noteIdx = arpIndex % (int)notes.size();
            oct     = arpIndex / (int)notes.size();
            arpIndex = (arpIndex + 1) % totalSteps;
            break;
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
    double effectiveBpm = params.sync ? bpm : 120.0;
    double beatsPerSec = effectiveBpm / 60.0;
    stepSamples        = (rateBeats / beatsPerSec) * sr;

    // Apply swing: alternate long/short steps
    double swingFactor = 1.0 + (swingToggle == 0 ? params.swing : -params.swing) * 0.5;
    double currentStepSamples = stepSamples * swingFactor;
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
