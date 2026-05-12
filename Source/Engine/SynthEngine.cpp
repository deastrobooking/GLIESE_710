#include "SynthEngine.h"

void SynthEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    for (auto& v : voices) v.prepare(sampleRate);
    arp.prepare(sampleRate);

    // Seed the wavetable oscillator in each voice with a sine table
    for (auto& v : voices) v.getWavetableOsc().addSineTable();
}

void SynthEngine::reset()
{
    for (auto& v : voices) v.reset();
    arp.reset();
}

void SynthEngine::setAPVTS(juce::AudioProcessorValueTreeState* a)
{
    apvts = a;
}

//──────────────────────────────────────────────────────────────────────────────
SynthVoice* SynthEngine::findFreeVoice() const
{
    for (auto& v : voices)
        if (!v.isActive()) return const_cast<SynthVoice*>(&v);
    return nullptr;
}

SynthVoice* SynthEngine::stealVoice() const
{
    // Oldest-note stealing
    SynthVoice* oldest = nullptr;
    int maxAge = -1;
    for (auto& v : voices)
    {
        if (v.getVoiceAge() > maxAge)
        {
            maxAge = v.getVoiceAge();
            oldest = const_cast<SynthVoice*>(&v);
        }
    }
    return oldest;
}

void SynthEngine::noteOn(int note, float velocity)
{
    SynthVoice* voice = findFreeVoice();
    if (!voice) voice = stealVoice();
    if (voice) voice->noteOn(note, velocity);
}

void SynthEngine::noteOff(int note)
{
    for (auto& v : voices)
        if (v.isActive() && v.getMidiNote() == note)
            v.noteOff();
}

void SynthEngine::allNotesOff()
{
    for (auto& v : voices) v.noteOff();
}

//──────────────────────────────────────────────────────────────────────────────
void SynthEngine::handleMidi(const juce::MidiBuffer& midi, int startSample, int endSample)
{
    bool arpOn = false;
    if (apvts)
    {
        auto* p = apvts->getRawParameterValue("arp_on");
        arpOn = p && p->load() > 0.5f;
    }

    for (const auto meta : midi)
    {
        if (meta.samplePosition < startSample || meta.samplePosition >= endSample)
            continue;

        auto msg = meta.getMessage();

        if (arpOn)
        {
            arp.handleMidi(msg);
            continue;
        }

        if (msg.isNoteOn())
            noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            noteOff(msg.getNoteNumber());
        else if (msg.isAllNotesOff())
            allNotesOff();
    }
}

//──────────────────────────────────────────────────────────────────────────────
void SynthEngine::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiMessages,
                                   int startSample,
                                   int numSamples)
{
    if (!apvts) return;

    bool arpOn = apvts->getRawParameterValue("arp_on")->load() > 0.5f;

    if (arpOn)
    {
        // Sync arp params
        auto getF = [&](const juce::String& id) {
            auto* p = apvts->getRawParameterValue(id);
            return p ? p->load() : 0.f;
        };
        auto getI = [&](const juce::String& id) {
            auto* p = apvts->getRawParameterValue(id);
            return p ? (int)p->load() : 0;
        };

        Arpeggiator::Params ap;
        ap.pattern  = static_cast<Arpeggiator::Pattern>(getI("arp_pattern"));
        ap.rateIdx  = getI("arp_rate");
        ap.sync     = getI("arp_sync") != 0;
        ap.octaves  = getI("arp_octaves");
        ap.gate     = getF("arp_gate");
        ap.swing    = getF("arp_swing");
        ap.latch    = getI("arp_latch") != 0;
        arp.setParams(ap);

        // Get host BPM
        if (auto* ph = juce::AudioProcessor::getPlayHead())
        {
            juce::AudioPlayHead::CurrentPositionInfo info;
            if (ph->getCurrentPosition(info))
                arp.setHostBPM(info.bpm);
        }

        // Generate arp MIDI events into buffer
        juce::MidiBuffer arpMidi;
        arp.processBlock(arpMidi, startSample, numSamples);

        // Play arp notes
        for (const auto meta : arpMidi)
        {
            auto msg = meta.getMessage();
            if (msg.isNoteOn())
                noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())
                noteOff(msg.getNoteNumber());
        }
    }

    handleMidi(midiMessages, startSample, startSample + numSamples);

    // Render all active voices
    for (auto& v : voices)
        if (v.isActive())
            v.renderBlock(buffer, startSample, numSamples, *apvts);
}

void SynthEngine::addWavetable(juce::String name, const float* samples, int numSamples)
{
    for (auto& v : voices)
        v.getWavetableOsc().addTable(name, samples, numSamples);
}
