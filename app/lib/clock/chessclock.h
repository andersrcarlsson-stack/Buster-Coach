// chessclock.h — chess clocks: plain C++ (no Qt), like the board model, so it can sit behind a test.
// The time always comes IN as a number (ms), never read from the system: a test can say "3.2 s later".

#pragma once
#include <string>

// A clock reading for the display: "m:ss", "h:mm:ss" from an hour, "s.t" (tenths) under 10 s.
// Rounded DOWN: the clock never shows time you don't have. Zero or less: "0.0".
std::string formatClock(long long ms);

// How urgent a clock looks. Low = amber: below 20% of the start time, but at most 2 minutes before the end.
// Critical = red: below 10 s (also when out of time).
enum class ClockAlert { None, Low, Critical };
ClockAlert clockAlert(long long remainingMs, long long startMs);

// Two clocks, White and Black, each with its own time and increment (time odds come free).
// "now" is any steady time in ms, passed in by the caller (the GUI: a QElapsedTimer; a test: made up).
class ChessClock {
public:
    ChessClock(long long whiteMs, long long blackMs, long long whiteIncMs, long long blackIncMs);
    void start(bool white, long long now);                  // this side's clock starts running
    void moveMade(long long now);                           // the running side moved: + its increment, the other runs
    void stop(long long now);                               // nothing runs (game over); the times are kept
    void addTime(bool white, long long ms);                // a bonus: this side's time grows (running or not)
    long long remaining(bool white, long long now) const;   // time left; a running clock counts down to now
    bool isRunning(bool white) const;

private:
    static int side(bool white) { return white ? 0 : 1; }   // index: [0] White, [1] Black
    long long m_left[2] {};    // time left when this clock last stopped (or at the start)
    long long m_inc[2] {};     // increment per move
    int m_running = -1;        // the side whose clock runs: 0, 1, or -1 = none
    long long m_since = 0;     // when the running clock started
};
