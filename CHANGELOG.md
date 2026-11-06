# Changelog

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
