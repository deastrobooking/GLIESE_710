#pragma once
#include <JuceHeader.h>
#include <array>

class Arpeggiator
{
public:
    enum class Pattern { Up, Down, UpDown, DownUp, AsPlayed, Random, RandomLatch };

    // arp_rate index values, in beats
    static constexpr float kRateBeats[] = {
        1.f/8.f, 1.f/4.f, 1.f/2.f, 3.f/4.f,
        1.0f, 1.5f, 2.0f, 3.0f, 4.0f
    };
    static constexpr int kNumRates = 9;

    struct Params
    {
        Pattern pattern = Pattern::Up;
        int     rateIdx = 3;      // index into kRateBeats (default = dotted 8th)
        bool    sync    = true;
        int     octaves = 1;      // 1..4
        float   gate    = 0.8f;   // 0..1
        float   swing   = 0.f;    // 0..0.66
        bool    latch   = false;
    };

    void prepare(double sampleRate);
    void setParams(const Params& p);
    void setHostBPM(double bpm);
    void reset();

    void handleMidi(const juce::MidiMessage& msg);

    // Writes note-on/off MIDI events into buffer for this block
    void processBlock(juce::MidiBuffer& outputMidi, int startSample, int numSamples);

private:
    double sr  = 48000.0;
    double bpm = 120.0;
    Params params;

    struct Note
    {
        int number = -1;
        int channel = 1;
        juce::uint8 velocity = 100;
    };

    struct NoteList
    {
        static constexpr int capacity = 128;
        std::array<Note, capacity> data {};
        int count = 0;

        bool empty() const noexcept { return count == 0; }
        int size() const noexcept { return count; }
        const Note& operator[] (int index) const noexcept { return data[(size_t) index]; }
        void clear() noexcept { count = 0; }
        bool contains(int note, int channel) const noexcept;
        bool push(Note note) noexcept;
        bool remove(int note, int channel) noexcept;
        void copyFrom(const NoteList& other) noexcept;
    };

    NoteList heldNotes;     // currently pressed keys, in played order
    NoteList latchedNotes;  // latched
    NoteList sortedNotes;   // rebuilt only when active notes change
    NoteList randomLatchOrder;
    int arpIndex   = 0;             // position in the pattern
    int octaveStep = 0;
    int direction  = 1;            // +1 or -1 for UpDown/DownUp

    // Timing (in samples)
    double stepPhase         = 0.0;  // 0..1 within current step
    double stepSamples       = 0.0;
    bool   noteIsOn          = false;
    int    currentNoteNumber = -1;
    int    currentNoteChannel = 1;
    int    swingToggle       = 0;

    uint32_t rngState = 42;
    bool sortedNotesDirty = true;
    bool randomLatchDirty = true;

    NoteList& activeNotes();
    const NoteList& orderedNotesForPattern();
    void markNotesChanged();
    Note nextNote();
    void sendNoteOff(juce::MidiBuffer& buf, int sample);
    void sendNoteOn(juce::MidiBuffer& buf, int sample, const Note& note);
    int  randInt(int max);
};
