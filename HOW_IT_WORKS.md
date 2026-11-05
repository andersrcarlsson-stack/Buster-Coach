# How Buster Coach works — the story behind it

This is the long version of the README: what Buster Coach does, how it does it, and why it ended up this way. It is
written for two kinds of reader at once. Every chapter opens with **In short**, a few lines in plain words; a
newcomer can read just those and get the whole picture. **Under the hood** follows for the reader who wants the
mechanics, the rules and the numbers. The built-in help (F1 in the app) explains how to *use* it; this document
explains how it *thinks*.

**Contents**

- Part 1 — The idea: [1. What it's for](#1-what-its-for) · [2. Three engines, one game](#2-three-engines-one-game) ·
  [3. The life of one move](#3-the-life-of-one-move)
- Part 2 — The coach: [4. What you see](#4-what-you-see) · [5. How a move is judged](#5-how-a-move-is-judged) ·
  [6. Why, in words](#6-why-in-words) · [7. The threat](#7-the-threat) ·
  [8. The coaching levels](#8-the-coaching-levels)
- Part 3 — The opponent: [9. The problem](#9-the-problem) · [10. Strength](#10-strength) ·
  [11. Human mistakes](#11-human-mistakes) · [12. Calibration](#12-calibration)
- Part 4 — [13. How it got here](#13-how-it-got-here)
- [Glossary](#glossary)

---

# Part 1 — The idea

## 1. What it's for

**In short.** Buster Coach is for *playing* and *training* chess, not for analysing with an engine. You play a real
game against a computer opponent of your strength, and a coach sits beside you. The coach never plays for you. It
answers when you ask, and, if you want it to, warns you when something is wrong. Everything it knows is turned into
things a player understands: a colour, a sentence, a win chance. You never see raw engine output.

**Under the hood.** A handful of principles shaped every decision:

1. **Easy to start.** A new player can start a game without reading anything.
2. **Help is chosen, not imposed.** From no help at all (Tournament) to full coaching, and every aid can be switched
   on or off on its own.
3. **Ask, don't be told.** The strongest aids are questions you ask: a hint, "what does the opponent threaten?",
   "why is this move good?". Research on how players learn is clear that working it out yourself beats being told,
   so the coach waits to be asked unless you choose the level that warns by itself.
4. **The engine is a black box.** Buster Coach talks to its engines only through UCI, the standard text protocol
   for chess engines. It never links engine code. So **any UCI engine** can be the opponent or the coach; Buster,
   the built-in one, is just the default.
5. **An opponent that plays like a person.** A weakened engine that drops a queen out of nowhere teaches nothing.
   The opponent's mistakes must be the kind a human of that rating makes (Part 3).

## 2. Three engines, one game

**In short.** Behind the board run three copies of a chess engine. One plays against you. Two coach you: one checks
every move you could make, the other looks for the very best move as deep as it can.

**Under the hood.** Each is a separate engine process, started at launch and kept for the whole session:

| Engine | Its job | How it searches |
|---|---|---|
| **The opponent** | plays the opponent's moves | a node limit per strength level (chapter 10); at full strength a virtual clock |
| **The coach** (general coach) | scores **every** legal move you have; finds the threat; on the opponent's turn, scores the opponent's moves for the mistake pick | fixed-depth searches of each move, in passes: depth 6, 8, 10, 12, 14 |
| **The master coach** | finds the single best move: the green move, the hint, the eval bar | an open-ended search of the position (`go infinite`) while you think; listened to from depth 10 on |

All three get a fresh start (`ucinewgame`) with every new game; within a game they keep their memory (hash
tables), which makes each turn's searches faster.

Why two coaches? They answer different questions. "How good is *each* of my moves?" needs many short searches, one
per move, compared on equal terms. "What is *the* best move?" needs one long search that goes as deep as time
allows. One engine can't do both at once, and you don't want to wait.

## 3. The life of one move

**In short.** When your turn starts, both coaches start thinking. Within a second or two every move you could make
has a colour, and the colours keep getting more reliable while you think. You move; the opponent's turn starts; the
coach now scores the *opponent's* moves too, and the opponent picks one — sometimes a human-like mistake. A short
pause, and its move appears.

**Under the hood.** One full cycle:

1. **Your turn starts.** The coach queues every legal move, plus one special entry: the *pass* (chapter 7). The
   master coach starts its open-ended search.
2. **Pass 1 (depth 6)** scores every move. The move under your mouse box is scored first, out of turn, so the box
   gets its colour at once. Moves that end the game (mate, stalemate, a draw) are scored exactly, without a search.
3. **Passes 2–5 (depth 8 … 14)** rescore only the *candidates*: the moves that aren't bad, plus the master's move.
   A clearly bad move stays bad; there's no need to look deeper at it.
4. **The master speaks** from depth 10. Its move becomes the green move and the hint; its score fills the eval bar.
5. **The confidence dot** in the status bar goes from a yellow glow to deep green once the verdicts stop changing
   (three passes in a row without a change, or all five passes done).
6. **You move.** Your move is recorded with everything the coach knew at that moment: its verdict, its score, the
   best move, the line, how many hints you used.
7. **The opponent's turn.** The opponent engine searches at its level. Meanwhile the coach, otherwise idle, scores
   the opponent's legal moves three ways: as they *really* are (depth 8), and as the opponent might *see* them in a
   moment of carelessness (two short views; chapter 11). The pick then chooses the move.
8. **The courtesy pause.** The opponent never answers faster than 0.8 seconds, so you can see its move coming.
   Opening book moves skip the engine and the pick: the opponent plays what people really play (chapter 10).

---

# Part 2 — The coach

## 4. What you see

**In short.** Pick up a piece and a box follows the mouse: **green** for the best move, **amber** for a playable one,
**red** for a mistake, **black** while the coach doesn't know yet. Keep the mouse still and the coach tells you
*why*. Around the board: an eval bar (your win chance), a graph of the game, a confidence dot, and a column of small
cards with the coach's words.

**Under the hood.** The aids, and what each answers:

| Aid | The question it answers |
|---|---|
| **The box** (green / amber / red / black) | Is this move good? |
| **Lingering** on a move (rest the mouse) | Why? Then, a moment longer: what's the strongest line? |
| **Lingering** on your own piece | Is this piece worth a look? A small number = how many of its moves aren't bad; a grey glow = only bad ones |
| **Space** | Show me that line on the board, one move per press |
| **H** (hint) | What's the best move? First press: the piece. Second: where it goes. Hints are counted |
| **T** (threat) | What does the opponent threaten? (chapter 7) |
| **The red "!"** | Which of my pieces are hanging? |
| **The eval bar** | Who is better, as a win chance (and "M3" for a mate) |
| **The win-chance graph** | How did the game go? Red dots on your mistakes |
| **The confidence dot** | Has the coach made up its mind yet? |
| **The coach line** (cards) | The position now (threat or check) · a note ("Buster missed …") · a hanging piece · the move's why · the strongest line |
| **"Buster missed it"** | After your red move, if the opponent didn't punish it: what it missed |

Colours have fixed meanings everywhere: green, amber and red are verdicts (red is also danger: the threat, the "!");
magenta is the hint; violet marks an illegal attempt; blue is the "if you play this" preview. Colour-blind marks
(▲ ● ▼) can be added in the settings.

## 5. How a move is judged

**In short.** Every move gets a **win chance**: how likely you are to win after playing it. The coach compares each
move with the best one. Lose a lot of win chance and the move is red; a little, and it's amber. Exactly one move is
green: the master coach's choice.

**Under the hood.**

**Win chance.** An engine scores a position in *centipawns* (hundredths of a pawn). Buster Coach turns that into a
chance from 0 to 100 %:

> win chance = 1 / (1 + 10^(−centipawns / 400))

Equal is 50 %; a pawn up is about 64 %; a piece up about 85 %. A found mate is 100 % (or 0 %). Win chance is the
right scale for a player because a pawn matters much more in an equal position than when you're already a rook up.

**The verdict rule.** Each move is judged against the coach's best score this turn:

- **Green (correct):** the master's move — until the master has spoken, the coach's own best. Always exactly one.
- **Red (bad):** any of
  - it loses **15 or more points** of win chance; or
  - it is **2 or more pawns** worse (a backstop: when one side is far ahead, win chance hardly moves, yet throwing
    away a rook is still a mistake); or
  - it **walks into a mate** the best move avoids; or
  - it **misses a forced mate** the best move has (a slower mate is fine; "still winning" isn't enough).
- **Amber (playable):** everything else.
- **Black:** not scored yet.

The eval bar's words use the same scale: 43–57 % is *equal*, from 57 % *slightly better*, from 70 % *clearly
better*, from 90 % *winning* (and the same mirrored).

**Why depth 6 → 14 in passes?** The first pass must be fast, so every move has a colour within a second or two. Deeper
passes then correct the few verdicts a shallow search gets wrong. The confidence dot shows when that has settled.
Every search also has a time cap (5 seconds; 2 for the opponent's short searches): in a rare position an engine's
search can blow up, and then the coach stops it and keeps the deepest score it reached.

## 6. Why, in words

**In short.** Rest the mouse on a move and the coach says, in one sentence, why it is good or bad: "Nxe5: wins the
knight on move 16." "g4: allows mate." "Nd5: moves the knight away from …exf6, the threat." "O-O: castles: the king is
safe, the rook joins." It never names the best move when it explains a mistake: that's what the hint is for.

**Under the hood.** The coach has the move's score and its *line* (the moves the engine expects to follow). From
those it builds the first sentence that applies, in this order:

1. **Mate.** "…: leads to mate." / "…: allows mate." If the opponent threatens mate and your move deals with it:
   "Kh1: stops the mate threat (…Qh2#)."
2. **A sacrifice.** A move that isn't red but whose line *gives* material first: "Bxe4: gives the bishop for 2 pawns,
   and wins it back on move 25." or "…: the coach judges the play worth it."
3. **Material.** The line is played through on the coach's own board, and the material balance followed ply by ply.
   The gain or loss is counted where the line *settles* (no capture pending), and the words name the pieces:
   "Nxe5: loses the queen and the knight for a bishop and 2 pawns on move 7." For a move that isn't red, only the
   first six plies count: a gain far down the line isn't the reason for the move (and the line's tail is its least
   reliable part).
4. **Red moves stop here.** Without mate or material, a red move gets "another move wins a pawn on move 5" (what the
   best move achieves, never naming it) or "a worse position than the best move (no tactics)".
5. **It stops something.** It answers the opponent's threat: takes the attacker, moves the piece away, blocks,
   defends, or makes the threat cost a piece. Or it avoids the *common reply*: when one opponent move punishes most of
   your red moves, a safe move "keeps the knight safe from …Bxf3".
6. **An even exchange.** "Nxd4: starts an even exchange (knight and pawn each)."
7. **It creates a threat.** "Rc1: attacks the bishop on c5, which is undefended." / "Ra8+: gives check." / "…: a
   double attack."
8. **A principle.** "Nf3: develops the knight." / "e4: takes the centre." / "O-O: castles: the king is safe, the rook
   joins." Otherwise: "a3: a quiet move: no tactics."

Behind these sentences sits a plain C++ board model with its own attack maps: which pieces attack a square, which
pieces are *hanging* (attacked and undefended, or attacked by a cheaper piece), what a move newly attacks. The
engine says *how good*; the model says *what happens*.

## 7. The threat

**In short.** Press **T** and the coach answers "what does the opponent threaten?": it flashes the attacking piece and
its target, and says it in words ("Buster threatens …exf3, taking your undefended knight."). If nothing real is
threatened, it says so.

**Under the hood.** The coach imagines you **passed**: the same position with the opponent to move again (a *null
move*). The opponent's best move there is the threat. This search runs first in every pass, so the answer is ready
before you ask, and grows more reliable with each pass.

A threat is **real** only if ignoring it would be a red move: the pass is judged with the same verdict rule as your
moves. A capture that isn't worth stopping is named as such ("Buster can take: …Nxd5. Stopping it isn't worth it.").
The threat isn't asked while you're in check (the check words show instead), nor while you have a forced mate
yourself. At the Full coaching level, a real threat is shown by itself once per turn, and your hanging pieces get the
red "!" without asking.

## 8. The coaching levels

**In short.** Four levels, from no help to full help: **Tournament**, **Play**, **Guided**, **Full coaching**. Each
level is a set of switches; change one and you're on **Custom**.

**Under the hood.**

| Switch | Tournament | Play | Guided | Full |
|---|---|---|---|---|
| Guidance (movable pieces glow, legal targets only, last move, check) | off | off | on | on |
| Evaluation bar | off | off | on | on |
| Move colours (the box's colour) | off | off | off | on |
| Thinking aids (lingering, why, Space, T, the "!") | off | off | on | on |
| Hints allowed (H) | off | on | on | on |
| Comments after moves ("Buster missed …") | off | on | on | on |
| Warnings by themselves (the threat and the "!" unasked) | off | off | off | on |

Guided is deliberately *without* move colours: everything is there to ask for, but the verdict isn't given away.
Tournament adds real tournament rules: a clock is required, touch-move, no takebacks, no pause, and a second illegal
move loses. The coach still records every move in every level, so the game can be reviewed afterwards.

---

# Part 3 — The opponent

## 9. The problem

**In short.** A chess engine is far stronger than any human. Making it weaker is easy; making it weaker *like a
human* is hard. The first version made its mistakes by random blunders, and a strong tester said exactly what was
wrong: a 2200 player doesn't hang a piece to a one-move capture. The opponent was rebuilt around one principle:
**only the mistakes a player of that rating could make.**

**Under the hood.** The usual ways to weaken an engine each feel wrong in their own way:

- **A short search** (a small node or depth limit) never drops a piece to a simple capture, but misses anything
  deeper, and always plays the same move in the same position. It feels mechanical.
- **Random noise on the scores** (the classic "skill level") produces moves that are bad for no visible reason.
- **Picking blunders on purpose**, by size, is what the first Buster Coach did: a blunder chance per level, chosen
  evenly among the moves that lose a lot. Measured afterwards, 3.2 of the 3.6 big blunders per 100 moves at the 2200
  level were *obvious* ones, visible at a single glance. The count was right, the kind was wrong.

What humans actually do: they play a move that **looks fine to them**, and it's punished by something they didn't
see. A beginner doesn't check the reply at all. A club player checks the captures but misses a quiet move that sets
up a fork. A master misses something deeper still. That's what the opponent now does.

## 10. Strength

**In short.** Eleven levels, from **1 · Beginner (≈ 1000)** through **10 · Master (≈ 2400)** to **11 · Full
strength**. Two levers set a level: how long the engine may think, and how often it makes a human mistake.

**Under the hood.**

- **Thinking time** is a node limit (`go nodes N`): 1,500 nodes at level 1, about ×2.2 a level, 1,500,000 at level 10.
  Full strength has no limit; untimed it thinks on a virtual 5-minute clock, fresh every move, so its own time
  management decides (with a 60-second cap). In timed games the real clocks apply as well.
- **The opening book.** Up to a depth that grows with the level (2 plies at level 1, the whole 14-ply book from level
  7), the opponent plays from a book of what players rated 2000+ really play (from the lichess games database), drawn by
  how often each move is chosen. A weak opponent leaves the book early, as a weak player would. Book moves skip the
  engine and the mistake pick.
- **The mistakes** (chapter 11) are the main lever up to the middle levels; above that, the node limit does more of
  the work and the mistakes become rarer.
- **Decided positions are left alone.** When the engine's own move is a found mate, or one side has 95 % or more,
  the opponent plays the engine's move. A random choice there would only undo a plan (a won endgame left unmated). When your move has already decided the game, the coach doesn't even score the
  opponent's moves: the opponent answers straight from its own search.

The ratings (≈ 1000 … ≈ 2400) are calibrated for the built-in Buster (chapter 12). With another engine the levels
still work, but the app doesn't show ratings it hasn't measured.

## 11. Human mistakes

**In short.** On some moves, the opponent plays from a *quick, careless look* at the position instead of its full
search. It picks a move that looks fine in that quick look. Usually the move *is* fine. Sometimes the quick look
missed something, and that's the mistake. Two kinds of quick look exist: a **lapse** (it doesn't check your reply at
all; only the beginner levels) and an **oversight** (it checks the captures, but misses a quiet move; every level
below full strength).

**Under the hood.**

While the opponent engine thinks, the coach (idle on the opponent's turn) scores every legal opponent move three
ways:

| View | What it is | What it sees | What it misses |
|---|---|---|---|
| **The truth** | the position after the move, depth 8 | nearly everything at this level of play | — |
| **The lapse** ("hope") | the position after the move, with *you passing*; the opponent looks once more (depth 1) | its own threats: "my knight attacks the queen!" | your reply entirely: the piece it just put en prise |
| **The oversight** ("a quick look") | the position after the move, searched one ply (your reply) plus every capture that follows | every capture sequence, so it never drops a piece to a plain capture | a quiet move: a fork prepared quietly, a pin, an in-between move, a mating net |

Why these two? A UCI engine can't be made blind to captures: every search ends in a *quiescence* search that follows
all the captures. So "blind to the reply" can only be built by letting the other side pass (the same trick as the
threat, chapter 7). And a deeper quick look doesn't simply make fewer mistakes: depth 3 erred *more* than depth 1–2
in the measurements (odd and even depths see differently). So the levels differ in *how often* they take a quick
look, not in how deep it is.

One important detail: before the quick views the coach's engine gets `ucinewgame`. Otherwise its memory still holds
the deep results from your turn, and a "quick" search reads them and sees as far as they did. (That one was found
by measurement: a depth-2 look returned depth 8's scores.)

**The pick**, one function (`pickMove` in `app/lib/uci/uci_info.cpp`), for every opponent move:

1. Roll the dice (a number from 0 to 1).
2. Below the level's **lapse** chance: play from the lapse view — among the moves that look within 5 points of its
   best-looking move, the better-looking ones likelier.
3. Else below lapse + **oversight**: play from the quick look. A share of these oversights (`best`: 25 % at levels
   1–5, 50 % at 6–8, 75 % at 9–10; the stronger player trusts its first impression more) plays the look's
   best-looking move; the rest choose among the moves that look within 15 points of it, weighted towards the best.
4. Otherwise: the engine's own move, from its full search at the level's node limit.

**The mistake is never chosen by its size.** The dice only decide *which view* the opponent plays from. Whether that
move turns out to be a mistake, and how big, the position decides: the truth (depth 8) is compared with the engine's
own move by the same red-move rule as yours. Most of the time the quick look's move is perfectly fine. A mistake
happens only when the position actually hides something beyond the quick look — which is exactly when a human
would make it.

**What you see.** When the opponent's move was a mistake, the saved game marks it `?` with "Best was …". At Guided
and Full a quiet card says "Buster slipped. There's something here." It doesn't say what: finding it is the
exercise (H gives it, if you want).

**The settings per level** (lapse / oversight chance per move):

| Level | ≈ Rating | Lapse | Oversight |
|---|---|---|---|
| 1 Beginner | 1000 | 22 % | 25 % |
| 2 | 1150 | 20 % | 22 % |
| 3 | 1300 | 2 % | 98 % |
| 4 | 1450 | — | 100 % |
| 5 | 1600 | — | 89 % |
| 6 | 1750 | — | 95 % |
| 7 | 1900 | — | 85 % |
| 8 | 2050 | — | 73 % |
| 9 | 2200 | — | 73 % |
| 10 Master | 2400 | — | 55 % |
| 11 Full strength | — | — | — |

An oversight chance near 100 % doesn't mean a mistake every move: it means the move is chosen by the quick look,
which is right most of the time. The node limit and the `best` share make the difference between, say, levels 4 and
6. Lapses — the only source of a piece hung to a plain capture — exist only at levels 1–3, by design: players from
about 1450 up don't do that often enough to imitate.

## 12. Calibration

**In short.** The numbers above weren't guessed. Each level's mistakes were matched to how often real players of that
rating make mistakes, measured from lichess games. Then the levels played well over a thousand games against each other to
check that each one is about 150 rating points stronger than the one below.

**Under the hood.** Two measurements, two tools.

**1. How many mistakes, and what kind.** From the lichess games database (September 2026, rapid and slower games, 3,000 per rating band, every move analysed by Stockfish), per rating
band: how many *red* moves (by the same verdict rule as the coach's, in positions not yet decided) players make per
100 moves, and how big they are. A measurement tool then took 600 positions from earlier self-play games, scored every
move in every view once, and searched each level's chances so that its red moves per 100 match the lichess band:

| Level | ≈ Rating | Red moves per 100, Buster Coach | Lichess players | Big blunders (30+ points), share of moves | Lichess |
|---|---|---|---|---|---|
| 1 | 1000 | 17.1 | 16.1 | 6.8 % | 5.3 % |
| 2 | 1150 | 15.4 | 14.4 | 6.5 % | 4.7 % |
| 3 | 1300 | 12.4 | 12.9 | 1.4 % | 4.2 % |
| 4 | 1450 | 11.5 | 11.7 | 0.9 % | 3.8 % |
| 5 | 1600 | 10.2 | 10.3 | 0.8 % | 3.3 % |
| 6 | 1750 | 9.2 | 9.5 | 0.6 % | 3.0 % |
| 7 | 1900 | 8.1 | 8.4 | 0.6 % | 2.7 % |
| 8 | 2050 | 7.1 | 7.3 | 0.5 % | 2.4 % |
| 9 | 2200 | 6.0 | 6.2 | 0.4 % | 1.9 % |
| 10 | 2400 | 4.6 | 4.7 | 0.3 % | 1.4 % |

And the one that mattered to the tester: **obvious** blunders, visible at a single glance, are **none from level 4
up** (5–6 per 100 at levels 1–2, about 1 at level 3).

**2. The steps between levels.** A developer build can play the app against itself, each side at a level, through
the same code path as a real game (coaching, record and saving included). Each level played the level above; the
results were turned into a rating difference:

| Pairing | Games | Stronger side's gain | Rating gap |
|---|---|---|---|
| 1 → 2 | 40 | +17 | 150 |
| 2 → 3 | 40 | +70 | 150 |
| 3 → 4 | 40 | +108 | 150 |
| 4 → 5 | 40 | +147 | 150 |
| 5 → 6 | 100 | +96 | 150 |
| 6 → 7 | 90 | +143 | 150 |
| 7 → 8 | 40 | +215 | 150 |
| 8 → 9 | 40 | +137 | 150 |
| 9 → 10 | 40 | +285 | 200 |

Every step goes the right way, and from level 4 up the whole ladder adds up to what the ratings say. With 40 games a
step, each single number is uncertain by about ±110 points, so 96 and 215 can both be a true 150.

**Known limits, honestly:**

- **The bottom levels sit close together.** Levels 1 to 4 span about 200 points in self-play instead of 450. Their
  mistake counts match lichess, but real beginners lose in more ways than counted blunders. Players' feedback will
  tell; the likely fix is a weaker level 1.
- **Levels 3–8 make more medium inaccuracies** (5–10 points) than lichess players, and fewer big blunders. That's a
  choice: big blunders only at the beginner levels, a cleaner opponent from about 1450 up; the red-move count then
  has to come from medium mistakes.
- **The ratings are for the built-in Buster on a typical PC.** Another engine, or a much slower machine, changes
  the strength per node.
- **A rating is a lichess-like rapid rating**, not a FIDE rating.

---

# Part 4 — The story

## 13. How it got here

**In short.** Buster Coach was built in about six weeks, in the autumn of 2026, by the author of the Buster chess
engine, as a way to learn Qt (the C++ toolkit for desktop programs). It started as a tutorial — a window, then a
board, then a mouse click — and grew into a coach. Claude, an AI assistant, was the tutor: it explained, reviewed
and measured, and wrote the helper tools; the author wrote the program, apart from a few late changes handed to the assistant.

**Under the hood.** The turning points:

- **Late September: the learning path.** Choosing Qt; a first window; signals and slots; layouts; a hand-drawn board
  widget; mouse input; then `QProcess`, talking to the Buster engine over UCI. From the first day the rule held: the
  engine is a black box behind UCI, so any engine works.
- **Early October: the board model.** A plain C++ chess model (legal moves, FEN, SAN, draws by rule), behind fast
  tests that run on every build. Everything the coach later says in words rests on it.
- **Mid October: the coach.** Scoring every move, the verdict colours, the master coach, the eval bar; then the
  coaching levels, hover and linger, the hint, the threat (the null-move trick), hanging pieces, and the "why"
  sentences. Dark mode, the pieces' look, clocks.
- **Late October: a complete program.** A board editor, engine setup (any UCI engine, tested before use), the game
  buttons, the win-chance graph, openings (names, a book and opening practice, from open lichess data), saving and
  opening games as PGN.
- **The first opponent calibration (late October).** A self-play mode and lichess error profiles; five rounds tuned
  the node limits and a blunder chance until the red-move counts matched.
- **Release work.** A built-in help with pictures cut from the real widgets; a public repository; a build workflow
  for Linux (AppImage) and Windows (zip). Under Wine, the Windows build's engines went silent mid-game; the release
  waited for a real Windows PC, where it ran cleanly — the fault was Wine's.
- **The beta's lesson (early November).** A 2200 tester's verdict on the old blunder chance — "really stupid and big
  mistakes" — led to the redesign in chapters 9–12: mistakes by what a player *sees*, not by size, measured and
  recalibrated in a day, with a new Master level (≈ 2400).
- **4 November 2026: version 1.0.0.**

Some lessons along the way, for anyone building something similar:

- **Measure, don't guess.** Nearly every rule above was changed at least once by a measurement: the blunders'
  obviousness, the depth that erred *more*, the hash that let a quick look see deep.
- **A count can be right and the kind wrong.** The first opponent matched lichess's mistake count exactly and still
  felt wrong to a strong player.
- **Keep the engine a black box.** It cost some convenience; it bought "any UCI engine" for free.
- **Fix known defects before adding features.** A known bug confounds everything built on it.

---

## Glossary

- **UCI** — Universal Chess Interface: the text protocol between a chess program and an engine (`position …`,
  `go …`, `bestmove …`).
- **Centipawn (cp)** — a hundredth of a pawn: the engine's unit of advantage.
- **Win chance** — the centipawn score turned into a chance of winning, 0–100 % (chapter 5).
- **Point** (of win chance) — one percentage point. A red move loses 15 or more.
- **Red / amber / green move** — bad / playable / the best (chapter 5).
- **Obvious blunder** — a big mistake visible at a single glance (a one-ply look).
- **Ply** — one move by one side. "Move 16" is a full move: White's and Black's.
- **Depth** — how many plies an engine searches ahead (plus captures).
- **Node** — one position the engine looks at. A node limit caps how much it may look at.
- **Quiescence** — the engine's habit of following every capture to the end before scoring a position: why no
  search is ever blind to a capture.
- **Null move / pass** — letting one side "pass" to see what the other side threatens.
- **Hash (table)** — the engine's memory of positions it has searched.
- **Lapse / oversight** — the opponent's two kinds of human mistakes (chapter 11).
- **Book** — the opening moves real players choose, built from lichess games.
- **Decided position** — one side has 95 % or more, or a found mate.
- **Elo / rating** — the strength scale; 150 points apart means the stronger side scores about 70 %.
