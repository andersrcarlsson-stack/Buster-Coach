# Changelog

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
