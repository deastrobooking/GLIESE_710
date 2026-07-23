#include "SynthEngine.h"
#include <cmath>

namespace
{
float pF(std::atomic<float>* p, float def = 0.f) noexcept
{
    return p ? p->load(std::memory_order_relaxed) : def;
}

int pI(std::atomic<float>* p, int def = 0) noexcept
{
    return p ? (int)p->load(std::memory_order_relaxed) : def;
}

bool pB(std::atomic<float>* p, bool def = false) noexcept
{
    return p ? p->load(std::memory_order_relaxed) > 0.5f : def;
}
}

void SynthEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    for (auto& v : voices) v.prepare(sampleRate);
    modSequencer.prepare(sampleRate);
    arp.prepare(sampleRate);
    arpMidi.ensureSize(4096);
    renderMidi.ensureSize(4096);
}

void SynthEngine::reset()
{
    for (auto& v : voices) v.reset();
    modSequencer.reset();
    arp.reset();
    feedbackL = feedbackR = 0.f;
    ageSampleL = ageSampleR = 0.f;
    ageCounter = 0;
    heldNoteCounts = {};
    sustainDown = {};
}

void SynthEngine::setAPVTS(juce::AudioProcessorValueTreeState* a)
{
    apvts = a;
    modSequencer.setAPVTS(a);
    buildParamCache();
}

void SynthEngine::setHostBPM(double bpm)
{
    modSequencer.setHostBPM(bpm);
    arp.setHostBPM(bpm);
    for (auto& v : voices)
        v.setHostBPM(bpm);
}

void SynthEngine::setHostPosition(std::optional<double> ppqPosition)
{
    modSequencer.setHostPosition(ppqPosition);
}

void SynthEngine::buildParamCache()
{
    if (apvts == nullptr)
    {
        arpParams = {};
        return;
    }

    auto get = [&](const char* id) { return apvts->getRawParameterValue(id); };
    arpParams.on      = get("arp_on");
    arpParams.pattern = get("arp_pattern");
    arpParams.rate    = get("arp_rate");
    arpParams.sync    = get("arp_sync");
    arpParams.octaves = get("arp_octaves");
    arpParams.gate    = get("arp_gate");
    arpParams.swing   = get("arp_swing");
    arpParams.latch   = get("arp_latch");
    arpParams.built   = true;

    fxParams.age      = get("master_age");
    fxParams.feedback = get("master_feedback");
    fxParams.built    = true;

    for (auto& v : voices)
        v.buildParamCache(*apvts);
}

void SynthEngine::processMasterEffects(juce::AudioBuffer<float>& buffer,
                                        int startSample, int numSamples)
{
    if (numSamples <= 0) return;
    if (buffer.getNumChannels() < 2) return;
    if (!fxParams.built && apvts)
        buildParamCache();

    auto* L = buffer.getWritePointer(0, startSample);
    auto* R = buffer.getWritePointer(1, startSample);

    // ── Age / bitcrusher ──────────────────────────────────────────────────────
    float age = pF(fxParams.age, 0.f);
    if (age > 0.f)
    {
        int   srDiv  = 1 + (int)(age * 15.f);
        float quant  = (float)(1 << (int)(16.f - age * 12.f));
        float iquant = 1.f / quant;
        for (int s = 0; s < numSamples; ++s)
        {
            if (ageCounter <= 0)
            {
                ageSampleL = std::round(L[s] * quant) * iquant;
                ageSampleR = std::round(R[s] * quant) * iquant;
                ageCounter = srDiv;
            }
            --ageCounter;
            L[s] = ageSampleL;
            R[s] = ageSampleR;
        }
    }

    // ── Feedback loop ─────────────────────────────────────────────────────────
    float fb = pF(fxParams.feedback, 0.f);
    if (fb > 0.f)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            L[s] += fb * feedbackL;
            R[s] += fb * feedbackR;
            feedbackL = juce::jlimit(-1.f, 1.f, L[s]);
            feedbackR = juce::jlimit(-1.f, 1.f, R[s]);
        }
    }
    else
    {
        feedbackL = feedbackR = 0.f;
    }
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

void SynthEngine::noteOn(int note, float velocity, int channel)
{
    const int channelIndex = juce::jlimit(1, 16, channel) - 1;
    const int noteIndex = juce::jlimit(0, 127, note);
    auto& count = heldNoteCounts[(size_t) channelIndex][(size_t) noteIndex];
    if (count < 255)
        ++count;

    SynthVoice* voice = findFreeVoice();
    if (!voice) voice = stealVoice();
    if (voice) voice->noteOn(note, velocity, channel);
}

void SynthEngine::noteOff(int note, int channel)
{
    const int channelIndex = juce::jlimit(1, 16, channel) - 1;
    const int noteIndex = juce::jlimit(0, 127, note);
    auto& count = heldNoteCounts[(size_t) channelIndex][(size_t) noteIndex];
    if (count > 0)
        --count;
    if (sustainDown[(size_t) channelIndex] || count > 0)
        return;

    for (auto& v : voices)
        if (v.isActive() && v.getMidiNote() == note && v.getMidiChannel() == channel)
            v.noteOff();
}

void SynthEngine::setSustain(int channel, bool down)
{
    const int channelIndex = juce::jlimit(1, 16, channel) - 1;
    if (sustainDown[(size_t) channelIndex] == down)
        return;
    sustainDown[(size_t) channelIndex] = down;

    if (!down)
        for (auto& v : voices)
            if (v.isActive() && v.getMidiChannel() == channel
                && heldNoteCounts[(size_t) channelIndex][(size_t) v.getMidiNote()] == 0)
                v.noteOff();
}

void SynthEngine::allNotesOff()
{
    heldNoteCounts = {};
    sustainDown = {};
    for (auto& v : voices) v.noteOff();
}

//──────────────────────────────────────────────────────────────────────────────
void SynthEngine::handleMidiMessage(const juce::MidiMessage& msg, bool triggerNotes)
{
    if (triggerNotes && msg.isNoteOn())
        noteOn(msg.getNoteNumber(), msg.getFloatVelocity(), msg.getChannel());
    else if (triggerNotes && msg.isNoteOff())
        noteOff(msg.getNoteNumber(), msg.getChannel());
    else if (msg.isAllNotesOff())
        allNotesOff();
    else if (msg.isAftertouch() || msg.isChannelPressure())
    {
        float pressure = msg.isChannelPressure()
                             ? (float) msg.getChannelPressureValue() / 127.f
                             : (float) msg.getAfterTouchValue() / 127.f;
        for (auto& v : voices)
        {
            if (v.getMidiChannel() != msg.getChannel())
                continue;
            if (msg.isAftertouch() && v.getMidiNote() != msg.getNoteNumber())
                continue;
            if (msg.isAftertouch())
                v.setMidiCC(ModSource::Aftertouch, pressure);
            else
                v.setMidiCC(ModSource::ChanPressure, pressure);
        }
    }
    else if (msg.isController())
    {
        float norm = (float) msg.getControllerValue() / 127.f;
        if (msg.getControllerNumber() == 64)
            setSustain(msg.getChannel(), msg.getControllerValue() >= 64);
        else if (msg.getControllerNumber() == 1)
        {
            for (auto& v : voices)
                if (v.getMidiChannel() == msg.getChannel())
                    v.setMidiCC(ModSource::ModWheel, norm);
        }
        else if (msg.getControllerNumber() == 74)
        {
            for (auto& v : voices)
                if (v.getMidiChannel() == msg.getChannel())
                    v.setMidiCC(ModSource::CC74, norm);
        }
    }
    else if (msg.isPitchWheel())
    {
        float pb = (float) (msg.getPitchWheelValue() - 8192) / 8192.f;
        for (auto& v : voices)
            if (v.getMidiChannel() == msg.getChannel())
                v.setMidiCC(ModSource::PitchBend, pb);
    }
}

void SynthEngine::renderVoices(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0 || apvts == nullptr)
        return;

    for (auto& v : voices)
        if (v.isActive())
            v.renderBlock(buffer, startSample, numSamples, *apvts, modSequencer.getOutput());
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
    if (!arpParams.built)
        buildParamCache();

    modSequencer.processBlock(numSamples);

    bool arpOn = pB(arpParams.on);

    if (arpOn)
    {
        Arpeggiator::Params ap;
        ap.pattern  = static_cast<Arpeggiator::Pattern>(pI(arpParams.pattern));
        ap.rateIdx  = pI(arpParams.rate, 3);
        ap.sync     = pB(arpParams.sync, true);
        ap.octaves  = pI(arpParams.octaves, 1);
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

        ap.gate     = juce::jlimit(0.01f, 1.0f, pF(arpParams.gate, 0.8f) + arpGateMod);
        ap.swing    = pF(arpParams.swing);
        ap.latch    = pB(arpParams.latch);
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
    }
    else
    {
        renderTimedMidi(buffer, midiMessages, startSample, numSamples, true);
    }

    processMasterEffects(buffer, startSample, numSamples);
}

void SynthEngine::addWavetable(juce::String name, const float* samples, int numSamples)
{
    for (auto& v : voices)
        v.addWavetable(name, samples, numSamples);
}

bool SynthEngine::setTerrainHeightmap(int index, const juce::Image& image)
{
    if (voices.empty() || !voices.front().setTerrainHeightmap(index, image))
        return false;

    for (size_t i = 1; i < voices.size(); ++i)
        voices[i].shareTerrainHeightmap(index, voices.front());
    return true;
}
