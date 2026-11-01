# Third-party components

Buster Coach itself is under the MIT licence ([LICENSE](LICENSE)). It uses, and the releases include, the following.

## Qt 6 — LGPL-3.0

The user interface is built with [Qt](https://www.qt.io) (Qt Base: Core, Gui, Widgets), © The Qt Company Ltd and
other contributors, used under the **GNU Lesser General Public License v3** (https://www.gnu.org/licenses/lgpl-3.0.html).

Qt is linked dynamically: the Linux AppImage carries Qt's shared libraries inside it, the Windows zip has them beside
`buster_gui.exe`. You may replace them with your own build of the same Qt version. Qt's source code is available from
https://download.qt.io/archive/qt/ (the version is shown in Help → About Qt) and https://code.qt.io. The licence texts
are in [licenses/](licenses/) (the LGPL builds on the GPL, so both), and in every download.

## Buster — MIT

The built-in chess engine, [Buster](https://github.com/andersrcarlsson-stack/Buster-Chess), © 2026 Anders R Carlsson,
MIT licence. It runs as a separate program (`buster` / `buster.exe`) and is spoken to over the UCI protocol only.

## The chess pieces — GNU FreeSerif, GPL-3.0-or-later with the font exception

`app/fonts/BusterPieces.ttf` holds the twelve chess glyphs (U+2654–U+265F) of **GNU FreeSerif** (GNU FreeFont,
2002–2012, the GNU FreeFont contributors), subset and renamed "Buster Pieces". Licence: GPL-3.0-or-later with the GNU
FreeFont **Special Font Exception**: embedding the font in a program does not by itself make the program GPL. Source:
https://www.gnu.org/software/freefont/

## Opening data — lichess, CC0

- The opening names (`app/data/openings.tsv`) come from lichess's
  [chess-openings](https://github.com/lichess-org/chess-openings), released under **CC0 1.0**.
- The opening book (`app/data/book.bin`) is counted from the [lichess game database](https://database.lichess.org)
  (September 2026, rated games), released under **CC0**.

Thank you, lichess.

## The strength calibration

The error rates the opponent's levels were matched to were measured from the lichess game database (CC0) as well.
