// chessclock.cpp — the clock's display rules and the two-sided clock itself.
//
// Sections:
//   1. The display: formatting, low-time alerts
//   2. The clock: start, a move made, stop, time remaining

#include "chessclock.h"
#include <format>

// =================================================================================================
// 1. THE DISPLAY: formatting, low-time alerts
// =================================================================================================
std::string formatClock(long long ms) {
    if (ms <= 0)
        return "0.0";                                       // out of time
    if (ms < 10000)                                         // under 10 s: seconds and tenths, "8.4"
        return std::format("{}.{}", ms / 1000, ms / 100 % 10);
    const long long s = ms / 1000;                          // whole seconds: integer division rounds down
    const long long h = s / 3600, m = s / 60 % 60, sec = s % 60;
    if (h > 0)
        return std::format("{}:{:02}:{:02}", h, m, sec);    // "1:30:00"
    return std::format("{}:{:02}", m, sec);                 // "5:00", "0:59"
}

ClockAlert clockAlert(long long remainingMs, long long startMs) {
    if (remainingMs < 10000)
        return ClockAlert::Critical;                          // under 10 s (or out of time): red
    if (remainingMs < std::min(startMs / 5, 120000LL))
        return ClockAlert::Low;                               // under 20% of the start, at most 2 min: amber
    return ClockAlert::None;
}

// =================================================================================================
// 2. THE CLOCK: start, a move made, stop, time remaining
// =================================================================================================
ChessClock::ChessClock(long long whiteMs, long long blackMs, long long whiteIncMs, long long blackIncMs)
    : m_left{whiteMs, blackMs}, m_inc{whiteIncMs, blackIncMs} {}

void ChessClock::start(bool white, long long now) {
    m_running = side(white);
    m_since = now;
}

void ChessClock::moveMade(long long now) {
    if (m_running == -1)
        return;                                       // no clock runs: nothing to switch
    const int mover = m_running;
    stop(now);                                        // book the time used
    m_left[mover] += m_inc[mover];                    // the increment: after the move
    start(mover == 1, now);                           // the other side: 0 (White) moved -> Black (false) runs
}

void ChessClock::stop(long long now) {
    if (m_running != -1)
        m_left[m_running] -= now - m_since;           // book the time used on the running clock
    m_running = -1;
}

// A bonus is booked on the time left. A running clock counts down from m_left, so it simply keeps running from more.
void ChessClock::addTime(bool white, long long ms) {
    m_left[side(white)] += ms;
}

long long ChessClock::remaining(bool white, long long now) const {
    const int s = side(white);
    return (s == m_running) ? m_left[s] - (now - m_since)   // running: count down to now
                            : m_left[s];
}

bool ChessClock::isRunning(bool white) const {
    return m_running == side(white);
}
