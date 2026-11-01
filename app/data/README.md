# data/ — prepared data built into Buster Coach (as Qt resources)

## openings.tsv — the opening names
One line per named position: `key <TAB> eco <TAB> name <TAB> moves` (the moves: how to get there from the start, in
UCI, space-separated; opening practice plays them). The key is the position's FEN without its last two fields
(the halfmove clock and the move number), exactly as the board model writes it (`Position::FenOutput`), so a lookup
matches only if the app builds the key the same way.

- **Source:** lichess's `chess-openings` (https://github.com/lichess-org/chess-openings), files a.tsv … e.tsv,
  released under **CC0 1.0** (public domain dedication). Thank you, lichess.
- **Made by:** `tools/openingbook names a.tsv b.tsv c.tsv d.tsv e.tsv > openings.tsv` (each line played through with
  the board model; 3,865 positions; 623 KB raw, ~79 KB built in).

## book.bin — the opening book (what people play)
14 bytes per move, little-endian, sorted by key; a position's moves are next to each other, most played first:

| bytes | field | |
|---|---|---|
| 0–7 | key | FNV-1a 64-bit hash of the position key (as in openings.tsv: the FEN without the move counters) |
| 8–9 | move | from \| to << 6 \| promotion << 12 (squares a1 = 0 … h8 = 63; promotion 0 none, 1 n, 2 b, 3 r, 4 q) |
| 10–13 | games | how many games played it (the real count): the weight; a position's games = the sum of its moves' |

- **Source:** the lichess game database (https://database.lichess.org), September 2026, rated games, released under
  **CC0** ("research, commercial purpose, publication, anything you like"). Thank you, lichess.
- **Made by:** `zstdcat …2026-09.pgn.zst | tools/openingbook count 1000000 > counts.tsv` (both players 2000+, blitz or
  slower; 1,000,000 games of 15.7 M), then `openingbook pack counts.tsv book.bin 14 100 3 35`: the first 14 plies,
  positions seen in 100+ games, moves played in 3 %+ of a position's games and scoring 35 %+ for the mover. 11,076
  positions, 40,585 moves, 568 KB raw (~218 KB built in).
- **Format change 2026-10-24:** the count was 2 bytes, scaled down per position when over 65,535 (5 positions);
  that broke comparing positions (a name's popularity). Now 4 bytes, the real count. The counts are kept outside the repository: another setting is one
  `pack` away (up to 20 plies, 5 games).
- `openingbook probe book.bin "<fen>"` lists a position's moves with their share: the reader in ~40 lines.
