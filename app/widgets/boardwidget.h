// boardwidget.h — the chess board on screen.
//
// A custom widget: it draws the board itself (paintEvent) and turns mouse clicks into
// moves (mousePressEvent). The chess rules come from the board model (Position): the
// widget only accepts legal moves. It talks to the outside world in UCI text:
//   out: moveMade ("e2e4") when the user moves a piece; boxMoveChanged (the move under the box)
//   in:  the position, input on/off, viewing, guidance from the Game; colours, marks, the hint
//        from the coaching (see the public functions)

#pragma once
#include <QWidget>
#include <QHash>
#include <QSet>
#include <QTimer>
#include "position.h"
// Only <QWidget> here: <QPainter>, <QMouseEvent> etc. are needed by the .cpp alone,
// so files that include this header don't pay for them.

class QPainter;   // only a reference here: the full class is needed in the .cpp alone

class BoardWidget : public QWidget   // a widget *is* a QObject, so signals/slots work
{
    Q_OBJECT                         // needed for signals (moc generates their bodies)

signals:
    // Emitted after the user has moved a piece with two clicks. Argument in UCI format
    // (from-square + to-square, e.g. "e2e4"). No body: moc writes it.
    void moveMade(const QString &uciMove);

    // The move under the target box changed ("" = no box shown), e.g. for the eval bar's preview.
    void boxMoveChanged(const QString &uci);

    // The mouse has stayed a longer time on this square ("lingering" = asking); -1 = it moved away again.
    void lingerChanged(int square);

    // The player selected this piece (a click on an own piece). For the hint: selecting another piece ends it.
    void pieceSelected(int square);

    // Lingering longer (level 2) with the box on a move: that move; "" = the linger ended. For the line text.
    void lingeredOnMove(const QString &move);

    // Edit mode (the board editor): a square was clicked; right = with the right button (clear it).
    void editClicked(int square, bool right);

    // Touch-move: another piece was clicked while this one is touched (and can move).
    void touchRefused(int touched);

    // A click on a square the selected piece can't legally reach (the violet mark). Tournament counts these.
    void illegalAttempt(int square);


public:
    // --- Construction ----------------------------------------------------------------------------
    // `explicit`: no silent conversion from a QWidget* to a BoardWidget (as in Engine).
    explicit BoardWidget(QWidget *parent = nullptr);

    // A square in UCI text -> model square (a1 = 0). The square starts at `at`: the move's from-square at 0,
    // its to-square at 2. E.g. uciSquare("g1f3", 0) = 6 (g1), uciSquare("g1f3", 2) = 21 (f3).
    static int uciSquare(const QString &uci, int at) {
        return (uci.at(at + 1).toLatin1() - '1') * 8 + (uci.at(at).toLatin1() - 'a');
    }

    // --- From the Game: input, position, last move, flip, guidance, viewing ----------------------
    // The game switches input on (human to move) and off (engine thinking, game over).
    void setInputEnabled(bool enabled);

    // Show this position (the game owns the real one; the board shows a copy).
    void setPosition(const Position::Chess &position);

    // Highlight the last move (model squares, a1 = 0); -1, -1 = no highlight.
    void setLastMove(int from, int to);

    // Show the board from Black's side (true) or White's side (false).
    void setFlipped(bool flipped);
    
    bool isFlipped() const { return m_flipped; }   // Black at the bottom? (the board editor's Flip toggles it)

    // Guidance on (level 1) or off (level 0): decides what the board reveals about the position.
    void setGuidance(bool on);

    // Read-only while an earlier position is shown: no hover, no selection, no moves.
    void setViewing(bool viewing);

    // --- From the coaching: box colours, marks, game end, the hint -------------------------------
    // Box colours for moves (UCI text -> colour), e.g. from the coach. Moves not in the table: black box.
    void setTargetColours(const QHash<QString, QColor> &colours);

    // Marks on the box's sides (UCI text -> +1 = up, 0 = circle, -1 = down), e.g. for colour-blind players.
    void setTargetMarks(const QHash<QString, int> &marks);

    // Book moves (UCI text): the box on one gets a small open book on its top edge, in the box's colour; hollow (white
    // pages, the colour as their edge) for the ones in `leaving`: book moves that leave the practised opening.
    void setBookMoves(const QSet<QString> &moves, const QSet<QString> &leaving = {});

    // How the game stands (from the Game: the board can't see repetition). Shown at every level:
    // mate = the mated king red, a draw = both kings neutral. Ongoing = no marks.
    void setGameState(Position::GameState state);

    // The hint (from outside): the piece's square in gold; with a target (the hint's second step) also a
    // gold box there. -1 = none.
    void setHint(int pieceSquare, int targetSquare);

    // Piece verdicts for lingering: square -> how many correct/playable moves that piece has (0 = only bad
    // moves). Squares not in the table: no verdict yet. Shown while the mouse lingers on the piece.
    void setPieceVerdicts(const QHash<int, int> &verdicts);

    // The strongest reply to each move (UCI -> UCI), for the linger flash: lingering with the box on a move
    // lights up the enemy piece that answers and the square it strikes. Moves not in the table: no flash.
    void setTargetReplies(const QHash<QString, QString> &replies);

    // Buster's threat (UCI, from outside), drawn like the linger flash: the enemy piece and the square it
    // strikes. Shown when no linger flash is, live position only. "" = none.
    void setThreat(const QString &move);

    // Hanging pieces (from outside; attack_design.md): a small red "!" in the top-left corner of each square.
    // Live position only. Empty = none.
    void setHangingMarks(const QList<int> &squares);

    // The linger time (a personal setting, from outside): how long the mouse must stay on a square. The second
    // level (the line) waits the same time again. Default 1500 ms.
    void setLingerTime(int ms);

    // A preview is playing (a line shown on the board, not the game): a frame around the board says so.
    void setPreview(bool on);

    // Edit mode (the board editor): clicks are reported as editClicked instead of moves; no hover, no linger.
    void setEditMode(bool on);

    // Touch-move (Tournament, release_design.md 8): the first piece selected this turn must be moved if it can;
    // other pieces are refused, and an illegal attempt keeps it selected.
    void setTouchMove(bool on);

    // Paused in a timed game: a veil over the board with "Paused" (no thinking on the clock's time).
    void setVeil(bool on);

    // The game-end banner (release_design.md 6): a band across the middle with the headline and a detail line,
    // edged in the outcome's colour. An empty title = no banner.
    void setBanner(const QString &title, const QString &detail = QString(), const QColor &tone = QColor());

protected:
    // Event handlers: Qt calls these, never our own code. `override` makes the compiler
    // check that they really replace QWidget's virtual functions.
    void paintEvent(QPaintEvent *) override;             // "draw yourself now"
    void mousePressEvent(QMouseEvent *event) override;   // "a mouse button went down"
    void mouseMoveEvent(QMouseEvent *event) override;    // "the mouse moved"
    void leaveEvent(QEvent *event) override;              // "the mouse left the widget"

private:
    // --- The rules and the position --------------------------------------------------------------
    // The rules (legal moves, UCI text), used to check clicks before reporting them.
    Position m_model;
    // A copy of the game's position, for drawing and checking clicks (set by setPosition).
    // Model numbering: a1 = 0, rank-major.
    Position::Chess m_position;   // empty until the Game calls setPosition()

    // --- Geometry: pixels <-> squares ------------------------------------------------------------
    // The board's pixel rectangle inside the widget: as large as fits (square), centred.
    // The ONE place the geometry is computed, so drawing and clicking always agree.
    // Integer division may leave a few spare pixels; they are split between both sides.
    QRect boardRect() const {
        const int sq = qMin(width(), height()) / 8;     // square size in pixels
        const int x0 = (width()  - 8 * sq) / 2;         // left edge of the board
        const int y0 = (height() - 8 * sq) / 2;         // top edge of the board
        return QRect(x0, y0, 8 * sq, 8 * sq);
    }

    // Screen row/column (row 0 = top) -> model square (a1 = 0). The ONE place that knows
    // how the board is turned on screen (drawing and clicking both use it).
    int squareAt(int row, int col) const {
        return m_flipped ? row * 8 + (7 - col)     // Black's view: top-left = h1
                         : (7 - row) * 8 + col;    // White's view: top-left = a8
    }

    // Widget pixel -> model square, or -1 outside the board. The reverse of drawing;
    // used by clicks and hover.
    int squareAtPixel(const QPoint &p) const {
        const QRect board = boardRect();
        if (!board.contains(p))
            return -1;
        const int sq = board.width() / 8;
        return squareAt((p.y() - board.y()) / sq, (p.x() - board.x()) / sq);
    }

    // --- What can be selected or targeted; square names ------------------------------------------
    // Does the piece on this square have at least one legal move?
    bool canMove(int square) const {
        for (const Position::Move &m : m_model.LegalMoves(m_position).moves)
            if (m.from_square == square)
                return true;
        return false;
    }

    // Is this square a legal target for the selected piece?
    bool isTarget(int square) const {
        for (const Position::Move &m : m_model.LegalMoves(m_position).moves)
            if (m.from_square == m_selSquare && m.to_square == square)
                return true;
        return false;
    }

    // Can a click select the piece on this square? Hover lights up exactly these.
    bool selectable(int square) const {
        const int code = m_position.mailbox[square];
        return !m_viewing && m_inputEnabled && code != 0 && (code % 2 == 1) == m_position.pit
               && (!m_guidance || canMove(square));    // level 1: only pieces that can move
    }

    // Model square -> chess name, e.g. 12 -> "e2". Same on a flipped board.
    static QString squareName(int square) {
        return QString(QChar('a' + square % 8)) + QString::number(square / 8 + 1);
    }

    // --- Interaction state: selection, hover, the illegal mark -----------------------------------
    // The currently selected square (first click).
    int m_selSquare = -1;  // the selected square (model numbering), -1 = nothing selected

    int m_hoverSquare = -1;   // the square under the mouse (model numbering), -1 = none
    int m_illegalSquare = -1;   // the last illegal target clicked, shown violet; -1 = none

    // --- Display state: last move, input, flip, guidance -----------------------------------------
    int m_lastFrom = -1;   // last move's squares (model numbering), -1 = none
    int m_lastTo = -1;

    bool m_inputEnabled = true;   // the human plays White, who moves first

    bool m_flipped = false;   // true: Black at the bottom

    bool m_guidance = true;   // on at start (level 1)

    bool m_editMode = false;          // the board editor is open (setEditMode)
    
    bool m_touchMove = false;         // touch-move is on (setTouchMove)
    int m_touched = -1;               // the touched piece's square this turn; -1 = none yet


    // --- The promotion chooser and the drawing helpers -------------------------------------------
    // Ask the player which piece to promote to; "q"/"r"/"b"/"n", or "" if cancelled.
    QString choosePromotion(const QPoint &globalPos);

    // A soft "flashlight" spot on a square: colour in the centre, fading to transparent.
    void drawGlow(QPainter &painter, const QRect &cell, QColor colour);

    // A ring of light: clear in the centre, bright at about 80% of the radius, clear at the edge.
    // Not used at the moment: kept from the glow experiments (2026-10-11) for the Looks phase.
    void drawHalo(QPainter &painter, const QRect &cell, QColor colour);

    // --- Viewing and coaching state --------------------------------------------------------------
    bool m_viewing = false;   // an earlier position is shown (set by the Game)

    QHash<QString, QColor> m_targetColours;   // empty = plain black boxes

    QString m_boxMove;              // the move under the box, as last announced
    QString boxMove() const;        // the move the box shows right now ("" = none)
    void updateBoxMove();           // announce it if it changed

    QHash<QString, int> m_targetMarks;   // empty = no marks
    QSet<QString> m_bookMoves;   // empty = no book signs
    QSet<QString> m_leavingMoves;   // of those: the ones that leave the practised opening (drawn hollow)

    Position::GameState m_gameState = Position::GameState::Ongoing;

    int m_hintPiece = -1;    // the hint's piece square, -1 = no hint
    int m_hintTarget = -1;   // the hint's target square (second step), -1 = not shown

    bool m_preview = false;        // a line is being played through: draw the preview frame

    bool m_veil = false;

    QString m_bannerTitle;         // the game-end banner; empty = none
    QString m_bannerDetail;
    QColor m_bannerTone;

    // --- Lingering ---------------------------------------------------------------------------------
    QTimer m_lingerTimer;          // single-shot: runs out when the mouse has stayed on one square
    int m_lingerSquare = -1;       // the square being lingered on, -1 = none
    QHash<int, int> m_pieceVerdicts;   // empty = no verdicts (not scored yet, or switched off)
    void restartLinger(int square);   // a new square (or off the board): end any linger, wait again
    QHash<QString, QString> m_targetReplies;   // move -> strongest reply; empty = no flash
    QString m_threat;              // Buster's threat ("" = none): the flash without a linger
    QList<int> m_hangingMarks;     // squares with a hanging piece: the red "!" (empty = none)
    QTimer m_deepTimer;            // single-shot: the second linger level, after another wait
    bool m_deepLinger = false;     // lingeredOnMove has announced a move (end it with "")


};
