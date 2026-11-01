// pgn_test.cpp — the test gate for PGN. Exit status 0 = all pass (ctest reads it).
#include "pgn.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cmath>
#include <fstream>

int main() {
    int failures = 0;
    auto check = [&failures](const std::string &name, bool ok) {
        std::cout << (ok ? "PASS " : "FAIL ") << "pgn: " << name << "\n";
        if (!ok) ++failures;
    };
    auto has = [](const std::string &text, const std::string &part) { return text.find(part) != std::string::npos; };

    GameRecord r;
    r.start();
    GameInfo &info = r.info();
    info.white = "You";
    info.black = "Buster 7.0.2";
    info.date = "2026.10.25";
    info.time = "11:10";
    info.level = 5;
    info.result = "0-1";
    info.eco = "C00";
    info.opening = "French Defense";
    const std::string pgn = writePgn(r);

    check("starts with the Event tag", pgn.rfind("[Event \"Buster Coach game\"]\n", 0) == 0);
    check("the roster in order: Date before White before Result",
          pgn.find("[Date \"2026.10.25\"]") < pgn.find("[White \"You\"]")
              && pgn.find("[White \"You\"]") < pgn.find("[Result \"0-1\"]"));
    check("Black, Time, TimeControl, Level", has(pgn, "[Black \"Buster 7.0.2\"]") && has(pgn, "[Time \"11:10:00\"]")
                                                 && has(pgn, "[TimeControl \"-\"]") && has(pgn, "[Level \"5\"]"));
    check("the opening", has(pgn, "[ECO \"C00\"]") && has(pgn, "[Opening \"French Defense\"]"));
    check("no FEN from the standard start", !has(pgn, "[FEN "));
    check("ends with a blank line, then the result", has(pgn, "\"]\n\n") && pgn.size() >= 4
                                                         && pgn.substr(pgn.size() - 4) == "0-1\n");

    info.startFen = "8/8/8/8/8/8/k7/K7 w - - 0 1";
    info.white = "Anders \"the\" Player";
    const std::string setUp = writePgn(r);
    check("a set-up position: SetUp and FEN", has(setUp, "[SetUp \"1\"]") && has(setUp, "[FEN \"8/8/8/8/8/8/k7/K7 w - - 0 1\"]"));
    check("a quote in a value is escaped", has(setUp, "[White \"Anders \\\"the\\\" Player\"]"));

    // --- The moves --------------------------------------------------------------------------------------------
    auto move = [](const std::string &uci, const std::string &san, bool human, double after) {
        MoveEntry m;
        m.uci = uci; m.san = san; m.human = human; m.after = after;
        return m;
    };
    GameRecord g;
    g.start();
    g.info().result = "0-1";
    g.info().ending = "You resigned: Buster wins.";
    g.addMove(move("a2a3", "a3", true, -1));            // a try, taken back: not in the main line
    g.addMove(move("a7a6", "a6", false, -1));
    g.takeBack(2);
    g.addMove(move("e2e4", "e4", true, 0.55));
    g.addMove(move("e7e5", "e5", false, 0.52));
    g.addMove(move("g1f3", "Nf3", true, 0.55));
    g.addMove(move("b8c6", "Nc6", false, -1));          // no value: no comment
    g.addMove(move("f1c4", "Bc4", true, -1));
    g.addMove(move("c6d4", "Nd4", false, -1));
    MoveEntry mistake = move("f3e5", "Nxe5", true, 0.33);
    mistake.scored = true;
    mistake.quality = MoveQuality::Bad;
    mistake.bestMove = "f3d4";
    mistake.notes = {"Buster missed it."};
    g.addMove(mistake);
    g.addMove(move("d8g5", "Qg5", false, 0.40));
    std::string moves = writePgn(g);
    for (char &c : moves)
        if (c == '\n')
            c = ' ';                                     // compare without the wrapping
    check("numbers, values, Black's number after a comment and after a side line",
          has(moves, "1. e4 { [%eval 0.35] } ( 1. a3 { Taken back. } 1... a6 ) 1... e5 { [%eval 0.14] } "
                     "2. Nf3 { [%eval 0.35] } 2... Nc6 3. Bc4 Nd4"));
    check("a bad move: ?, its value, Best was, the note",
          has(moves, "4. Nxe5? { [%eval -1.23] Best was Nxd4. Buster missed it. } 4... Qg5 { [%eval -0.70] }"));
    check("the ending, then the result", has(moves, "{ You resigned: Buster wins. } 0-1"));
    check("the try is only in its side line", moves.find(" a3 ") == moves.rfind(" a3 "));
    bool shortLines = true;
    std::string line;
    for (std::istringstream in(writePgn(g)); std::getline(in, line);)
        shortLines = shortLines && line.size() < 80;
    check("every line under 80 characters", shortLines);

    GameRecord slip;                                    // the opponent's mistake on purpose (release_design.md 14)
    slip.start();
    slip.addMove(move("e2e4", "e4", true, -1));
    MoveEntry busters = move("f7f6", "f6", false, -1);
    busters.scored = true;
    busters.quality = MoveQuality::Bad;
    busters.bestMove = "e7e5";
    slip.addMove(busters);
    check("the opponent's mistake: ?, Best was", has(writePgn(slip), "1. e4 f6? { Best was e5. }"));
    const std::vector<PgnGame> slipBack = readPgn(writePgn(slip));
    check("read back: the opponent's mistake, marked", slipBack.size() == 1
          && slipBack[0].record.moves()[1].quality == MoveQuality::Bad && slipBack[0].record.moves()[1].bestMove == "e7e5");

    GameRecord only;                                    // your only good move: "!" (movelist_design.md 3)
    only.start();
    MoveEntry found = move("e2e4", "e4", true, -1);
    found.scored = true;
    found.quality = MoveQuality::Correct;
    found.onlyMove = true;
    only.addMove(found);
    only.addMove(move("e7e5", "e5", false, -1));
    check("the only good move: !", has(writePgn(only), "1. e4! e5"));
    const std::vector<PgnGame> onlyBack = readPgn(writePgn(only));
    check("read back: the only good move", onlyBack.size() == 1 && onlyBack[0].record.moves()[0].onlyMove
          && onlyBack[0].record.moves()[0].quality == MoveQuality::Correct && !onlyBack[0].record.moves()[1].onlyMove);
    const std::vector<PgnGame> marks = readPgn("1. e4!! e5!? 2. Nf3 $1 Nc6?! *");
    const auto &mm = marks.empty() ? std::vector<MoveEntry>{} : marks[0].record.moves();
    check("marks read: !! only, !? neither, $1 only, ?! a mistake", mm.size() == 4
          && mm[0].onlyMove && !mm[1].onlyMove && !mm[1].scored && mm[2].onlyMove
          && mm[3].scored && mm[3].quality == MoveQuality::Bad);

    GameRecord noted;                                   // your own note (movelist_design.md 5)
    noted.start();
    MoveEntry withNote = move("e2e4", "e4", true, 0.55);
    withNote.notes = {"Buster missed it."};
    withNote.yourNote = "my plan: d4 next {or c4}";
    noted.addMove(withNote);
    noted.addMove(move("e7e5", "e5", false, -1));
    std::string notedText = writePgn(noted);
    for (char &c : notedText)
        if (c == '\n')
            c = ' ';
    check("your note: a comment of its own, after the coach's, a } made safe",
          has(notedText, "1. e4 { [%eval 0.35] Buster missed it. } { Note: my plan: d4 next {or c4) } 1... e5"));
    const std::vector<PgnGame> notedBack = readPgn(writePgn(noted));
    check("read back: your note apart from the coach's comments", notedBack.size() == 1
          && notedBack[0].record.moves()[0].yourNote == "my plan: d4 next {or c4)"
          && notedBack[0].record.moves()[0].notes == std::vector<std::string>{"Buster missed it."});

    GameRecord asBlack;                                 // you play Black: the values turn to White's side
    asBlack.start();
    asBlack.addMove(move("e2e4", "e4", false, 0.45));
    check("as Black: your 45 % is White's +0.35", has(writePgn(asBlack), "1. e4 { [%eval 0.35] }"));

    // --- Taken-back tries as side lines -----------------------------------------------------------------------
    GameRecord t;
    t.start();
    for (const auto &[uci, san] : {std::pair{"e2e4", "e4"}, {"e7e5", "e5"}, {"g1f3", "Nf3"}, {"b8c6", "Nc6"},
                                   {"f1c4", "Bc4"}, {"c6d4", "Nd4"}})
        t.addMove(move(uci, san, uci[1] == '2' || uci[1] == '1', -1));   // White's moves are yours here
    MoveEntry tried = move("f3e5", "Nxe5", true, 0.33);
    tried.scored = true;
    tried.quality = MoveQuality::Bad;
    tried.bestMove = "f3d4";
    t.addMove(tried);
    t.addMove(move("d8g5", "Qg5", false, 0.40));
    t.takeBack(2);                                        // the first try at move 4
    t.addMove(move("h2h3", "h3", true, -1));
    t.addMove(move("d4f3", "Nxf3+", false, -1));
    t.takeBack(2);                                        // a second try at the same point
    t.addMove(move("f3d4", "Nxd4", true, -1));
    t.addMove(move("e5d4", "exd4", false, -1));
    t.addMove(move("c2c3", "c3", true, -1));
    t.takeBack(1);                                        // a try at the very end
    std::string lines = writePgn(t);
    for (char &c : lines)
        if (c == '\n')
            c = ' ';
    check("a try as a side line: Taken back, Best was, the reply",
          has(lines, "4. Nxd4 ( 4. Nxe5? { [%eval -1.23] Taken back. Best was Nxd4. } 4... Qg5 { [%eval -0.70] } ) "));
    check("two tries at one point: both, in order, then the game goes on",
          has(lines, "4... Qg5 { [%eval -0.70] } ) ( 4. h3 { Taken back. } 4... Nxf3+ ) 4... exd4"));
    check("a try at the very end: a comment", has(lines, "4... exd4 { Taken back: c3 } *"));

    // --- Reading: tokens, tags, games ---------------------------------------------------------------------------
    const std::string two =
        "% an escape line: skipped\n"
        "[Event \"Test\"]\n[White \"You\"]\n[Black \"Buster 7.0.2\"]\n[Date \"2026.10.25\"]\n[Time \"11:47:00\"]\n"
        "[Result \"0-1\"]\n[TimeControl \"300+3\"]\n[Level \"5\"]\n[ECO \"C21\"]\n[Opening \"Danish Gambit\"]\n"
        "\n1. e4 e5 ; a comment to the line's end\n2. d4 { a { comment } exd4 0-1\n\n"
        "[White \"Anders \\\"the\\\" Player\"]\n[SetUp \"1\"]\n[FEN \"8/8/8/8/8/8/k7/K7 w - - 0 1\"]\n\n1. Kb1 *\n"
        "1. d4 d5 1/2-1/2\n";                                   // a third game without tags
    const std::vector<PgnGame> read = readPgn(two);
    check("three games: two with tags, one without", read.size() == 3);
    if (read.size() == 3) {
        const GameInfo &a = read[0].record.info();
        check("the tags into the info", a.white == "You" && a.black == "Buster 7.0.2" && a.date == "2026.10.25"
                                            && a.time == "11:47" && a.result == "0-1" && a.timeControl == "300+3"
                                            && a.level == 5 && a.eco == "C21" && a.opening == "Danish Gambit");
        const GameInfo &b = read[1].record.info();
        check("an escaped quote, a FEN, the result from the moves",
              b.white == "Anders \"the\" Player" && b.startFen == "8/8/8/8/8/8/k7/K7 w - - 0 1" && b.result == "*");
        check("a game without tags: its result", read[2].record.info().result == "1/2-1/2");
    }

       // --- Reading: the main line -----------------------------------------------------------------------------------
    const std::vector<PgnGame> back = readPgn(writePgn(g));   // the 2b game: written, then read back
    check("read back: one game, complete", back.size() == 1 && back[0].problem.empty());
    if (back.size() == 1) {
        const GameRecord &rb = back[0].record;
        std::vector<const MoveEntry *> main;
        for (const MoveEntry &m : rb.moves())
            if (!m.takenBack)
                main.push_back(&m);
        check("read back: the 8 moves of the game", main.size() == 8 && main[0]->san == "e4" && main[7]->san == "Qg5");
        if (main.size() == 8) {
            const MoveEntry &bad = *main[6];
            check("read back: the bad move, its best move, its note",
                  bad.quality == MoveQuality::Bad && bad.bestMove == "f3d4" && bad.notes.size() == 1
                      && bad.notes[0] == "Buster missed it.");
            check("read back: the values (to 0.005)", std::abs(bad.after - 0.33) < 0.005
                                                          && std::abs(main[7]->after - 0.40) < 0.005
                                                          && main[3]->after < 0);
            check("read back: your moves are yours", main[0]->human && !main[1]->human);
        }
        check("read back: the ending", rb.info().ending == "You resigned: Buster wins.");
    }
    const std::vector<PgnGame> broken = readPgn("1.e4 e5 2. Ke3 Nf6 *");
    check("an illegal move stops the game there, with the reason",
          broken.size() == 1 && broken[0].record.liveCount() == 2 && broken[0].problem == "move 2: no legal Ke3");
    const std::vector<PgnGame> lichess = readPgn("1. e4 { [%eval 0.17] [%clk 0:03:00] } 1... f5?! $6 2. exf5 *");
    check("lichess's form: a clock dropped, ?! a mark, no note",
          lichess.size() == 1 && lichess[0].record.liveCount() == 3 && lichess[0].record.moves()[1].quality == MoveQuality::Bad
              && lichess[0].record.moves()[0].notes.empty());

    // --- The round trip: write → read → write gives the same text ----------------------------------------------
    for (const GameRecord *original : {&g, &t}) {
        const std::string first = writePgn(*original);
        const std::vector<PgnGame> again = readPgn(first);
        check("round trip: the same text again", again.size() == 1 && writePgn(again[0].record) == first);
    }
    const std::vector<PgnGame> analysis = readPgn("1. e4 e5 ( 1... c5 2. Nf3 ( 2. c3 ) ) 2. Nf3 *");
    check("someone else's side lines: skipped", analysis.size() == 1 && analysis[0].record.moves().size() == 3);
    
    // --- Real files: lichess games (CC0), and games Buster Coach saved --------------------------------------------
    auto fileText = [](const std::string &name) {
        std::ifstream file(std::string(PGN_TESTDATA) + "/" + name);
        std::stringstream text;
        text << file.rdbuf();
        return text.str();
    };
    const std::vector<PgnGame> real = readPgn(fileText("lichess.pgn"));
    bool complete = real.size() == 4;
    for (const PgnGame &game : real)
        complete = complete && game.problem.empty();
    check("lichess: four games, each read to its end", complete);
    if (real.size() == 4) {
        check("lichess: the first game's 89 moves, each with its value",
              real[0].record.liveCount() == 89 && real[0].record.curve().size() == 89 && real[0].record.curve()[0] >= 0);
        check("lichess: a draw of 162 plies", real[3].record.liveCount() == 162 && real[3].record.info().result == "1/2-1/2");
    }
    for (const std::string name : {"own1.pgn", "own2.pgn"}) {
        const std::string text = fileText(name);
        const std::vector<PgnGame> own = readPgn(text);
        check("our own saved game " + name + ": read, then written, is the file again",
              !text.empty() && own.size() == 1 && writePgn(own[0].record) == text);
    }

    return failures == 0 ? 0 : 1;
}
