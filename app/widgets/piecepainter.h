// piecepainter.h — one chess piece, drawn the Buster Coach way (Looks, 2026-10-20): the "Buster Pieces" glyphs in two
// layers, a solid body (the outline glyph's contours, every one filled) and the outline glyph on top. Used by the
// board and the eval bar, so they always look the same.
#pragma once
#include <QColor>
#include <QPalette>
#include <QPixmap>

class QPainter;
class QRect;

// code: the model's piece code (1-12, Black odd, White even). tint: a state colour (hover, selection, hint, ...) that
// tints body and lines; an invalid QColor() = no state (white off-white, black dark grey, black lines). rim: a thin
// edge around the body (a dark piece on a dark background); invalid = none.
void drawPiece(QPainter &painter, const QRect &cell, int code, const QColor &tint = QColor(),
               const QColor &rim = QColor());
// The rim a piece needs off the board, on this palette: light grey for a black piece in a dark theme, else none.
QColor panelRim(const QPalette &palette, int code);
// The same piece as an image, drawn off screen: size × size pixels, transparent around the piece. For a QLabel
// (setPixmap), wherever a piece should look exactly like the board's.
QPixmap piecePixmap(int code, int size, const QColor &rim = QColor());   // rim: as drawPiece's

