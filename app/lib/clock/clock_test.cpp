// clock_test.cpp — the test gate for the chess clock. Exit status 0 = all pass (ctest reads it).

#include "chessclock.h"
#include <iostream>
#include <string>

int main() {
    int failures = 0;

    // --- formatClock -----------------------------------------------------------------------------
    struct FormatCase { long long ms; std::string expected; };
    const FormatCase formats[] = {
        {300000, "5:00"},  {59999, "0:59"},    {60000, "1:00"},      {10000, "0:10"},
        {9999, "9.9"},     {8450, "8.4"},      {100, "0.1"},         {99, "0.0"},
        {0, "0.0"},        {-500, "0.0"},      {3599999, "59:59"},   {3600000, "1:00:00"},
        {5400000, "1:30:00"},
    };
    for (const FormatCase &c : formats) {
        const std::string got = formatClock(c.ms);
        const bool ok = (got == c.expected);
        std::cout << (ok ? "PASS " : "FAIL ") << "format " << c.ms << " ms: \"" << got
                  << "\" (expected \"" << c.expected << "\")\n";
        if (!ok) ++failures;
    }

    // --- ChessClock ------------------------------------------------------------------------------
    // The clock. Times in ms; "now" is passed in, so the test travels in time without waiting.
    auto check = [&failures](const std::string &name, bool ok) {
        std::cout << (ok ? "PASS " : "FAIL ") << "clock: " << name << "\n";
        if (!ok) ++failures;
    };
    ChessClock c(300000, 300000, 2000, 2000);                       // 5 min + 2 s for both
    check("not started: full time", c.remaining(true, 0) == 300000);
    c.start(true, 1000);                                            // White's clock runs from t = 1 s
    check("White runs", c.isRunning(true) && !c.isRunning(false));
    check("3.2 s later", c.remaining(true, 4200) == 296800);
    check("Black untouched", c.remaining(false, 4200) == 300000);
    c.moveMade(4200);                                               // White moves after 3.2 s
    check("White + increment, stopped", c.remaining(true, 9000) == 298800);
    check("Black runs", c.isRunning(false) && !c.isRunning(true));
    check("Black after 1.8 s", c.remaining(false, 6000) == 298200);
    c.stop(6000);
    check("stopped: nothing runs", !c.isRunning(true) && !c.isRunning(false));
    check("stopped: time kept, no increment", c.remaining(false, 99999) == 298200);

    ChessClock odds(60000, 5000, 0, 0);                             // time odds: 1 min v 5 s, no increment
    odds.start(false, 0);
    check("odds: White has its own time", odds.remaining(true, 0) == 60000);
    check("Black flags at 5 s", odds.remaining(false, 5000) <= 0);
    check("Black not yet at 4.9 s", odds.remaining(false, 4900) > 0);

    // --- addTime: a bonus (Tournament: an illegal move gives the opponent a minute) ----------------------------------
    ChessClock bonus(300000, 300000, 0, 0);
    bonus.start(true, 0);                                           // White's clock runs
    bonus.addTime(false, 60000);                                    // to the waiting side
    check("bonus to the waiting side", bonus.remaining(false, 5000) == 360000);
    bonus.addTime(true, 60000);                                     // to the running side: it keeps counting down
    check("bonus to the running side", bonus.remaining(true, 5000) == 355000);


    // --- clockAlert ------------------------------------------------------------------------------
    // Low time: amber below 20% of the start (capped at 2 min), red below 10 s. "Below" = strictly less.
    struct AlertCase { std::string name; long long left; long long start; ClockAlert expected; };
    const AlertCase alerts[] = {
        {"5 min game, 4:00 left",          240000,  300000, ClockAlert::None},
        {"5 min game, 0:59 left (20%)",     59000,  300000, ClockAlert::Low},
        {"5 min game, exactly 1:00 left",   60000,  300000, ClockAlert::None},
        {"90 min game, 2:01 left",         121000, 5400000, ClockAlert::None},
        {"90 min game, 1:59 left (cap)",   119000, 5400000, ClockAlert::Low},
        {"1 min game, 0:11 left",           11000,   60000, ClockAlert::Low},
        {"exactly 10 s: still amber",       10000,  300000, ClockAlert::Low},
        {"9.9 s: red",                       9900,  300000, ClockAlert::Critical},
        {"out of time: red",                 -100,  300000, ClockAlert::Critical},
    };
    for (const AlertCase &c : alerts)
        check("alert: " + c.name, clockAlert(c.left, c.start) == c.expected);

    return failures == 0 ? 0 : 1;
}
