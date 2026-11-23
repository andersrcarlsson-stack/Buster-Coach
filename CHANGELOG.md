# Changelog

## 1.0.7 — levels 1–3 teach

- **Taking back is "wins back", also when you're ahead.** After a queen trade begun by Buster (…Qxb3), the card for
  axb3 said "takes a queen" or "wins the queen", because you were a piece up before. Now: "axb3: wins back the queen."

- **No false threat when you have a winning move.** With a crushing move of your own (Nxe7, +16), the threat card
  said "Buster threatens …Kd8. Ignoring it costs you.": passing would only have thrown your chance away. A threat with
  nothing to name now counts only if passing really changes the game.

- **Levels 1–3 are lessons.** They used to be one level in three disguises (each measured about 1400–1500). Now each
  teaches a beginner one habit: **1 · free pieces** (the opponent now and then leaves a piece unprotected: look for
  what you can take), **2 · your loose pieces** (it never misses a piece you leave hanging: check your pieces first),
  **3 · threats** (it makes simple, direct attacks: ask what it wants). A card names the habit before your move and
  says how it went after it ("You took the free knight."). The lessons build on each other: level 2 also points out
  free pieces, level 3 all three. Coaching is Full during the lessons, with the warnings waiting for you; your own
  coaching level comes back at level 4. Coaching → Lessons turns them off: levels 1–3 are then plain easy games of
  about 800, 1000 and 1200.

## 1.0.6 — stronger top levels, truer cards

- **A capture that wins at once says so.** When your move takes material and an even trade follows straight away, the
  card dated the gain at the trade's end ("dxc3: wins back the bishop on move 2", "Qxa5: can win a pawn on move 73").
  Now it's dated now: "Qxa5: wins a pawn on move 72."
- **No warning box where Buster's piece merely escapes.** When Buster's own piece is hanging, the threat is only that it
  gets away ("take it, or it escapes"). The board still marks the piece, but no longer puts a violet box on its escape
  square, which looked like a danger to you.
- **A pawn the coach can't promise is a pawn you "can win".** When the coach's line wins a single pawn a few moves
  later, the card now says "can win a pawn on move 16" instead of "wins a pawn": a check against a stronger engine
  confirmed only about half of these. A pawn your move takes itself, winning material back, and bigger gains still
  say "wins". Prompted by the beta tester's "I don't gain a pawn".
- **Strong levels drop fewer big chunks.** The opponent's safety net, which keeps a slip from losing too much by a
  deeper check, is tighter from level 6 up (level 9: 20 points of win chance instead of 30, level 10: 15). The beta
  tester, a 2200+ player, saw level 9 drop 30 points to lines a player of its strength would see. The opponent still
  makes as many mistakes as before; only the big ones its own deeper look can see are gone. A replay of 1,100 saved
  positions puts the strength change at about +10 to +25 Elo.
- **A piece that's lost anyway isn't blamed on your move.** When one of your pieces was already hanging and couldn't
  be saved, nearly every move's card said "gives the knight for a pawn: the coach judges the play worth it". Now the
  card talks about the move itself instead.
- **The win-chance graph and the eval bar agree.** While you thought, the bar followed the coach's deeper searches but
  the graph's last point stayed at its first look, so in sharp positions the two showed different numbers. The graph's
  last point now follows the bar until you move. Seen by the beta tester.
- **"Loses" only on a mistake.** A move the coach rates fine, whose line gives material a few moves later, said
  "loses the rook and a pawn for a bishop on move 18" next to a green or amber box. It now says "gives …: the coach
  judges the play worth it", as for any sacrifice. Found by a check of 15,000 coach cards against rules a card must
  never break.
- **Red means only "a mistake" now.** The threat card, the hanging-piece card, the "!" on a hanging piece and the
  flash of the opponent's piece and target were red, the same red as a bad move, so a red threat card above a green
  box read as "my move is wrong". They are violet now, the colour of "watch out" (as an illegal attempt already was);
  red is only ever a mistake of yours. Found by the beta tester.
- **Castling away from a threat is called that.** A rook threatened on h1, saved by O-O, got "defends the rook
  against …Bxh1"; now "O-O: moves the rook away from …Bxh1, the threat."

## 1.0.5 — winning back is called that

- **Taking back material is called that.** After 4...cxd4, the card for 5.Nxd4 said "wins a pawn on move 5", though
  it only takes the pawn back and material is level again. Now: "Nxd4: wins back a pawn, and takes the attacker
  (…dxc3), the threat." The same for a gambit pawn regained later. Probably the beta tester's "the coach says I gain
  a pawn, but I don't".

## 1.0.4 — fixes from the beta tester

- **The side panel fits short screens.** On a laptop with Windows' display scaling at 150 % (a 1920 × 1080 screen),
  the window has only about 660 points of height, and the panel needed more: the opponent's strip was cut (half its
  king, the captured pieces) and the coach's cards were squeezed. Now everything fits in a window 650 high: the
  margins are tighter, the move list and the graph give way first on a short screen (the graph shrinks from 100 to
  50), and the captured pieces have a size limit. When the cards still don't fit, the least important one (the
  strongest line first) steps aside whole, instead of every card being cut. Found by the beta tester.
- **The navigation buttons beside the move list are gone** (they held the panel's height on short screens). Step
  through a game with ← → Home, End or Esc (back to the game), or click a move in the list or a point on the graph.
- **Sharp pieces with display scaling.** At 125 % or 150 % the kings in the player strips and the board editor's
  pieces were blurry (drawn small and enlarged); they are now drawn at the screen's real resolution. And the help
  window never opens taller than the screen.
- **Save game… saves where Open game looks.** The dialog now suggests the game's own file in this month's folder,
  named like the automatic saves; saved there, it is the same file the automatic save updates at the game's end (one
  game, one file). Before, it started in the top folder, which Open game doesn't list.
- **Dark title bars on every window (Linux).** In the dark appearance, the help window, About, the file dialogs and
  message boxes now get the dark title bar too, not only the main window; switching the appearance updates the open
  windows.
- **A saved game names the level it was played at.** Changing the strength after the game had started (even before
  the first move) changed how the opponent played, but the saved game kept the level from the start. Now the saved
  game follows the slider (after a change mid-game: the last level).
- **A coach card no longer claims a piece is safe when it isn't.** "Keeps the knight safe from …Bxf3" was shown for
  a move that only postponed the capture (a check first, …Bxf3 a move later). The card now says so only when the
  punishing move never comes in the line. Found by the beta tester.
- **The coach connects its cards on a threatened piece.** When a piece was attacked twice and defended once (6.Be3
  in the Sicilian, against …Nxd4 or …Bxd4), the "why" card spoke of a different capture than the threat card and
  didn't say the move answers it. Now both cards name the same move: "Be3: defends the knight against …Bxd4, the
  threat."

## 1.0.3 — an opponent that errs like a person

- **The opponent's mistakes are rebuilt.** Each level now has a *horizon*: how far ahead it looks when it judges a
  move. Now and then it plays the move that looks best within that horizon instead of the engine's own choice. Usually
  that move is fine; when the position hides something just beyond the horizon, it walks into it, the way a player of
  that strength would. A beginner may miss your direct reply; a strong club player misses a combination three moves
  deep. A safety net keeps every level from the wild, out-of-character blunders that testers noticed in 1.0.
- **Fewer mistakes right after the opening.** People play their first moves out of the opening carefully and make
  most of their mistakes in the middlegame. The opponent now does the same: fewer slips just after the book (half as
  many from level 4 up), back to normal by move 25. In testing, the strong levels used to collapse early far too often; that is fixed.
- **The levels were measured by results.** Every level played hundreds of games against its neighbours and against
  Stockfish at a known strength, and was tuned until it won about as often as its rating says. Levels 4 to 10 are now
  close to their ratings. Levels 1 to 3 still play closer together, and stronger, than their labels (more like
  1400–1500); making the beginner levels weaker is the next step. The opponent now thinks a little longer per move
  (under a second at most levels, about 2 seconds at level 9 and 6 at level 10).
- **The coach's "why" is more accurate on captures.** A capture that wins material and then leads into an even trade
  now says what it wins ("Bxd6: wins a knight and two pawns for a bishop on move 16."); before, it could be called
  "a quiet move". And a capture is never called quiet any more: at the least it says what it takes.
- **Help and HOW_IT_WORKS.md** describe the new opponent; HOW_IT_WORKS.md is rewritten to be easier to read.

## 1.0.2 — the AppImage remembers its engine

- **The Linux AppImage no longer opens the engine page on every start.** An AppImage unpacks itself to a new folder
  each time it runs, and the app saved the built-in Buster by that folder's path, which was gone by the next start.
  The built-in Buster is now remembered as itself, wherever the app runs from. Settings left by 1.0.0 and 1.0.1 are
  read correctly, and their extra copies of Buster in the engine list disappear. Windows was not affected.

## 1.0.1 — the coach never waits on a stuck search

- **The coach keeps going when an engine's search gets stuck.** In some positions, typically a won endgame where the
  losing side has a long run of checks, a single engine search could run for minutes: the confidence dot stayed
  yellow, the hint stayed greyed out, and the opponent's move could be delayed. Now every coach search has a time
  limit (it keeps the deepest result reached), the opponent no longer waits for coach work it doesn't need once the
  game is decided, and the hint comes after a few seconds even when the deepest search is slow.
- **Buster 7.0.4 built in:** mate scores count down steadily (no more "mate in 6" and back between depths). The
  same strength as 7.0.3, so the levels and their ratings are unchanged.
- **HOW_IT_WORKS.md:** how the coach and the opponent work, and the story behind Buster Coach.

## 1.0.0 — the first release

A chess opponent with a coach beside you: the coloured box and its reasons, hints, the threat, hanging pieces, the
strongest line and its play-through; four coaching levels (Tournament, Play, Guided, Full coaching); eleven opponent
strengths (≈ 1000 to ≈ 2400, and full strength) whose mistakes are traps beyond their horizon, calibrated against
lichess error rates; clocks and the game buttons; the move list with marks, comments and
your notes; the win-chance graph; PGN saving and opening; opening names, a book and opening practice; a board editor;
light and dark appearance; the built-in help with pictures. Linux AppImage and Windows zip. Buster 7.0.3
built in.
