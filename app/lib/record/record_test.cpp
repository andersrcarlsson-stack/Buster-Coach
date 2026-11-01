// record_test.cpp — the test gate for the game record. Exit status 0 = all pass (ctest reads it).
#include "gamerecord.h"
#include <iostream>
#include <string>

int main() {
    int failures = 0;
    auto check = [&failures](const std::string &name, bool ok) {
        std::cout << (ok ? "PASS " : "FAIL ") << "record: " << name << "\n";
        if (!ok) ++failures;
    };
    auto human = [](const std::string &uci, MoveQuality q) {   // a scored human move
        MoveEntry m;
        m.uci = uci; m.human = true; m.scored = true; m.quality = q;
        return m;
    };
    auto buster = [](const std::string &uci) {
        MoveEntry m;
        m.uci = uci;
        return m;
    };

    GameRecord r;
    r.start();
    check("empty at the start", r.moves().empty() && r.liveCount() == 0);

    r.addMove(human("e2e4", MoveQuality::Correct));
    r.addMove(buster("e7e5"));
    r.addMove(human("d1h5", MoveQuality::Bad));        // a mistake ...
    r.addMove(buster("g8f6"));
    check("four moves played", r.moves().size() == 4 && r.liveCount() == 4);

    r.takeBack(2);                                     // ... taken back (your move and the reply)
    check("taken back: still recorded", r.moves().size() == 4);
    check("taken back: not in the game", r.liveCount() == 2);
    check("taken back: the right two marked", r.moves().size() == 4             // guard: never index past the end
                                               && r.moves()[2].takenBack && r.moves()[3].takenBack
                                               && !r.moves()[1].takenBack);

    r.addMove(human("g1f3", MoveQuality::Correct));     // the game goes on
    check("after the takeback: appended", r.moves().size() == 5 && r.liveCount() == 3);

    r.addMove(buster("b8c6"));
    r.takeBack(2);                                     // must skip the moves taken back before
    check("second takeback: the newest two", r.moves().size() == 6
                                              && r.moves()[4].takenBack && r.moves()[5].takenBack
                                              && !r.moves()[1].takenBack && r.liveCount() == 2);

    check("mistakes: the bad move, taken back too", r.mistakes() == std::vector<int>{2});

    r.start();
    check("a new game: empty again", r.moves().empty());


    // --- Notes: they go to your newest move still in the game ----------------------------------------
    GameRecord n;
    n.start();
    n.addMove(human("f2f3", MoveQuality::Playable));
    n.addMove(buster("e7e5"));
    n.addMove(human("g2g4", MoveQuality::Bad));
    n.addMove(buster("a7a6"));                         // Buster missed ...Qh4#
    n.noteLastHumanMove("Buster missed 2... Qh4#");
    check("note: on your last move", n.moves().size() == 4 && n.moves()[2].notes.size() == 1
                                      && n.moves()[2].notes[0] == "Buster missed 2... Qh4#");
    check("note: not on Buster's move", n.moves().size() == 4 && n.moves()[3].notes.empty());
    n.takeBack(2);                                     // g4 and a6 taken back
    n.noteLastHumanMove("a later note");               // must skip g4 (taken back): goes to f3
    check("note: skips taken-back moves", n.moves().size() == 4 && n.moves()[0].notes.size() == 1
                                           && n.moves()[2].notes.size() == 1);
    GameRecord e;
    e.start();
    e.noteLastHumanMove("nothing to note on");
    check("note: no move yet: ignored", e.moves().empty());

    // --- The curve: one value per move in the game (the win-chance graph, graph_design.md) ------------------------
    GameRecord g;
    g.start();
    MoveEntry e4 = human("e2e4", MoveQuality::Correct);
    e4.after = 0.55;                                   // your move: its value comes with it
    g.addMove(e4);
    g.addMove(buster("e7e5"));                         // the opponent's: its value comes a moment later
    check("curve: the opponent's value unknown at first", g.curve() == std::vector<double>{0.55, -1.0});
    g.setLastAfter(0.52);
    check("curve: filled in later", g.curve() == std::vector<double>{0.55, 0.52});
    MoveEntry g4 = human("g2g4", MoveQuality::Bad);
    g4.after = 0.30;
    g.addMove(g4);
    g.addMove(buster("d8h4"));
    g.setLastAfter(0.0);
    g.takeBack(2);
    check("curve: taken-back moves drop out", g.curve() == std::vector<double>{0.55, 0.52});
    g.setLastAfter(0.50);                              // must skip the taken-back moves: goes to e5
    check("curve: the setter skips taken-back moves", g.curve() == std::vector<double>{0.55, 0.50});
    GameRecord empty;
    empty.start();
    empty.setLastAfter(0.5);
    check("curve: no move yet: ignored", empty.curve().empty());

    // --- The game's data ------------------------------------------------------------------------------------------
    GameRecord g2;
    g2.start();
    check("info: a new game is unfinished and untimed", g2.info().result == "*" && g2.info().timeControl == "-");
    g2.info().white = "You";
    g2.info().result = "1-0";
    g2.info().opening = "Italian Game";
    const GameRecord &readOnly = g2;
    check("info: kept, and readable through a const record",
          readOnly.info().white == "You" && readOnly.info().result == "1-0" && readOnly.info().opening == "Italian Game");
    g2.start();
    check("info: a new game forgets the old one", g2.info().white.empty() && g2.info().result == "*");
    
    // --- Each move's ply (where it was played): the variations in PGN need it --------------------------------------
    GameRecord p;
    p.start();
    p.addMove(buster("e2e4"));
    p.addMove(buster("e7e5"));                            // plies 0, 1
    p.addMove(buster("g1f3"));
    p.addMove(buster("b8c6"));                            // plies 2, 3: then taken back
    p.takeBack(2);
    p.addMove(buster("f1c4"));
    p.addMove(buster("f8c5"));                            // plies 2, 3 again: a second try at the same point
    check("ply: counted from the game's start", p.moves()[0].ply == 0 && p.moves()[1].ply == 1);
    check("ply: a taken-back try keeps its place", p.moves()[2].ply == 2 && p.moves()[3].ply == 3);
    check("ply: the next try starts at the same place", p.moves()[4].ply == 2 && p.moves()[5].ply == 3);

    // --- The moves of the game as it stands --------------------------------------------------------------------
    const std::vector<const MoveEntry *> livePlies = p.liveMoves();   // p: e4 e5 (Nf3 Nc6 taken back) Bc4 Bc5
    check("liveMoves: the game as it stands, in order",
          livePlies.size() == 4 && livePlies[0]->uci == "e2e4" && livePlies[2]->uci == "f1c4" && livePlies[3]->uci == "f8c5");
    GameRecord none;
    none.start();
    check("liveMoves: none in a new game", none.liveMoves().empty());

    GameRecord noted;                                   // your note goes on the move as the game stands
    noted.start();
    noted.addMove(human("e2e4", MoveQuality::Correct));
    noted.addMove(buster("e7e5"));
    noted.addMove(human("a2a3", MoveQuality::Playable));   // a try, taken back
    noted.addMove(buster("a7a6"));
    noted.takeBack(2);
    noted.addMove(human("g1f3", MoveQuality::Correct));
    noted.setYourNote(2, "develop first");
    noted.setYourNote(9, "nothing there");                 // beyond the game: ignored
    check("setYourNote: on the live move, not the taken-back try",
          noted.liveMoves()[2]->yourNote == "develop first" && noted.moves()[2].yourNote.empty());

    return failures == 0 ? 0 : 1;
}
