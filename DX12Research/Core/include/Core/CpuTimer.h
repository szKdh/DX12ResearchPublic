#pragma once

#include <windows.h>

namespace Core
{
    class CpuTimer
    {
    public:
        CpuTimer();

        void Start();
        double ElapsedMs() const;

    private:
        LONGLONG m_nStartTicks;
    };

    class FrameTimer
    {
    public:
        FrameTimer();

        void Tick();
        double GetDeltaMs() const;
        double GetTotalMs() const;
        UINT64 GetFrameCount() const;

    private:
        CpuTimer m_timer;
        double m_dLastMs;
        double m_dDeltaMs;
        UINT64 m_uFrameCount;
    };
}
