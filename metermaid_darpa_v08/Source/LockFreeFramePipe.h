#pragma once
#include <atomic>
#include <array>
#include "MeterFrame.h"

class LockFreeFramePipe
{
public:
    void push(const metermaid::MeterFrame& frame) noexcept
    {
        const uint32_t next = (writeIndex.load(std::memory_order_relaxed) + 1u) & 1u;
        buffers[next] = frame;
        writeIndex.store(next, std::memory_order_release);
        serial.fetch_add(1, std::memory_order_release);
    }

    bool pull(metermaid::MeterFrame& dst) noexcept
    {
        const auto now = serial.load(std::memory_order_acquire);
        if (now == lastSeen)
            return false;
        const uint32_t idx = writeIndex.load(std::memory_order_acquire) & 1u;
        dst = buffers[idx];
        lastSeen = now;
        return true;
    }

private:
    std::array<metermaid::MeterFrame, 2> buffers {};
    std::atomic<uint32_t> writeIndex { 0 };
    std::atomic<uint64_t> serial { 0 };
    uint64_t lastSeen = 0;
};
