// lessons_test.cpp — the lesson moves on small positions with hand-made truth tables.

#include "lessons.h"
#include <cstdio>

static int failures = 0, checks = 0;
static void check(bool ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}

static UciInfo cp(int score, std::vector<std::string> pv) {
    UciInfo info;
    info.depth = 10;
    info.score = score;
    info.pv = pv;
    info.pvMove = pv.empty() ? "" : pv[0];
    return info;
}

int main() {
    Position model;

    // --- The levels and the rhythm ---
    check(lessonFor(1) == Lesson::FreePieces, "level 1: free pieces");
    check(lessonFor(2) == Lesson::LoosePieces, "level 2: loose pieces");
    check(lessonFor(3) == Lesson::Threats, "level 3: threats");
    check(lessonFor(4) == Lesson::None, "level 4: none");
    check(lessonGap(0.0) == 6, "gap at dice 0: 6");
    check(lessonGap(0.999) == 10, "gap at dice 0.999: 10");
    check(lessonGap(1.0) == 10, "gap at dice 1.0: still 10");
    bool inRange = true;
    for (int i = 0; i <= 100; ++i)
        inRange = inRange && lessonGap(i / 100.0) >= 6 && lessonGap(i / 100.0) <= 10;
    check(inRange, "gap always 6..10");

    // --- Level 1: a free piece. Black (the opponent) to move; White's knight on f3 covers d4.
    const Position::Chess free = model.FenInput("4k3/8/2n5/8/8/5N2/8/4K3 b - - 0 1").value();
    {
        std::map<std::string, UciInfo> deep{
            {"c6e7", cp(0, {"f3d4"})},       // the engine's own: safe
            {"c6d4", cp(-300, {"f3d4"})},    // the gift: hangs on d4, the reply takes it
            {"c6b4", cp(-300, {"e1d2"})},    // hangs nothing the reply takes (b4 isn't attacked anyway)
        };
        check(freePieceMove(model, free, deep, "c6e7", 0.5) == std::optional<std::string>("c6d4"), "the knight to d4 is the gift");
        deep["c6d4"] = cp(-40, {"f3d4"});
        check(!freePieceMove(model, free, deep, "c6e7", 0.5), "too small a loss is no gift");
        deep["c6d4"] = cp(-600, {"f3d4"});
        check(!freePieceMove(model, free, deep, "c6e7", 0.5), "too big a loss is no gift");
        deep["c6d4"] = cp(-300, {"e1d2"});
        check(!freePieceMove(model, free, deep, "c6e7", 0.5), "the reply must take it");
        check(!freePieceMove(model, free, deep, "e8d8", 0.5), "no yardstick: nothing");
    }

    // A defended piece attacked by a cheaper one is an exchange, not a gift (2026-11-23): Black's bishop to e5 is
    // attacked by the pawn on d4; with the knight on c6 it's defended, without it free.
    {
        const std::map<std::string, UciInfo> deep{
            {"h8g7", cp(0, {"e1d2"})},
            {"h8e5", cp(-200, {"d4e5"})},
        };
        const Position::Chess defended = model.FenInput("4k2b/8/2n5/8/3P4/8/8/4K3 b - - 0 1").value();
        check(!freePieceMove(model, defended, deep, "h8g7", 0.5), "a defended bishop attacked by a pawn is no gift");
        const Position::Chess alone = model.FenInput("4k2b/8/8/8/3P4/8/8/4K3 b - - 0 1").value();
        check(freePieceMove(model, alone, deep, "h8g7", 0.5) == std::optional<std::string>("h8e5"),
              "the same bishop undefended is a gift");
    }

    // --- Levels 2-3: take a loose piece. Black to move; White's knight on d4 hangs to the knight on c6.
    const Position::Chess loose = model.FenInput("4k3/8/2n5/8/3N4/8/8/4K3 b - - 0 1").value();
    {
        std::map<std::string, UciInfo> deep{
            {"e8e7", cp(0, {"e1d2"})},
            {"c6d4", cp(300, {"e1d2", "e7e6"})},
        };
        check(takeLooseMove(model, loose, deep, "e8e7") == std::optional<std::string>("c6d4"), "takes the loose knight");
        deep["c6d4"] = cp(-100, {"e1d2"});
        check(!takeLooseMove(model, loose, deep, "e8e7"), "not when the truth says it's much worse");
        check(!takeLooseMove(model, free, {{"c6e7", cp(0, {"f3d4"})}}, "c6e7"), "nothing loose: nothing");
    }

    // --- Level 3: a threat. Black to move; Ra7-h7 attacks the undefended knight on h1.
    const Position::Chess threat = model.FenInput("4k3/r7/8/8/8/8/8/4K2N b - - 0 1").value();
    {
        std::map<std::string, UciInfo> deep{
            {"e8d8", cp(10, {"e1f2"})},     // the engine's own
            {"a7h7", cp(0, {"h1g3"})},      // the threat: 0.1 pawns worse
            {"a7b7", cp(5, {"e1f2"})},      // attacks nothing
        };
        check(threatMove(model, threat, deep, "e8d8") == std::optional<std::string>("a7h7"), "the rook attacks the knight");
        deep["a7h7"] = cp(-100, {"h1g3"});
        check(!threatMove(model, threat, deep, "e8d8"), "not when it costs too much");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
