# tools/helppics — the Help's pictures

Claude's tools (not part of the app, run by hand) for the pictures in `app/help/images/` and the coach sentences on
them. They build against the app's own code, so every picture is the real widget, and every sentence is one the
real coach writes.

    cmake -S tools/helppics -B tools/helppics/build && cmake --build tools/helppics/build

| Program | What it does |
|---|---|
| `helppics <folder> [dark]` | Cuts every picture into `<folder>`: an offscreen `BoardWidget`, `EvalBar`, `WinGraph`, `ConfidenceDot` and coach cards, put into each state the way the app does it (mouse moves and clicks, the setters). Twice the size (`QT_SCALE_FACTOR=2`); the help shows them at half width, so they stay sharp when zoomed. Run with `QT_QPA_PLATFORM=offscreen`. |
| `coachtext <fen> [uci ...]` | The real `Coach` on a position (all five passes, the built-in Buster in `app/build/buster`): each move's verdict, `whyText`, `lineText`, and the threat's words. Where the cards' sentences come from. |
| `probe.py <fen> [depth] [square]` | Quick: every move scored by Buster at one depth, red or not by the app's rule. For finding a position that shows something (a piece whose moves are all red, …). Needs `build/moves`. |
| `bookinfo "<opening name>" [uci ...]` | After the moves: every book move with its share (rounded as the app does), the name after it, and whether it stays inside the opening (`Game::staysInside`'s rule). The openings page's numbers. |
| `page <page.html> <out.png> [height]` | A Help page as QTextBrowser lays it out, saved as a picture: check a page without the app. |

## A new picture

1. Find the position: `probe.py` to look, `coachtext` for the exact words.
2. Add a block to `helppics.cpp` (a board from a FEN, the state, `cut(...)` for a board crop, `stack(...)` for a board
   with cards under it, `cards(...)` for cards alone).
3. `QT_QPA_PLATFORM=offscreen tools/helppics/build/helppics /tmp/pics`, look, then copy the picture to
   `app/help/images/`.
4. In `help.md`: `<img src="images/name.png" width="W">`, W = half the picture's width (or less). Pictures in one row
   should be the same height. Then `python3 tools/help_pages.py` and build the app.

The positions used so far:
- the signs page: the start position (the boxes, the marks, the books, the hint, the pieces, violet); a knight on c4
  and a queen on e3 (the threat); e2–e4; 3.Bxf7+ (check); Fool's mate; a knight on e5 under a pawn (hanging);
- the asking page: 1.e4 e5 2.Nf3 d6 3.Nc3 Bg4 (the pin), and the Two Knights after 5...Na5;
- the move list page: Legal's mate (1.e4 e5 2.Nf3 d6 3.Bc4 Bg4 4.Nc3 h6 5.Nxe5 Bxd1 6.Bxf7+ Ke7 7.Nd5#): the
  table built as panel.cpp builds it, the marks as the coach gives them (5...Bxd1 red, 6.Bxf7+ the only move);
- the openings page: the Najdorf (to 6.Be3 e5; the strip as setupOpenings builds it), the real PracticePanel on the
  real names and book, and you as Black after 5.Nc3 (a6 65 % inside, g6 23 % into the Dragon: bookinfo).
