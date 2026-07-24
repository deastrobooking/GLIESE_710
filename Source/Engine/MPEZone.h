#pragma once
#include <array>

class GlieseMPEZone
{
public:
    enum class Type { None, Lower, Upper };

    void reset() noexcept;
    bool handleController(int channel, int controller, int value) noexcept;

    Type getType() const noexcept { return type; }
    int getMasterChannel() const noexcept;
    int getMemberCount() const noexcept { return memberCount; }
    bool isMasterChannel(int channel) const noexcept;
    bool isMemberChannel(int channel) const noexcept;
    bool expressionAffectsChannel(int sourceChannel, int voiceChannel) const noexcept;

private:
    struct RpnSelection
    {
        int msb = 127;
        int lsb = 127;
    };

    std::array<RpnSelection, 16> rpn {};
    Type type = Type::None;
    int memberCount = 0;
};
