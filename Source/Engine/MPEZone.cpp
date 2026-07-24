#include "MPEZone.h"
#include <algorithm>

void GlieseMPEZone::reset() noexcept
{
    rpn = {};
    for (auto& selection : rpn)
        selection = {};
    type = Type::None;
    memberCount = 0;
}

bool GlieseMPEZone::handleController(int channel, int controller, int value) noexcept
{
    channel = std::clamp(channel, 1, 16);
    value = std::clamp(value, 0, 127);
    auto& selection = rpn[(size_t) (channel - 1)];

    if (controller == 101)
    {
        selection.msb = value;
        return true;
    }
    if (controller == 100)
    {
        selection.lsb = value;
        return true;
    }
    if (controller != 6 || selection.msb != 0 || selection.lsb != 6)
        return false;

    if (channel != 1 && channel != 16)
        return true;

    const auto requestedType = channel == 1 ? Type::Lower : Type::Upper;
    const int requestedMembers = std::clamp(value, 0, 15);
    if (requestedMembers == 0)
    {
        if (type == requestedType)
        {
            type = Type::None;
            memberCount = 0;
        }
        return true;
    }

    type = requestedType;
    memberCount = requestedMembers;
    return true;
}

int GlieseMPEZone::getMasterChannel() const noexcept
{
    if (type == Type::Lower) return 1;
    if (type == Type::Upper) return 16;
    return 0;
}

bool GlieseMPEZone::isMasterChannel(int channel) const noexcept
{
    return channel == getMasterChannel();
}

bool GlieseMPEZone::isMemberChannel(int channel) const noexcept
{
    if (type == Type::Lower)
        return channel >= 2 && channel <= 1 + memberCount;
    if (type == Type::Upper)
        return channel <= 15 && channel >= 16 - memberCount;
    return false;
}

bool GlieseMPEZone::expressionAffectsChannel(int sourceChannel, int voiceChannel) const noexcept
{
    if (type == Type::None)
        return sourceChannel == voiceChannel;
    if (isMasterChannel(sourceChannel))
        return isMasterChannel(voiceChannel) || isMemberChannel(voiceChannel);
    return sourceChannel == voiceChannel;
}
