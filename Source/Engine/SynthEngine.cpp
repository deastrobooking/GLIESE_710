#include "SynthEngine.h"
#include <cmath>

void SynthEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    for (auto& v : voices) v.prepare(sampleRate);
    arp.prepare(sampleRate);
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

void SynthEngine::setHostBPM(double bpm)
{
    arp.setHostBPM(bpm);
    for (auto& v : voices)
        v.setHostBPM(bpm);
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
void SynthEngine::handleMidiMessage(const juce::MidiMessage& msg, bool triggerNotes)
{
    if (triggerNotes && msg.isNoteOn())
        noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
    else if (triggerNotes && msg.isNoteOff())
        noteOff(msg.getNoteNumber());
    else if (msg.isAllNotesOff())
        allNotesOff();
    else if (msg.isAftertouch() || msg.isChannelPressure())
    {
        float pressure = msg.isChannelPressure()
                             ? msg.getChannelPressureValue() / 127.f
                             : msg.getAfterTouchValue() / 127.f;
        for (auto& v : voices)
        {
            v.setMidiCC(ModSource::Aftertouch, pressure);
            v.setMidiCC(ModSource::ChanPressure, pressure);
        }
    }
    else if (msg.isController())
    {
        float norm = msg.getControllerValue() / 127.f;
        if (msg.getControllerNumber() == 1)
            for (auto& v : voices)
                v.setMidiCC(ModSource::ModWheel, norm);
        else if (msg.getControllerNumber() == 74)
            for (auto& v : voices)
                v.setMidiCC(ModSource::CC74, norm);
    }
    else if (msg.isPitchWheel())
    {
        float pb = (msg.getPitchWheelValue() - 8192) / 8192.f;
        for (auto& v : voices)
            v.setMidiCC(ModSource::PitchBend, pb);
    }
}

void SynthEngine::renderVoices(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0 || apvts == nullptr)
        return;

    for (auto& v : voices)
        if (v.isActive())
            v.renderBlock(buffer, startSample, numSamples, *apvts);
}

void SynthEngine::renderTimedMidi(juce::AudioBuffer<float>& buffer,
                                  const juce::MidiBuffer& midi,
                                  int startSample,
                                  int numSamples,
                                  bool triggerNotes)
{
    const int blockEnd = startSample + numSamples;
    int renderedUntil = startSample;

    for (const auto meta : midi)
    {
        const int eventSample = juce::jlimit(startSample, blockEnd, meta.samplePosition);
        renderVoices(buffer, renderedUntil, eventSample - renderedUntil);
        handleMidiMessage(meta.getMessage(), triggerNotes);
        renderedUntil = eventSample;
    }

    renderVoices(buffer, renderedUntil, blockEnd - renderedUntil);
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
        float arpGateMod = 0.f;
        float arpRateMod = 0.f;
        int numModVoices = 0;
        for (auto& v : voices)
        {
            if (!v.isActive())
                continue;

            arpGateMod += v.getModDestination(ModDest::Arp_Gate);
            arpRateMod += v.getModDestination(ModDest::Arp_Rate);
            ++numModVoices;
        }

        if (numModVoices > 0)
        {
            arpGateMod /= (float)numModVoices;
            arpRateMod /= (float)numModVoices;
        }

        ap.gate     = juce::jlimit(0.01f, 1.0f, getF("arp_gate") + arpGateMod);
        ap.swing    = getF("arp_swing");
        ap.latch    = getI("arp_latch") != 0;
        ap.rateIdx  = juce::jlimit(0, Arpeggiator::kNumRates - 1,
                                   ap.rateIdx + (int)std::round(arpRateMod));
        arp.setParams(ap);

        renderMidi.clear();
        int segmentStart = startSample;
        const int blockEnd = startSample + numSamples;

        for (const auto meta : midiMessages)
        {
            const int eventSample = juce::jlimit(startSample, blockEnd, meta.samplePosition);
            arp.processBlock(renderMidi, segmentStart, eventSample - segmentStart);

            const auto msg = meta.getMessage();
            if (msg.isNoteOn() || msg.isNoteOff())
                arp.handleMidi(msg);
            else
            {
                if (msg.isAllNotesOff())
                    arp.reset();
                renderMidi.addEvent(msg, eventSample);
            }

            segmentStart = eventSample;
        }

        arp.processBlock(renderMidi, segmentStart, blockEnd - segmentStart);
        renderTimedMidi(buffer, renderMidi, startSample, numSamples, true);
        return;
    }

    renderTimedMidi(buffer, midiMessages, startSample, numSamples, true);
}

void SynthEngine::addWavetable(juce::String name, const float* samples, int numSamples)
{
    for (auto& v : voices)
        v.getWavetableOsc().addTable(name, samples, numSamples);
}
