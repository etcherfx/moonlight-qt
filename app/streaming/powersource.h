#pragma once

#include <cstdint>

// Where this device is getting its power from. This is kept free of SDL
// and Qt so the logic can be tested on its own.
enum class PowerSource
{
    None,       // No battery, or the power state is unknown
    Ac,         // Plugged in (charging or fully charged)
    Battery     // Running on battery
};

inline const char* powerSourceName(PowerSource source)
{
    switch (source) {
    case PowerSource::Ac:
        return "ac";
    case PowerSource::Battery:
        return "battery";
    default:
        return "none";
    }
}

// Returns the frame rate to stream at for the given power source. The
// battery frame rate is only used if it's enabled (non-zero) and lower
// than the normal frame rate. It never raises the frame rate.
inline int getFrameRateForPowerSource(int fps, int fpsOnBattery, PowerSource source)
{
    if (source == PowerSource::Battery && fpsOnBattery > 0 && fpsOnBattery < fps) {
        return fpsOnBattery;
    }

    return fps;
}

// Accepts a power source change only after it has stayed the same for
// the whole delay, so briefly plugging in or unplugging is ignored.
class PowerSourceDebouncer
{
public:
    explicit PowerSourceDebouncer(PowerSource initial = PowerSource::None, uint32_t delayMs = 5000)
        : m_Current(initial),
          m_Candidate(initial),
          m_CandidateSince(0),
          m_DelayMs(delayMs)
    {
    }

    // Returns true exactly once for each accepted change. nowMs is a
    // millisecond tick count that may wrap around.
    bool update(PowerSource source, uint32_t nowMs)
    {
        if (source == m_Current) {
            // Back to the current source, so drop any pending change
            m_Candidate = m_Current;
            return false;
        }

        if (source != m_Candidate) {
            // A new candidate restarts the delay
            m_Candidate = source;
            m_CandidateSince = nowMs;
            return false;
        }

        // Unsigned subtraction is correct across tick wraparound
        if (static_cast<uint32_t>(nowMs - m_CandidateSince) < m_DelayMs) {
            return false;
        }

        m_Current = source;
        return true;
    }

    PowerSource current() const
    {
        return m_Current;
    }

private:
    PowerSource m_Current;
    PowerSource m_Candidate;
    uint32_t m_CandidateSince;
    uint32_t m_DelayMs;
};
