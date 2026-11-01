// boardwidget.cpp — drawing the board and handling clicks.
//
// Sections:
//   1. Colours
//   2. Construction
//   3. Painting: the board, square by square, in layers; the glow helpers
//   4. Mouse input: press, move, leave
//   5. State from outside: position, last move, flip, guidance, viewing, input
//   6. The promotion chooser
//   7. Coaching on the board: box colours, the box move, marks, game end, the hint

#include "boardwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QDebug>
#include <QPainterPath>
#include <QFontMetricsF>
#include "move.h"
#include "piecepainter.h"

// =================================================================================================
// 1. COLOURS
// =================================================================================================
// The colour of an illegal attempt (the box and the glyph): violet, so red is free for "bad move".
static const QColor illegalColour(140, 60, 200);
// Game end: the mated king (deeper than the "bad move" red) and both kings on a draw (neutral).
static const QColor mateColour(150, 0, 0);
static const QColor drawColour(110, 110, 110);
// The hint: gold, used nowhere else ("a tip", not "you did something"). The tone in Looks.
static const QColor hintColour(200, 40, 160);   // magenta: the coach's suggestion, apart from green / amber / red (Looks)
// The linger flash: the strongest reply to the move under the box (for now the "bad move" red; Looks).
static const QColor replyColour(200, 40, 40);
// The play-through's frame around the board: "this is a preview, not the game" (Looks: tune).
static const QColor previewColour(0, 150, 255);

// A book move's sign: a small open book sitting on a box's top edge, two mirrored pages in the box's colour; hollow
// (white pages, the colour as their edge) for a book move that leaves the practised opening (openings_design.md).
static void drawBook(QPainter &painter, const QRect &box, int sq, const QColor &colour, bool hollow) {
    const double w = sq / 2.4, h = sq / 5.0, cx = box.center().x(), y = box.top() - h / 2;
    painter.setPen(hollow ? QPen(colour, 2.2) : QPen(Qt::white, 1.3));
    painter.setBrush(hollow ? QColor(Qt::white) : colour);
    for (const int side : {-1, 1}) {                 // the left page, then the right
        QPainterPath page;
        page.moveTo(cx, y + h * 0.25);               // the spine's top
        page.quadTo(cx + side * w * 0.25, y, cx + side * w / 2, y + h * 0.1);
        page.lineTo(cx + side * w / 2, y + h);
        page.quadTo(cx + side * w * 0.25, y + h * 0.85, cx, y + h * 1.12);   // back to the spine
        page.closeSubpath();
        painter.drawPath(page);
    }
}

// =================================================================================================
// 2. CONSTRUCTION
// =================================================================================================
BoardWidget::BoardWidget(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);   // mouseMoveEvent also without a button held (for hover)
    m_lingerTimer.setSingleShot(true);   // one timeout per wait
    m_lingerTimer.setInterval(1500);      // lingering: the mouse stays this long on one square (tune later)
    connect(&m_lingerTimer, &QTimer::timeout, this, [this]() {
        m_lingerSquare = m_hoverSquare;
        emit lingerChanged(m_lingerSquare);
        m_deepTimer.start();             // level 2 waits for another interval
        update();
    });
    m_deepTimer.setSingleShot(true);
    m_deepTimer.setInterval(1500);       // level 2: this much longer on the same square (tune with the first)
    connect(&m_deepTimer, &QTimer::timeout, this, [this]() {
        if (m_boxMove.isEmpty())
            return;                      // no box on this square: nothing to show at level 2
        m_deepLinger = true;
        emit lingeredOnMove(m_boxMove);
    });
}

// =================================================================================================
// 3. PAINTING: the board, square by square, in layers; the glow helpers
// =================================================================================================
// Called by Qt whenever the widget must be drawn: first show, resize, maximise/restore,
// or after update(). Never called while idle. Qt may have discarded the old picture,
// so every call draws EVERYTHING, purely from the current state (m_position, m_sel*).
void BoardWidget::paintEvent(QPaintEvent *) {
    // qDebug() << "paint" << width() << "x" << height();
    QPainter painter(this);                 // draw on this widget
    painter.setRenderHint(QPainter::Antialiasing);   // smooth edges on shapes (edge pixels partly coloured)
    const QRect board = boardRect();        // same geometry as mousePressEvent uses
    const int sq = board.width() / 8;
    const int x0 = board.x();
    const int y0 = board.y();
    if (sq < 8) return;                     // too small to be useful (and a font size of 0
                                            // would trigger a Qt warning): draw nothing

    // Piece font: size follows the squares. Pixels (not points) so it matches `sq`.
    QFont font = painter.font();
    font.setFamily("Buster Pieces");   // FreeSerif's chess glyphs, built into the program (main.cpp, resources.qrc)
    font.setPixelSize(int(sq * 1.1));   // 10 % larger than the square: fuller pieces, corner marks still clear (Looks)
    painter.setFont(font);

    // Legal targets of the selected piece (from the model): where the target box may appear (level 1).
    bool target[64] = {};                   // model numbering; all false
    if (m_selSquare != -1) {
        for (const Position::Move &m : m_model.LegalMoves(m_position).moves)
            if (m.from_square == m_selSquare)
                target[m.to_square] = true;
    }

    // Check: the side to move's king is attacked. Worked out from the position alone.
    Moves checker;
    const bool inCheck = checker.in_check(m_position, 0, 0);
    const int kingCode = m_position.pit ? 11 : 12;   // the side to move's king (Black odd)

    // The linger flash: lingering with the box on a move shows its strongest reply (the piece and its target).
    int replyFrom = -1, replyTo = -1;
    if (m_lingerSquare != -1 && !m_boxMove.isEmpty()) {
        const QString reply = m_targetReplies.value(m_boxMove, m_targetReplies.value(m_boxMove + "q"));
        if (reply.size() >= 4) {                     // "d7d5" -> 51 and 35 (a promotion: the queen's reply)
            replyFrom = uciSquare(reply, 0);
            replyTo   = uciSquare(reply, 2);
        }
    }
    
    // Buster's threat, in the same look, when no linger flash is showing. Only on the live position.
    if (replyFrom == -1 && m_threat.size() >= 4 && !m_viewing) {
        replyFrom = uciSquare(m_threat, 0);
        replyTo   = uciSquare(m_threat, 2);
    }

    // `rank` here is the SCREEN row (0 = top), model square = squareAt(rank, file)
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            // Layer 1: the square (its colour, last move, check, coordinates).
            // a8 (row 0, file 0) is a light square, so (row + file) even = light.
            bool light = (rank + file) % 2 == 0;
            QColor colour = light ? QColor(240, 217, 181) : QColor(181, 136, 99);
            painter.fillRect(x0 + file * sq, y0 + rank * sq, sq, sq, colour);

            // Last move (Looks, H1): "from" an even yellow-green square with a pale spot where the piece stood; "to" a soft glow in the same colour behind the
            // piece that moved (yellow-green, clearly yellower than the coach's verdict green; plain yellow sank into
            // the board's own colours).
            const int square = squareAt(rank, file);
            const QColor lastMove(155, 199, 0);
            if (m_guidance && square == m_lastFrom) {
                const QRect fromCell(x0 + file * sq, y0 + rank * sq, sq, sq);
                QColor from = lastMove;
                from.setAlpha(120);
                painter.fillRect(fromCell, from);
                QRadialGradient spot(QPointF(fromCell.center()), sq * 0.55);   // a pale spot where the piece stood
                spot.setColorAt(0.0, QColor(255, 255, 220, 170));
                spot.setColorAt(1.0, QColor(255, 255, 220, 0));
                painter.fillRect(fromCell, spot);
            }
            if (m_guidance && square == m_lastTo) {
                const QRect to(x0 + file * sq, y0 + rank * sq, sq, sq);
                QRadialGradient glow(QPointF(to.center()), sq * 0.72);   // reaches past the edge: fillRect clips it
                QColor c = lastMove;
                for (auto [at, alpha] : {std::pair{0.0, 230}, {0.45, 150}, {1.0, 0}}) {   // strong in the middle, fading
                    c.setAlpha(alpha);
                    glow.setColorAt(at, c);
                }
                painter.fillRect(to, glow);
            }

            // Check: a red tint on the king's square (level 1).
            if (m_guidance && inCheck && m_position.mailbox[square] == kingCode) {
                // Check: a red glow behind the king, fading out before the square's edge (as on lichess; Looks).
                const QRect cell(x0 + file * sq, y0 + rank * sq, sq, sq);
                QRadialGradient glow(cell.center(), sq * 0.7);    // centre, radius
                glow.setColorAt(0.0, QColor(255, 0, 0));          // full red in the middle ...
                glow.setColorAt(0.25, QColor(231, 0, 0));
                glow.setColorAt(0.89, QColor(169, 0, 0, 0));      // ... gone (transparent) towards the edge
                painter.fillRect(cell, glow);
            }

            // Coordinates: file letter in the bottom row, rank number in the left column.
            if (rank == 7 || file == 0) {
                painter.save();                                  // font and pen change only here
                QFont small = painter.font();
                small.setPixelSize(sq / 5);
                painter.setFont(small);
                painter.setPen(light ? QColor(181, 136, 99) : QColor(240, 217, 181));   // the other square colour
                const QRect inner = QRect(x0 + file * sq, y0 + rank * sq, sq, sq)
                                        .adjusted(sq / 20, sq / 30, -sq / 20, -sq / 20);   // margins: clear of the
                                                                                            // board's edge line
                const QString name = squareName(square);
                if (rank == 7)
                    painter.drawText(inner, Qt::AlignRight | Qt::AlignBottom, name.left(1));   // "e"
                if (file == 0)
                    painter.drawText(inner, Qt::AlignLeft | Qt::AlignTop, name.mid(1));        // "2"
                painter.restore();
            }

            // Layer 2: the piece (coloured for game end, hover, selection, an illegal attempt, the hint).
            // The model numbers squares a1 = 0 from the bottom; screen row 0 is rank 8.
            const int code = m_position.mailbox[square];
            if (code != 0) { // Index = piece code: Black odd, White even (the model's encoding).
                static const QString glyphs = " ♟♙♞♘♝♗♜♖♛♕♚♔";
                // Game end (every level): the mated king red, both kings neutral on a draw.
                QColor endColour;                                // invalid = no game-end mark here
                if (m_gameState == Position::GameState::Checkmate && code == kingCode)
                    endColour = mateColour;                      // the mated king is the side to move's
                else if (m_gameState != Position::GameState::Ongoing
                         && m_gameState != Position::GameState::Checkmate && (code == 11 || code == 12))
                    endColour = drawColour;
                painter.save();                                  // the pen changes for this glyph only
                painter.setPen(Qt::black);                       // no state: black. Not the painter's default pen:
                                                                 // that is the palette's text colour (light on dark)
                if (endColour.isValid()) {
                    drawGlow(painter, QRect(x0 + file * sq, y0 + rank * sq, sq, sq), QColor(255, 255, 255));
                    painter.setPen(endColour);
                } else if (square == m_hoverSquare && square != m_selSquare && selectable(square)) {
                    // Lingering asks "is it worth a look?": only bad moves -> a grey glow ("switched off").
                    const bool nothingGood = (square == m_lingerSquare && m_pieceVerdicts.value(square, -1) == 0);
                    drawGlow(painter, QRect(x0 + file * sq, y0 + rank * sq, sq, sq),
                             nothingGood ? QColor(110, 110, 110) : QColor(255, 255, 255));
                    painter.setPen(QColor(0, 150, 255));         // hover: "you can select this"
                } else if (square == m_selSquare) {
                    drawGlow(painter, QRect(x0 + file * sq, y0 + rank * sq, sq, sq), QColor(255, 255, 255));
                    painter.setPen(QColor(100, 200, 100));       // selected
                } else if (square == m_illegalSquare) {
                    drawGlow(painter, QRect(x0 + file * sq, y0 + rank * sq, sq, sq), QColor(255, 255, 255));
                    painter.setPen(illegalColour);               // the target of an illegal attempt
                } else if (square == m_hintPiece && !m_viewing) {   // the hint belongs to the live position
                    drawGlow(painter, QRect(x0 + file * sq, y0 + rank * sq, sq, sq), QColor(255, 255, 255));
                    painter.setPen(hintColour);                  // the hint: "this piece"
                } else if (square == replyFrom) {
                    drawGlow(painter, QRect(x0 + file * sq, y0 + rank * sq, sq, sq), QColor(255, 255, 255));
                    painter.setPen(replyColour);                 // the linger flash: "this piece answers"
                }
                // The piece, in two layers (piecepainter.cpp). The pen set above is the state's colour; the default
                // black means no state.
                const QColor pen = painter.pen().color();
                drawPiece(painter, QRect(x0 + file * sq, y0 + rank * sq, sq, sq), code,
                          pen == QColor(Qt::black) ? QColor() : pen);
                // Worth a look: how many correct/playable moves, small, in the square's top-right corner.
                if (square == m_lingerSquare && square != m_selSquare && selectable(square)
                    && m_pieceVerdicts.value(square, 0) > 0) {
                    // On a white badge (a pill as wide as the number needs, a thin dark rim): readable on both
                    // square colours and clear of the piece's top (Anders' pick a, 2026-10-28).
                    const QString count = QString::number(m_pieceVerdicts.value(square));
                    QFont small = painter.font();
                    small.setPixelSize(sq / 5);
                    small.setBold(true);
                    const int h = sq * 3 / 10;                // the badge's height; its width fits the number
                    const int w = std::max(h, QFontMetrics(small).horizontalAdvance(count) + h / 2);
                    const QRect badge(x0 + (file + 1) * sq - w - sq / 30, y0 + rank * sq + sq / 30, w, h);
                    painter.save();
                    painter.setRenderHint(QPainter::Antialiasing);
                    painter.setPen(QPen(QColor(60, 60, 60), sq / 50.0));
                    painter.setBrush(Qt::white);
                    painter.drawRoundedRect(badge, h / 2.0, h / 2.0);   // radius half the height: round ends
                    painter.setFont(small);
                    painter.setPen(Qt::black);
                    painter.drawText(badge, Qt::AlignCenter, count);
                    painter.restore();
                }
                // A hanging piece: a red "!" in the top-left corner (the top-right holds the verdict count).
                if (m_hangingMarks.contains(square) && !m_viewing) {
                    QFont mark = painter.font();
                    mark.setPixelSize(sq / 3);
                    mark.setBold(true);
                    painter.setFont(mark);
                    painter.setPen(QColor(200, 40, 40));          // the "bad" red (Looks: refine)
                    painter.drawText(QRect(x0 + file * sq, y0 + rank * sq, sq, sq).adjusted(sq / 30, 0, 0, 0),
                                     Qt::AlignLeft | Qt::AlignTop, "!");
                }
                if (endColour.isValid()) {                       // a box around the king, in the target box's style
                    painter.setPen(QPen(endColour, sq / 14));
                    painter.setBrush(Qt::NoBrush);
                    const int m = sq / 10;
                    painter.drawRoundedRect(QRect(x0 + file * sq, y0 + rank * sq, sq, sq).adjusted(m, m, -m, -m),
                                            sq / 6, sq / 6);
                }
                painter.restore();
            }

            // Layer 3: boxes on top: the hint's target, then the player's own target box (with its marks).
            // The linger flash's target: where the strongest reply strikes. A thin box OUTSIDE the player's own box, 
            // near the square's edge, so it shows even when the reply captures on the box's square, and stays clear of the (larger) pieces.
            if (square == replyTo) {
                painter.save();
                painter.setPen(QPen(replyColour, sq / 24));
                painter.setBrush(Qt::NoBrush);
                const int m = sq / 30; // near the square's edge: outside your own box (sq / 10), clear of the piece
                painter.drawRoundedRect(QRect(x0 + file * sq, y0 + rank * sq, sq, sq).adjusted(m, m, -m, -m),
                                        sq / 8, sq / 8);
                painter.restore();
            }

            // The hint's target (its second step): a gold box in the target box's shape. Drawn before the
            // player's own box, so that one lands on top.
            if (square == m_hintTarget && !m_viewing) {
                painter.save();
                painter.setPen(QPen(hintColour, sq / 14));
                painter.setBrush(Qt::NoBrush);
                const int m = sq / 10;
                const QRect box = QRect(x0 + file * sq, y0 + rank * sq, sq, sq).adjusted(m, m, -m, -m);
                painter.drawRoundedRect(box, sq / 6, sq / 6);
                // The hinted move from the book: the same sign, in magenta (in practice H follows the opening).
                const QString hint = squareName(m_hintPiece) + squareName(m_hintTarget);
                if (m_hintPiece != -1 && (m_bookMoves.contains(hint) || m_bookMoves.contains(hint + "q")))
                    drawBook(painter, box, sq, hintColour,
                             m_leavingMoves.contains(hint) || m_leavingMoves.contains(hint + "q"));
                painter.restore();
            }

            // The target box: after a selection, on the square under the mouse, if that square is
            // empty or holds an enemy piece. Level 0: black everywhere (reveals nothing).
            const bool ownPiece = code != 0 && (code % 2 == 1) == m_position.pit;
            const bool illegalMark = (square == m_illegalSquare);
            if (illegalMark || (m_selSquare != -1 && square == m_hoverSquare && !ownPiece
                                && (!m_guidance || target[square]))) {   // level 1: legal targets only
                painter.save();
                QColor boxColour = Qt::black;                              // no information: black
                if (illegalMark) {
                    boxColour = illegalColour;
                } else {
                    const QString uci = squareName(m_selSquare) + squareName(square);
                    boxColour = m_targetColours.value(uci,                 // a normal move ...
                                m_targetColours.value(uci + "q", boxColour));   // ... or a promotion (queen's colour)
                }
                painter.setPen(QPen(boxColour, sq / 14));
                painter.setBrush(Qt::NoBrush);
                const int m = sq / 10;                          // margin inside the square
                const QRect box = QRect(x0 + file * sq, y0 + rank * sq, sq, sq).adjusted(m, m, -m, -m);
                painter.drawRoundedRect(box, sq / 6, sq / 6);       // corner radius

                // Marks in the middle of the box's left and right sides: shape tells what colour tells.
                const QString uci = squareName(m_selSquare) + squareName(square);
                const int mark = m_targetMarks.value(uci, m_targetMarks.value(uci + "q", 99));   // 99 = no mark
                if (!illegalMark && mark != 99) {
                    const double r = sq / 12.0;                            // the mark's size, from the square
                    painter.setPen(Qt::NoPen);                             // no outline: just the filled shape
                    painter.setBrush(boxColour);
                    for (const int x : {box.left(), box.right()}) {
                        const QPointF c(x, box.center().y());
                        if (mark == 0) {
                            painter.drawEllipse(c, r, r);                  // ● playable
                        } else {
                            const double d = (mark > 0) ? -r : r;          // ▲ apex up (correct), ▼ apex down (bad)
                            painter.drawPolygon(QPolygonF({QPointF(c.x(), c.y() + d),
                                                           QPointF(c.x() - r, c.y() - d),
                                                           QPointF(c.x() + r, c.y() - d)}));
                        }
                    }
                }
                // A book move: the open book on the box's top edge (hollow if it leaves the practised opening).
                if (!illegalMark && (m_bookMoves.contains(uci) || m_bookMoves.contains(uci + "q")))
                    drawBook(painter, box, sq, boxColour,
                             m_leavingMoves.contains(uci) || m_leavingMoves.contains(uci + "q"));
                painter.restore();
            }
        }
    }

    // The board's edge (Looks, B): a thin dark-wood line, drawn inside the squares, so it needs no extra room.
    painter.setPen(QPen(QColor(90, 60, 35), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(x0, y0, 8 * sq, 8 * sq).adjusted(1, 1, -1, -1));   // the pen is centred on its line:
                                                                                  // 1 px in = the line's 2 px inside
    // The preview frame: a line is being played through, not the game.
    if (m_preview) {
        painter.setPen(QPen(previewColour, qMax(3, sq / 12)));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(board.adjusted(1, 1, -1, -1));
    }

    // Paused in a timed game: the position is hidden under a veil, "Paused" in the middle.
    if (m_veil) {
        QColor veil = palette().color(QPalette::Window);
        veil.setAlpha(235);                      // almost opaque: the pieces are no longer readable
        painter.fillRect(QRect(x0, y0, 8 * sq, 8 * sq), veil);
        QFont big = painter.font();
        big.setPixelSize(sq / 2);
        big.setBold(true);
        painter.setFont(big);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(QRect(x0, y0, 8 * sq, 8 * sq), Qt::AlignCenter, "Paused");
    }

    // The game-end banner: a dark, semi-transparent band over the middle two ranks, edged in the outcome's colour.
    // The same in light and dark (previewed: A).
    if (!m_bannerTitle.isEmpty()) {
        const QRect band(x0, y0 + 3 * sq - sq / 10, 8 * sq, 2 * sq + sq / 5);
        painter.fillRect(band, QColor(20, 20, 24, 190));
        const int edge = qMax(3, sq / 16);
        painter.fillRect(QRect(band.left(), band.top(), band.width(), edge), m_bannerTone);
        painter.fillRect(QRect(band.left(), band.bottom() - edge + 1, band.width(), edge), m_bannerTone);
        painter.setPen(Qt::white);
        QFont title = painter.font();
        title.setPixelSize(sq * 2 / 5);
        title.setBold(true);
        painter.setFont(title);
        const QRect top = band.adjusted(sq / 4, 0, -sq / 4, -band.height() / 3);   // the upper two thirds
        painter.drawText(top, Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWordWrap, m_bannerTitle);
        QFont small = title;
        small.setPixelSize(sq / 4);
        small.setBold(false);
        painter.setFont(small);
        const QRect bottom = band.adjusted(0, band.height() * 2 / 3 + sq / 10, 0, 0);   // the lower third
        painter.drawText(bottom, Qt::AlignHCenter | Qt::AlignTop, m_bannerDetail);
    }
}

// The "flashlight": a soft white spot behind a glyph (hover, selection, illegal, game end, hint).
void BoardWidget::drawGlow(QPainter &painter, const QRect &cell, QColor colour) {
    QRadialGradient glow(cell.center(), cell.width() / 2.0);   // centre and radius
    colour.setAlpha(170);
    glow.setColorAt(0.0, colour);        // strong in the centre
    colour.setAlpha(0);
    glow.setColorAt(1.0, colour);        // fully transparent at the radius
    painter.fillRect(cell, glow);        // a gradient works as a brush
}

// A ring of light. Not used: kept from the glow experiments for the Looks phase.
void BoardWidget::drawHalo(QPainter &painter, const QRect &cell, QColor colour) {
    QRadialGradient halo(cell.center(), cell.width() / 2.0);
    colour.setAlpha(0);
    halo.setColorAt(0.0, colour);        // clear centre
    halo.setColorAt(0.5, colour);        // still clear halfway out
    colour.setAlpha(200);
    halo.setColorAt(0.8, colour);        // the bright ring
    colour.setAlpha(0);
    halo.setColorAt(1.0, colour);        // fades out before the square's edge
    painter.fillRect(cell, halo);
}

// =================================================================================================
// 4. MOUSE INPUT: press, move, leave
// =================================================================================================
// Called by Qt when a mouse button is pressed over the widget.
// Two-click move: first click selects a piece, second click reports the move to the Game,
// if it's legal.
// Pattern: event -> change state -> update() -> Qt calls paintEvent later.
void BoardWidget::mousePressEvent(QMouseEvent *event) {
    if (!m_bannerTitle.isEmpty()) {     // the game-end banner: a click anywhere on the board puts it away, nothing else
        setBanner(QString());
        return;
    }
    const int square = squareAtPixel(event->position().toPoint());   // QPointF -> whole pixels
    if (square == -1)
        return;                                     // click outside the board: ignore
    // The board editor: every click is reported, left or right; the editor decides what it means.
    if (m_editMode) {
        emit editClicked(square, event->button() == Qt::RightButton);
        return;
    }

    // The game decides when input is allowed (human to move, game not over).
    if (!m_inputEnabled)
        return;
    if (m_viewing)
        return;                // an earlier position: look, don't touch
    if (selectable(square)) {
        // Touch-move: a touched piece that can move must be the one moved; another piece is refused.
        if (m_touchMove && m_touched != -1 && square != m_touched && canMove(m_touched)) {
            emit touchRefused(m_touched);
            return;
        }
        // A piece of the side to move: select it. That also switches from another selected
        // piece, and clicking the selected piece again changes nothing (no deselect).
        m_selSquare = square;
        if (m_touchMove)
            m_touched = square;                   // touched (again, or anew: the old piece had no legal move)
        m_illegalSquare = -1;                     // a new selection clears the violet mark
        emit pieceSelected(square);

    } else if (m_selSquare != -1) {
        if (m_guidance && !isTarget(square))
            return;                     // level 1: no box here, so the click does nothing (selection kept)
        {
            // second click (an empty square or an enemy piece): ask the model whether this move is legal
            std::string uci = (squareName(m_selSquare) + squareName(square)).toStdString();
            std::optional<Position::Move> move = m_model.UciToMove(m_position, uci);
            if (!move && m_model.UciToMove(m_position, uci + "q")) {   // a legal promotion: ask for the piece
                const QString piece = choosePromotion(event->globalPosition().toPoint());
                if (!piece.isEmpty())
                    move = m_model.UciToMove(m_position, uci + piece.toStdString());
            }
            if (move) {
                // Report the (legal) move; the Game makes it and sends back the new position.
                emit moveMade(QString::fromStdString(m_model.MoveToUci(*move)));
            } else if (!m_model.UciToMove(m_position, uci + "q")) {
                m_illegalSquare = square;          // illegal (not a cancelled promotion): mark it violet
                emit illegalAttempt(square);
            }
            if (move || !m_touchMove)
                m_selSquare = -1;                  // drop the selection; touch-move: an illegal attempt keeps it
        }
    }
    updateBoxMove();
    update();   // request a repaint (queued; several requests merge into one paint)
}

// Called by Qt when the mouse moves over the widget.
void BoardWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_editMode)
        return;                     // editing: no hover glow, no box, no linger
    const int square = squareAtPixel(event->position().toPoint());
    if (square == m_hoverSquare)
        return;                     // still the same square: nothing to redraw
    m_hoverSquare = square;
    restartLinger(square);
    if (square != -1 && selectable(square))
        m_illegalSquare = -1;       // looking for the next piece: the violet has done its job
    updateBoxMove();
    update();
}

// Called by Qt when the mouse leaves the widget: nothing is hovered any more.
void BoardWidget::leaveEvent(QEvent *) {
    m_hoverSquare = -1;
    restartLinger(-1);
    updateBoxMove();
    update();
}

// The mouse reached another square (or left the board): a linger ends, and the wait starts again.
void BoardWidget::restartLinger(int square) {
    m_deepTimer.stop();                  // moving on also ends level 2
    if (m_deepLinger) {
        m_deepLinger = false;
        emit lingeredOnMove(QString());
    }
    if (m_lingerSquare != -1) {
        m_lingerSquare = -1;
        emit lingerChanged(-1);
    }
    if (square != -1)
        m_lingerTimer.start();   // restarts it if it was already running
    else
        m_lingerTimer.stop();
}

// =================================================================================================
// 5. STATE FROM OUTSIDE: position, last move, flip, guidance, viewing, input
// =================================================================================================
void BoardWidget::setPosition(const Position::Chess &position) {
    m_position = position;
    m_illegalSquare = -1;   // the violet mark belonged to the old position
    updateBoxMove();
    update();
}

void BoardWidget::setLastMove(int from, int to) {
    m_lastFrom = from;
    m_lastTo = to;
    update();
}

void BoardWidget::setFlipped(bool flipped) {
    m_flipped = flipped;
    update();
}

void BoardWidget::setGuidance(bool on) {
    m_guidance = on;
    update();
}

void BoardWidget::setViewing(bool viewing) {
    m_viewing = viewing;
    m_selSquare = -1;          // no half-made move survives a change of view
    m_illegalSquare = -1;
    updateBoxMove();
    update();
}

void BoardWidget::setInputEnabled(bool enabled) {
    m_inputEnabled = enabled;
    m_selSquare = -1;             // drop any half-made selection ...
    m_touched = -1;               // ... and the touch with it: a new turn, a new game, or the game resting
    updateBoxMove();
    update();
}

// =================================================================================================
// 6. THE PROMOTION CHOOSER
// =================================================================================================
// A small pop-up menu at the mouse. exec() runs a nested event loop until the player picks
// an entry, or cancels (Esc, click outside) -> nullptr.
// The pieces as the board draws them (piecePixmap), at 32 px: the "menuIconSize" property asks the app's style for
// bigger menu icons (theme.cpp), and a larger font keeps the rows as tall as the pieces.
QString BoardWidget::choosePromotion(const QPoint &globalPos) {
    QMenu menu(this);
    const int iconSize = 32;
    menu.setProperty("menuIconSize", iconSize);
    QFont font = menu.font();
    font.setPointSizeF(font.pointSizeF() * 1.35);
    menu.setFont(font);
    const bool black = m_position.pit;   // the side promoting is the side to move
    const struct { int code; const char *name; const char *uci; } pieces[] = {   // White's codes; Black's one less
        {10, "Queen", "q"}, {8, "Rook", "r"}, {6, "Bishop", "b"}, {4, "Knight", "n"}};
    for (const auto &piece : pieces) {
        const int code = piece.code - (black ? 1 : 0);
        const QPixmap pixmap = piecePixmap(code, 2 * iconSize, panelRim(menu.palette(), code));   // twice: sharp
        menu.addAction(QIcon(pixmap), piece.name)->setData(piece.uci);
    }
    QAction *chosen = menu.exec(globalPos);
    return chosen ? chosen->data().toString() : QString();
}

// =================================================================================================
// 7. COACHING ON THE BOARD: box colours, the box move, marks, game end, the hint
// =================================================================================================
void BoardWidget::setTargetColours(const QHash<QString, QColor> &colours) {
    m_targetColours = colours;
    update();
}

// The same conditions as the box in paintEvent: a selection, a hovered square that isn't
// an own piece, and at level 1 only legal targets.
QString BoardWidget::boxMove() const {
    if (m_selSquare == -1 || m_hoverSquare == -1)
        return QString();
    const int code = m_position.mailbox[m_hoverSquare];
    if (code != 0 && (code % 2 == 1) == m_position.pit)
        return QString();                                  // own piece: no box
    if (m_guidance && !isTarget(m_hoverSquare))
        return QString();                                  // level 1: no box off the legal targets
    return squareName(m_selSquare) + squareName(m_hoverSquare);
}

// Announce the box move (boxMoveChanged) only when it has changed.
void BoardWidget::updateBoxMove() {
    const QString move = boxMove();
    if (move != m_boxMove) {
        m_boxMove = move;
        emit boxMoveChanged(move);
    }
}

void BoardWidget::setTargetMarks(const QHash<QString, int> &marks) {
    m_targetMarks = marks;
    update();
}

void BoardWidget::setBookMoves(const QSet<QString> &moves, const QSet<QString> &leaving) {
    m_bookMoves = moves;
    m_leavingMoves = leaving;
    update();
}

void BoardWidget::setGameState(Position::GameState state) {
    m_gameState = state;
    update();
}

void BoardWidget::setHint(int pieceSquare, int targetSquare) {
    m_hintPiece = pieceSquare;
    m_hintTarget = targetSquare;
    update();
}

void BoardWidget::setPieceVerdicts(const QHash<int, int> &verdicts) {
    m_pieceVerdicts = verdicts;
    update();
}

void BoardWidget::setTargetReplies(const QHash<QString, QString> &replies) {
    m_targetReplies = replies;
    update();
}

void BoardWidget::setThreat(const QString &move) {
    m_threat = move;
    update();
}

void BoardWidget::setHangingMarks(const QList<int> &squares) {
    m_hangingMarks = squares;
    update();
}

void BoardWidget::setLingerTime(int ms) {
    m_lingerTimer.setInterval(ms);
    m_deepTimer.setInterval(ms);     // level 2: the same time again
}

void BoardWidget::setPreview(bool on) {
    m_preview = on;
    update();
}

void BoardWidget::setEditMode(bool on) {
    m_editMode = on;
    m_selSquare = -1;                   // nothing selected, hovered or marked from the game
    m_hoverSquare = -1;
    m_illegalSquare = -1;
    restartLinger(-1);
    updateBoxMove();
    update();
}

void BoardWidget::setTouchMove(bool on) {
    if (on == m_touchMove)
        return;                       // no change: a touch in progress stays (callers may repeat themselves)
    m_touchMove = on;
    m_touched = -1;
}

void BoardWidget::setVeil(bool on) {
    m_veil = on;
    update();
}

void BoardWidget::setBanner(const QString &title, const QString &detail, const QColor &tone) {
    m_bannerTitle = title;
    m_bannerDetail = detail;
    m_bannerTone = tone;
    update();
}

