#pragma once

#include <array>
#include <cstdint>

namespace tfx
{
    
    inline std::array<uint32_t, 256> g_global_channel_uses{};

    inline void ResetGlobalChannelUsagePerFrame()
    {
        g_global_channel_uses.fill(0u);
    }

    inline void MarkGlobalChannelUsed(uint8_t idx)
    {
       
        ++g_global_channel_uses[idx];
    }
}
