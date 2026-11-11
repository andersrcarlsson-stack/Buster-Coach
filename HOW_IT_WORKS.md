# How Buster Coach works

This page explains how Buster Coach *thinks*: how the coach judges your moves, how the opponent decides when to make a
mistake, and how its strength was measured. You don't need to know anything about chess engines to read it. The
built-in help (F1 in the app) explains how to *use* the program; this page explains what goes on behind it.

**Contents:** [1. The idea](#1-the-idea) · [2. Three engines behind the board](#2-three-engines-behind-the-board) ·
[3. How the coach judges a move](#3-how-the-coach-judges-a-move) · [4. Why, in words](#4-why-in-words) ·
[5. The threat](#5-the-threat) · [6. The coaching levels](#6-the-coaching-levels) ·
[7. An opponent that errs like a person](#7-an-opponent-that-errs-like-a-person) ·
[8. How the levels were measured](#8-how-the-levels-were-measured) · [9. How it got here](#9-how-it-got-here) ·
[Words used](#words-used)

---

## 1. The idea

Buster Coach is for **playing and training** chess. You play a real game against a computer opponent of about your
strength, and a coach sits beside you.

A few ideas shaped everything:

- **The coach never plays for you.** It answers when you ask, and only warns you by itself if you choose that.
- **You choose how much help you get**, from none at all to full coaching.
- **Asking beats being told.** Working something out yourself teaches more than reading the answer, so the strongest
  aids are questions you ask: a hint, "what does the opponent threaten?", "why is this move good?".
- **Plain words, no engine numbers.** Everything the engines find is turned into a colour, a sentence or a win
  chance.
- **An opponent that plays like a person.** A computer that suddenly drops its queen for no reason teaches nothing.
  The opponent's mistakes should be the kind a human of that strength makes.
- **Any engine works.** The program talks to its chess engines in the standard language for chess engines (UCI). The
  built-in engine, Buster, is only the default.

## 2. Three engines behind the board

Behind the board run three copies of a chess engine, each with its own job:

- **The opponent** plays against you.
- **The coach** looks at every move you could make and gives each one a score. It also works out what the opponent
  threatens, and, on the opponent's turn, helps the opponent choose its mistakes (chapter 7).
- **The master coach** looks for the single best move, as deep as it can while you think. Its move is the green move
  and the hint.

Why two coaches? "How good is *each* of my moves?" and "what is *the* best move?" are different questions. The first
needs many short looks, one per move; the second needs one long, deep look. One engine can't do both at once, and you
shouldn't have to wait.

When your turn starts, both coaches begin. Within a second or two every move has a colour, and the colours keep
getting more reliable while you think: the coach looks again, deeper each time, at the moves that matter. A small dot
in the status bar turns deep green when the verdicts have settled.

## 3. How the coach judges a move

Every move gets a **win chance**: how likely you are to win after playing it, from 0 to 100 %. Equal is 50 %; a pawn
up is about 64 %; a whole piece up about 85 %.

Win chance is a better measure for a player than "pawns". Losing a pawn in an equal position matters a lot; losing a
pawn when you're already a rook up hardly matters at all, and the win chance shows exactly that.

The coach compares each move with the best one:

- **Green:** the best move. Always exactly one.
- **Amber:** a playable move, a little worse than the best.
- **Red:** a mistake. It loses 15 or more points of win chance, or throws away two pawns or more, or walks into a
  mate, or misses a mate you had.
- **Black:** the coach hasn't looked yet.

The bar beside the board shows your win chance in the position, and the graph under the move list shows how it went
through the game, with your mistakes marked.

## 4. Why, in words

Rest the mouse on a move and the coach tells you, in one sentence, why it is good or bad:

- "g4: allows mate."
- "Nxe5: wins the knight on move 16."
- "Bxe4: gives the bishop for two pawns, and wins it back on move 25." (a sacrifice)
- "Nd5: moves the knight away from …exf6, the threat."
- "Nxd4: starts an even exchange."
- "Rc1: attacks the bishop on c5, which is undefended."
- "O-O: castles: the king is safe, the rook joins." / "Nf3: develops the knight."
- "a3: a quiet move: no tactics."

To find the sentence, the coach plays the engine's expected line through on its own board and follows what is
captured, what is attacked and what is threatened. The engine says *how good* a move is; the coach's own board says
*what happens*. When it explains a mistake, the coach never names the better move: that's what the hint is for.

## 5. The threat

Press **T** and the coach answers "what does the opponent threaten?". It flashes the attacking piece and its target,
and says it in words: "Buster threatens …exf3, taking your undefended knight."

The trick behind it: the coach imagines that you **pass**, so the opponent moves twice in a row. The opponent's best
move then is the threat. It only counts as a real threat if ignoring it would be a mistake; a capture that isn't
worth stopping is named as such.

## 6. The coaching levels

Four levels, from no help to full help. Each level is a set of switches; change one and you're on **Custom**.

| | Tournament | Play | Guided | Full coaching |
|---|---|---|---|---|
| Movable pieces glow, legal targets only, last move, check | – | – | ✓ | ✓ |
| The win-chance bar | – | – | ✓ | ✓ |
| The box's colour (green / amber / red) | – | – | – | ✓ |
| Thinking aids: why, the strongest line, the threat, hanging pieces | – | – | ✓ | ✓ |
| Hints (H) | – | ✓ | ✓ | ✓ |
| Comments after moves ("Buster missed …") | – | ✓ | ✓ | ✓ |
| Warnings by themselves | – | – | – | ✓ |

**Guided** has everything you can ask for, but doesn't give away the verdict. **Tournament** also plays by tournament
rules: a clock, touch-move, no takebacks, no pause. In every level the coach still records each move, so you can look
back at the game afterwards.

---

## 7. An opponent that errs like a person

### The problem

A chess engine is far stronger than any human. Making it weaker is easy; making it weaker *like a human* is hard.

The usual ways feel wrong. An engine that simply thinks less plays strangely mechanical moves. An engine that makes
random mistakes drops pieces for no visible reason. The first version of Buster Coach chose mistakes by their size,
and a strong tester said exactly what was wrong: *a 2200 player doesn't hang a piece to a one-move capture.*

### What people actually do

A player rarely loses because they didn't think. They lose because they played a move that **looked fine to them**,
and it was punished by something they didn't see. How far a player sees is much of what their rating means:

- a beginner often doesn't check the reply at all;
- a club player checks the captures, but misses a quiet move that sets up a fork;
- a master misses something deeper still.

### How a level plays

Each level has a **horizon**: how far ahead it looks when it judges a move. Level 1 looks only at your direct answer;
level 9 looks about three moves ahead for each side.

On each move the opponent does one of two things:

1. **Plays the engine's own move.** This is its normal, careful move. At the lower levels the engine thinks less, so
   these moves have small inaccuracies of their own.
2. **Plays from a quick look.** Like a person, it glances at all its moves, keeps the few that look best, and looks
   at those again, as far as its horizon. Then it plays the one that looks best there.

Most of the time the quick look's move is perfectly good. A mistake happens only when the position hides something
**just beyond the horizon**: a combination one move too deep, a quiet move that wins a piece. That's exactly when a
player of that strength would go wrong too. **The mistake is never chosen by its size**; the position decides whether
there is one.

Some extra rules keep it human:

- **A safety net.** Before a move is played, the coach checks it more deeply. A move that would be a wild,
  out-of-character blunder for that level is never played. The net is coarse on purpose: the subtle mistakes still
  happen.
- **Only beginners hang pieces.** At levels 1–3 the opponent sometimes doesn't check your reply at all, and can leave
  a piece where it can simply be taken. From level 4 up that doesn't happen.
- **Careful after the opening.** People play their first moves out of the opening carefully and make most of their
  mistakes in the middlegame. The opponent makes fewer mistakes just after the opening (half as many from level 4
  up), and is back to normal by move 25.
- **The opening book.** In the first moves the opponent plays what people really play, taken from millions of
  online games. Stronger levels follow the book longer, as stronger players do.
- **No mistakes in a decided game.** Once the game is clearly won or lost, the opponent plays the engine's own moves.
  A won ending is played out properly.

When the opponent does make a mistake, the saved game marks it with "?" and says what it should have played. At the
Guided and Full levels a card says "Buster slipped. There's something here." It doesn't say what: finding it is the
exercise.

### The levels

| Level | Meant to play like | Looks ahead |
|---|---|---|
| 1 · Beginner | a lichess rapid player of about 1000 | your reply |
| 2 · Novice | about 1150 | 1–2 half-moves |
| 3 · Casual | about 1300 | 2 half-moves |
| 4 · Improving | about 1450 | 2–3 half-moves |
| 5 · Club player | about 1600 | 3 half-moves |
| 6 · Strong club | about 1750 | 3–4 half-moves |
| 7 · Tournament player | about 1900 | 4–5 half-moves |
| 8 · Strong tournament | about 2050 | 5 half-moves |
| 9 · Club champion | about 2200 | 6 half-moves |
| 10 · Top amateur | about 2400 | 6–7 half-moves |
| 11 · Full strength | the engine at full strength | — |

A half-move is one move by one side. "2–3" means: sometimes 2, sometimes 3, drawn fresh each move. The ratings are
**lichess rapid** ratings: online ratings run higher than FIDE ratings, so a lichess 2200 is roughly a FIDE 1900–2000
club player. They belong to the built-in Buster; with another engine the levels keep their order but the ratings
aren't shown.

## 8. How the levels were measured

The levels weren't guessed. They were measured in two ways, and the second one turned out to be the one that counts.

**1. Counting mistakes.** From about 3,000 analysed lichess games per rating band, we counted how often players of
each rating make a mistake, and how big. The first tuning made each level's mistakes match those counts.

**2. Playing games.** A developer version of the program can play against itself, each side at its own level, the
same way a real game is played. The levels played each other, close to 2,000 games in all, and level 9 played 100 games
against Stockfish set to a known strength of 2200. Together that gives every level a rating.

The surprise: levels whose mistakes matched the human counts came out **a few hundred points too weak**. The same
number of mistakes cost the program more games than they cost people; why exactly, we don't know yet. So the final
tuning was done by **results**, with the mistake counts only as a check. That's also where the "careful after
the opening" rule came from: the strong levels lost too many games early, which humans of their strength rarely do.

**Where the levels stand now** (measured, give or take about 100–150 points):

| Level | Label | Measured |
|---|---|---|
| 1–3 | 1000 – 1300 | about 1400 – 1500 |
| 4 | 1450 | about 1600 |
| 5 | 1600 | about 1700 |
| 6 | 1750 | about 1800 |
| 7 | 1900 | about 1950 |
| 8 | 2050 | about 2050 |
| 9 | 2200 | about 2150 |
| 10 | 2400 | about 2400 |

**Known limits, honestly:**

- **The beginner levels are too strong and too close together.** Levels 1–3 play like about 1400–1500 and differ
  little from each other. Making them weaker is the next step; the way to do it is the size of their blunders, not
  their timing.
- **No real people were measured.** The games were against the program itself and Stockfish. If a level feels too
  strong or too weak for your rating, please say so (see the README).
- **The ratings are for the built-in Buster** on an ordinary PC. Another engine, or a very slow machine, changes the
  strength.

## 9. How it got here

Buster Coach was built in the autumn of 2026 by the author of the Buster chess engine, as a way to learn Qt, the
toolkit for desktop programs. It started as a tutorial — a window, then a board, then a mouse click — and grew into a
coach. Claude, an AI assistant, was the tutor: it explained, reviewed and measured, and wrote the helper tools; the
author wrote the program, apart from a few changes handed to the assistant.

- **September:** learning Qt: a first window, a hand-drawn board, the mouse, talking to the engine.
- **October:** the chess rules in plain code, with tests; the coach (colours, hints, the threat, the "why"
  sentences); clocks, a board editor, openings, saving games; the first opponent.
- **4 November 2026: version 1.0.0.** Followed by two small fixes (1.0.1, 1.0.2).
- **November:** a tester's verdict on the opponent's mistakes ("really stupid and big mistakes") led to the opponent
  described in chapter 7, measured by results in chapter 8: **version 1.0.3.**

Some lessons along the way:

- **Measure, don't guess.** Nearly every rule on this page was changed at least once by a measurement.
- **A count can be right and the kind wrong.** The first opponent made exactly as many mistakes as humans, and still
  felt wrong to a strong player.
- **Matching mistakes isn't matching strength.** The second opponent made human-like mistakes, and was still too
  weak. In the end, results decide.

---

## Words used

- **Win chance:** how likely a side is to win, 0–100 %. Equal is 50 %.
- **Point:** one percentage point of win chance. A red move loses 15 or more.
- **Green / amber / red move:** the best / a playable one / a mistake.
- **Half-move:** one move by one side. "Move 16" is a full move: White's and Black's.
- **Horizon:** how far ahead a level looks when it judges a move.
- **Opening book:** the opening moves real players choose, taken from online games.
- **UCI:** the standard language between a chess program and a chess engine.
- **Rating:** the strength scale. 150 points apart means the stronger side scores about 70 %.
