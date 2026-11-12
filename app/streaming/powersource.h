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
