#include "Core/CpuTimer.h"

namespace Core
{
    namespace
    {
        constexpr double kMsPerSecond = 1000.0;

        double TicksPerMs()
        {
            static const double s_dTicksPerMs = []()
            {
                LARGE_INTEGER liFrequency{};
                QueryPerformanceFrequency(&liFrequency);
                return static_cast<double>(liFrequency.QuadPart) / kMsPerSecond;
            }();
            return s_dTicksPerMs;
        }

        LONGLONG NowTicks()
        {
            LARGE_INTEGER liCounter{};
            QueryPerformanceCounter(&liCounter);
            return liCounter.QuadPart;
        }
    }

    CpuTimer::CpuTimer()
        : m_nStartTicks(NowTicks())
    {
    }

    void CpuTimer::Start()
    {
        m_nStartTicks = NowTicks();
    }

    double CpuTimer::ElapsedMs() const
    {
        return static_cast<double>(NowTicks() - m_nStartTicks) / TicksPerMs();
    }

    FrameTimer::FrameTimer()
        : m_timer()
        , m_dLastMs(0.0)
        , m_dDeltaMs(0.0)
        , m_uFrameCount(0)
    {
    }

    void FrameTimer::Tick()
    {
        const double dNowMs = m_timer.ElapsedMs();
        m_dDeltaMs = dNowMs - m_dLastMs;
        m_dLastMs = dNowMs;
        ++m_uFrameCount;
    }

    double FrameTimer::GetDeltaMs() const
    {
        return m_dDeltaMs;
    }

    double FrameTimer::GetTotalMs() const
    {
        return m_dLastMs;
    }

    UINT64 FrameTimer::GetFrameCount() const
    {
        return m_uFrameCount;
    }
}
