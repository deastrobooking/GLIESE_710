#include "EnvelopeBank.h"

void EnvelopeBank::prepare(double sampleRate)
{
    for (auto& e : envs) e.prepare(sampleRate);
}

void EnvelopeBank::noteOn()
{
    for (auto& e : envs) e.noteOn();
}

void EnvelopeBank::noteOff()
{
    for (auto& e : envs) e.noteOff();
}

void EnvelopeBank::reset()
{
    for (auto& e : envs) e.reset();
}

bool EnvelopeBank::anyActive() const
{
    for (auto& e : envs)
        if (e.isActive()) return true;
    return false;
}
