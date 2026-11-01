# testdata/ — real PGN files for pgn_test (pgn_design.md step 4d)

- **lichess.pgn:** four rated games from the lichess game database, September 2026 (https://database.lichess.org),
  released under **CC0**. Picked for their features: lichess's `[%eval]` and `[%clk]` comments, castling on both wings,
  a promotion, a mate, a long draw without evals.
- **own1.pgn, own2.pgn:** two games Buster Coach autosaved on 2026-10-25 (own2 with taken-back tries). The test
  reads them and writes them again: the same text, byte for byte.
