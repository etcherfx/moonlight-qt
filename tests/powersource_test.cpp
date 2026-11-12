// Unit tests for the battery frame rate choice and the power source
// debouncer in app/streaming/powersource.h. Uses a fake clock (plain
// integers), so it needs no SDL or Qt.

#include "streaming/powersource.h"

#include <cstdint>
#include <cstdio>
#include <string_view>

static int s_Passed = 0;
static int s_Failed = 0;

#define CHECK(name, expr) \
    do { \
        if (expr) { \
            s_Passed++; \
            std::printf("PASS %s\n", name); \
        } \
        else { \
            s_Failed++; \
            std::printf("FAIL %s (%s:%d: %s)\n", name, __FILE__, __LINE__, #expr); \
        } \
    } while (0)

static const PowerSource AC = PowerSource::Ac;
static const PowerSource BATTERY = PowerSource::Battery;
static const PowerSource NONE = PowerSource::None;

static void testRateChoice()
{
    CHECK("battery rate off: ac gives normal", getFrameRateForPowerSource(120, 0, AC) == 120);
    CHECK("battery rate off: battery gives normal", getFrameRateForPowerSource(120, 0, BATTERY) == 120);
    CHECK("battery rate off: none gives normal", getFrameRateForPowerSource(120, 0, NONE) == 120);

    CHECK("battery 60 normal 120: battery gives 60", getFrameRateForPowerSource(120, 60, BATTERY) == 60);
    CHECK("battery 60 normal 120: ac gives 120", getFrameRateForPowerSource(120, 60, AC) == 120);
    CHECK("battery 60 normal 120: none gives 120", getFrameRateForPowerSource(120, 60, NONE) == 120);

    CHECK("battery rate equal to normal gives normal", getFrameRateForPowerSource(60, 60, BATTERY) == 60);
    CHECK("battery rate above normal gives normal", getFrameRateForPowerSource(60, 120, BATTERY) == 60);
    CHECK("negative battery rate gives normal", getFrameRateForPowerSource(60, -1, BATTERY) == 60);

    CHECK("source names", std::string_view(powerSourceName(AC)) == "ac" &&
                          std::string_view(powerSourceName(BATTERY)) == "battery" &&
                          std::string_view(powerSourceName(NONE)) == "none");
}

static void testShortChangeIgnored()
{
    PowerSourceDebouncer d(AC);
    int accepts = 0;
    for (uint32_t t = 0; t <= 4000; t += 1000) {
        accepts += d.update(BATTERY, 10000 + t) ? 1 : 0;
    }
    accepts += d.update(AC, 15000) ? 1 : 0;
    for (uint32_t t = 16000; t <= 30000; t += 1000) {
        accepts += d.update(AC, t) ? 1 : 0;
    }
    CHECK("change shorter than 5 s is never accepted", accepts == 0 && d.current() == AC);
}

static void testExactlyOnceAfterDelay()
{
    PowerSourceDebouncer d(AC);
    CHECK("first sighting is not accepted", !d.update(BATTERY, 1000));
    CHECK("not accepted at 4999 ms", !d.update(BATTERY, 1000 + 4999));
    CHECK("accepted at 5000 ms", d.update(BATTERY, 1000 + 5000));
    CHECK("current is battery after accept", d.current() == BATTERY);
    bool again = false;
    for (uint32_t t = 7000; t <= 20000; t += 1000) {
        again = again || d.update(BATTERY, t);
    }
    CHECK("not accepted again after the accept", !again);
}

// Feeds a sequence of changes, then keeps polling the last source once a
// second for 10 s. Returns the accept count and the time of the last accept.
static int runFlaps(PowerSourceDebouncer& d, const PowerSource* sources, int count,
                    uint32_t start, uint32_t step, uint32_t& lastChange, uint32_t& acceptTime)
{
    int accepts = 0;
    uint32_t t = start;
    for (int i = 0; i < count; i++) {
        t = start + i * step;
        if (d.update(sources[i], t)) {
            accepts++;
            acceptTime = t;
        }
    }
    lastChange = t;
    for (uint32_t ms = 1; ms <= 10000; ms++) {
        if (d.update(sources[count - 1], t + ms)) {
            accepts++;
            acceptTime = t + ms;
        }
    }
    return accepts;
}

static void testFlaps()
{
    {
        // Four changes inside 3 s ending back on ac
        PowerSourceDebouncer d(AC);
        const PowerSource seq[] = { BATTERY, AC, BATTERY, AC };
        uint32_t lastChange = 0, acceptTime = 0;
        int accepts = runFlaps(d, seq, 4, 1000, 750, lastChange, acceptTime);
        CHECK("even flaps ending on ac: zero accepts", accepts == 0);
        CHECK("even flaps ending on ac: normal rate",
              getFrameRateForPowerSource(120, 60, d.current()) == 120);
    }
    {
        // Three changes inside 3 s ending on battery
        PowerSourceDebouncer d(AC);
        const PowerSource seq[] = { BATTERY, AC, BATTERY };
        uint32_t lastChange = 0, acceptTime = 0;
        int accepts = runFlaps(d, seq, 3, 1000, 1000, lastChange, acceptTime);
        CHECK("odd flaps ending on battery: exactly one accept", accepts == 1);
        CHECK("odd flaps ending on battery: accepted 5 s after the last change",
              acceptTime == lastChange + 5000);
        CHECK("odd flaps ending on battery: battery rate",
              getFrameRateForPowerSource(120, 60, d.current()) == 60);
    }
    {
        // Five changes inside 3 s starting on battery, ending on ac
        PowerSourceDebouncer d(BATTERY);
        const PowerSource seq[] = { AC, BATTERY, AC, BATTERY, AC };
        uint32_t lastChange = 0, acceptTime = 0;
        int accepts = runFlaps(d, seq, 5, 1000, 600, lastChange, acceptTime);
        CHECK("odd flaps ending on ac: exactly one accept", accepts == 1);
        CHECK("odd flaps ending on ac: accepted 5 s after the last change",
              acceptTime == lastChange + 5000);
        CHECK("odd flaps ending on ac: normal rate",
              getFrameRateForPowerSource(120, 60, d.current()) == 120);
    }
}

static void testReturnCancels()
{
    PowerSourceDebouncer d(AC);
    d.update(BATTERY, 1000);
    d.update(BATTERY, 4000);
    CHECK("return to the original source is not an accept", !d.update(AC, 5000));
    CHECK("old candidate time is not reused", !d.update(BATTERY, 6100));
    CHECK("later change still waits at 4999 ms", !d.update(BATTERY, 6100 + 4999));
    CHECK("later change accepted after a fresh 5 s", d.update(BATTERY, 6100 + 5000));
}

static void testCandidateSwitch()
{
    PowerSourceDebouncer d(AC);
    d.update(BATTERY, 1000);
    CHECK("switching candidate is not an accept", !d.update(NONE, 4000));
    CHECK("switched candidate waits at 4999 ms", !d.update(NONE, 4000 + 4999));
    CHECK("switched candidate accepted after a fresh 5 s", d.update(NONE, 4000 + 5000));
    CHECK("current is none", d.current() == NONE);
}

static void testWraparound()
{
    PowerSourceDebouncer d(AC);
    const uint32_t start = UINT32_MAX - 2000;
    d.update(BATTERY, start);
    CHECK("wraparound: not accepted at 4999 ms", !d.update(BATTERY, start + 4999));
    CHECK("wraparound: accepted at 5000 ms", d.update(BATTERY, start + 5000));
}

static void testAcToNone()
{
    PowerSourceDebouncer d(AC);
    d.update(NONE, 1000);
    CHECK("ac to none is accepted", d.update(NONE, 6000));
    CHECK("ac to none leaves the rate unchanged",
          getFrameRateForPowerSource(120, 60, d.current()) ==
          getFrameRateForPowerSource(120, 60, AC));
}

int main()
{
    testRateChoice();
    testShortChangeIgnored();
    testExactlyOnceAfterDelay();
    testFlaps();
    testReturnCancels();
    testCandidateSwitch();
    testWraparound();
    testAcToNone();

    std::printf("%d passed, %d failed\n", s_Passed, s_Failed);
    return s_Failed == 0 ? 0 : 1;
}
