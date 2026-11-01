// helppics <out folder> [dark]: cuts the Help's pictures (app/help/images/) from the real widgets: an offscreen board,
// eval bar, graph, dot and coach cards, put into each state the way the app does it (mouse events, setters), at twice
// the size (QT_SCALE_FACTOR 2: sharp at any zoom). Every picture is one block below; the help shows it at half width.
#include <QApplication>
#include <QFontDatabase>
#include <QMouseEvent>
#include "boardwidget.h"
#include "piecepainter.h"
#include "evalbar.h"
#include "wingraph.h"
#include "confidencedot.h"
#include "theme.h"
#include "position.h"
#include "openings.h"
#include "practicepanel.h"
#include <QFile>
#include <QToolButton>
#include <QTreeWidget>
#include <QRadioButton>
#include <QStyle>
#include <QLabel>
#include <QVBoxLayout>
#include <QElapsedTimer>
#include <QPainter>
#include <QHeaderView>
#include <QLineEdit>
#include <QTableWidget>
#include <QToolTip>

static void wait(int ms) { QElapsedTimer t; t.start(); while (t.elapsed() < ms) QApplication::processEvents(); }
// The coach line's cards, as coaching.cpp draws them (updateCoachLine): edge colour, background, padding.
static QPixmap cards(const QList<QPair<QString, QString>> &rows, int width) {
    auto *w = new QWidget;
    auto *col = new QVBoxLayout(w);
    col->setContentsMargins(0, 6, 0, 0);
    col->setSpacing(4);
    for (const auto &[edge, text] : rows) {
        auto *card = new QLabel(text);
        card->setWordWrap(true);
        card->setStyleSheet(QString("QLabel { border-left: 4px solid %1; background: rgba(128, 128, 128, 12%);"
                                    " padding: 2px 6px;%2 }").arg(edge, edge == "gray" ? " color: gray;" : ""));
        col->addWidget(card);
    }
    w->setFixedWidth(width);
    w->adjustSize();
    w->show();
    QApplication::processEvents();
    return w->grab();
}
static bool flipped = false;      // the board seen from Black's side (setFlipped): squares sit mirrored on screen
// files f0..f1, ranks r0..r1 (0-based, a1 = file 0 rank 0), as a picture of those squares
static QPixmap grabBoard(BoardWidget *b, int f0, int f1, int r0, int r1) {
    QApplication::processEvents();
    const int s = 2 * 60;
    const int x = flipped ? 7 - f1 : f0, y = flipped ? r0 : 7 - r1;
    return b->grab().copy(x * s, y * s, (f1 - f0 + 1) * s, (r1 - r0 + 1) * s);
}
// A picture above, the cards under it, on the window's background.
static void stack(const QPixmap &top, const QPixmap &bottom, const QString &file) {
    QPixmap out(qMax(top.width(), bottom.width()), top.height() + bottom.height());
    out.setDevicePixelRatio(1);
    out.fill(QApplication::palette().color(QPalette::Window));
    QPainter p(&out);
    QPixmap t = top, b = bottom;
    t.setDevicePixelRatio(1); b.setDevicePixelRatio(1);
    p.drawPixmap(0, 0, t);
    p.drawPixmap(0, t.height(), b);
    p.end();
    out.save(file);
}

static QString out;
static const int S = 60;          // logical square; ×2 scale = 120 px

static QPoint at(int sq) {
    const int col = flipped ? 7 - sq % 8 : sq % 8, row = flipped ? sq / 8 : 7 - sq / 8;
    return QPoint(col * S + S / 2, row * S + S / 2);
}
static void send(QWidget *w, QEvent::Type t, int sq) {
    const QPoint p = at(sq);
    QMouseEvent e(t, p, w->mapToGlobal(p), t == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                  t == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &e);
}
// files f0..f1, ranks r0..r1 (0-based, White's view), into a png
static void cut(BoardWidget *b, int f0, int f1, int r0, int r1, const QString &name) {
    QApplication::processEvents();
    QPixmap p = b->grab();
    const int s = 2 * S;
    p.copy(f0 * s, (7 - r1) * s, (f1 - f0 + 1) * s, (r1 - r0 + 1) * s).save(out + name);
}
static BoardWidget *board(const char *fen) {
    auto *b = new BoardWidget;
    b->resize(8 * S, 8 * S);
    b->setPosition(*Position().FenInput(fen));
    b->setInputEnabled(true);
    b->setGuidance(true);
    b->show();
    QApplication::processEvents();
    return b;
}

int main(int argc, char *argv[])
{
    qputenv("QT_SCALE_FACTOR", "2");
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont(":/fonts/BusterPieces.ttf");
    applyTheme(argc > 2 && QString(argv[2]) == "dark");
    out = QString(argv[1]) + "/";
    const char *start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const int g1 = 6, f3 = 21, g3 = 22;

    // The box in its four colours: g1 picked up, the box on f3 (black: no verdict).
    const struct { const char *name; QColor colour; } plain[] = {
        {"box_green.png", QColor(60, 180, 75)}, {"box_amber.png", QColor(230, 160, 30)},
        {"box_red.png", QColor(200, 40, 40)}, {"box_black.png", QColor()}};
    for (const auto &x : plain) {
        BoardWidget *b = board(start);
        if (x.colour.isValid())
            b->setTargetColours({{"g1f3", x.colour}});
        send(b, QEvent::MouseButtonPress, g1);
        send(b, QEvent::MouseMove, f3);
        cut(b, 5, 6, 0, 2, x.name);
    }
    { // the book: an amber box with the open book
        BoardWidget *b = board(start);
        b->setTargetColours({{"g1f3", QColor(230, 160, 30)}});
        b->setBookMoves({"g1f3"});
        send(b, QEvent::MouseButtonPress, g1);
        send(b, QEvent::MouseMove, f3);
        cut(b, 5, 6, 0, 2, "book.png");
    }
    { // the hint, step 2: the piece and its target
        BoardWidget *b = board(start);
        b->setHint(g1, f3);
        cut(b, 5, 6, 0, 2, "hint.png");
    }
    { // the threat: the c4 knight would take the queen on e3
        BoardWidget *b = board("4k3/8/8/8/2n5/4Q3/8/4K3 w - - 0 1");
        b->setThreat("c4e3");
        cut(b, 2, 4, 2, 3, "threat.png");
    }

    // The box with colour-blind marks: g1 picked up, the box on f3.
    const struct { const char *name; QColor colour; int mark; } boxes[] = {
        {"marks_green.png", QColor(60, 180, 75), 1}, {"marks_amber.png", QColor(230, 160, 30), 0},
        {"marks_red.png", QColor(200, 40, 40), -1}};
    for (const auto &x : boxes) {
        BoardWidget *b = board(start);
        b->setTargetColours({{"g1f3", x.colour}});
        b->setTargetMarks({{"g1f3", x.mark}});
        send(b, QEvent::MouseButtonPress, g1);
        send(b, QEvent::MouseMove, f3);
        cut(b, 5, 6, 0, 2, x.name);
    }
    { // the hollow book
        BoardWidget *b = board(start);
        b->setTargetColours({{"g1f3", QColor(60, 180, 75)}});
        b->setBookMoves({"g1f3"}, {"g1f3"});
        send(b, QEvent::MouseButtonPress, g1);
        send(b, QEvent::MouseMove, f3);
        cut(b, 5, 6, 0, 2, "book_hollow.png");
    }
    { // your pieces: blue under the mouse, green picked up
        BoardWidget *b = board(start);
        send(b, QEvent::MouseMove, g1);
        cut(b, 5, 6, 0, 2, "piece_hover.png");
        send(b, QEvent::MouseButtonPress, g1);
        cut(b, 5, 6, 0, 2, "piece_picked.png");
    }
    { // the hint, step 1: the piece only
        BoardWidget *b = board(start);
        b->setHint(g1, -1);
        cut(b, 5, 6, 0, 2, "hint_piece.png");
    }
    { // violet: an illegal try (guidance off)
        BoardWidget *b = board(start);
        b->setGuidance(false);
        send(b, QEvent::MouseButtonPress, g1);
        send(b, QEvent::MouseMove, g3);
        send(b, QEvent::MouseButtonPress, g3);
        cut(b, 5, 6, 0, 2, "illegal.png");
    }
    { // the last move: e2-e4
        BoardWidget *b = board("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
        b->setInputEnabled(false);
        b->setLastMove(12, 28);
        cut(b, 3, 5, 1, 3, "lastmove.png");
    }
    { // check: 3.Bxf7+ (1.e4 e5 2.Bc4 Nf6)
        BoardWidget *b = board("rnbqkb1r/pppp1Bpp/5n2/4p3/4P3/8/PPPP1PPP/RNBQK1NR b KQkq - 0 3");
        b->setInputEnabled(false);
        b->setLastMove(26, 53);
        cut(b, 3, 5, 5, 7, "check.png");
    }
    { // hanging: the knight on e5, attacked by the d6 pawn, not defended
        BoardWidget *b = board("4k3/8/3p4/4N3/8/8/8/4K3 w - - 0 1");
        b->setHangingMarks({36});
        cut(b, 3, 5, 4, 5, "hanging.png");
    }
    { // the end of a game: fool's mate, the whole board
        const char *fen = "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3";
        BoardWidget *b = board(fen);
        b->setInputEnabled(false);
        b->setLastMove(59, 31);
        b->setGameState(Position::GameState::Checkmate);
        b->setBanner("Checkmate: Buster wins.", "0-1 after 2 moves", QColor(190, 70, 70));
        cut(b, 0, 7, 0, 7, "gameend.png");
    }
    { // the eval bar
        auto *bar = new EvalBar;
        bar->resize(48, 8 * S);
        bar->setPlayerWhite(true);
        bar->setWinChance(0.62);
        bar->show();
        QApplication::processEvents();
        bar->grab().save(out + "evalbar.png");
        bar->setPreview(0.41, 0.21);
        QApplication::processEvents();
        bar->grab().save(out + "evalbar_red.png");
        bar->setPreview(0.62, 0.0);
        QApplication::processEvents();
        bar->grab().save(out + "evalbar_green.png");
    }
    { // the graph: a made-up game, two red moves of yours
        auto *g = new WinGraph;
        g->resize(320, 100);
        QList<double> v; QList<bool> red;
        const double curve[] = {.5,.52,.51,.53,.55,.54,.56,.55,.58,.57,.60,.59,.47,.48,.50,.52,.55,.54,.57,.58,
                                .60,.62,.61,.63,.66,.65,.64,.38,.40,.42,.45,.44,.46,.45,.43,.42,.44,.46,.48,.47,.50,.52,.51,.53,
                                .55,.54,.56,.58,.57,.60,.59,.62,.61,.64,.63,.66,.68,.67,.70,.72,.71,.74,.76,.79,.81,.84,
                                .86,.88,.90,.92,.94,.96};
        for (size_t i = 0; i < sizeof curve / sizeof *curve; ++i) { v << curve[i]; red << (i == 12 || i == 27); }
        g->setCurve(v, red);
        g->show();
        QApplication::processEvents();
        g->grab().save(out + "graph.png");
    }
    for (int s : {0, 3}) { // the dot
        auto *d = new ConfidenceDot;
        d->resize(22, 22);
        d->setStable(s);
        d->show();
        QApplication::processEvents();
        d->grab().save(out + (s ? "dot_green.png" : "dot_yellow.png"));
    }
    // ---- The move list: Legal's mate, as panel.cpp builds the table (setupSidePanel, updateMoveList/-Cells) ----
    // You White. 5...Bxd1 red (the coach: "allows mate"; best dxe5), 6.Bxf7+ the only move that isn't red.
    auto table = [](const QStringList &san, const QStringList &notes, const QString &result) {
        auto *t = new QTableWidget(0, 4);
        t->horizontalHeader()->hide();
        t->verticalHeader()->hide();
        t->setShowGrid(false);
        t->setAlternatingRowColors(true);
        t->setEditTriggers(QAbstractItemView::DoubleClicked);
        t->setSelectionMode(QAbstractItemView::SingleSelection);
        t->setFocusPolicy(Qt::NoFocus);
        t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        const int moveWidth = t->fontMetrics().horizontalAdvance("Qxd8=Q+! ✎") + 12;
        for (int column : {1, 2}) {
            t->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
            t->setColumnWidth(column, moveWidth);
        }
        t->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        t->verticalHeader()->setDefaultSectionSize(t->fontMetrics().height() + 6);
        const int rows = (san.size() + 1) / 2;
        t->setRowCount(rows);
        for (int row = 0; row < rows; ++row) {
            auto *number = new QTableWidgetItem(QString::number(row + 1));
            number->setTextAlignment(Qt::AlignCenter);
            number->setForeground(QColor(130, 130, 130));
            number->setFlags(Qt::ItemIsEnabled);
            t->setItem(row, 0, number);
            auto *note = new QTableWidgetItem(notes.value(row));
            note->setFlags(Qt::ItemIsEnabled | Qt::ItemIsEditable);
            t->setItem(row, 3, note);
        }
        for (int i = 0; i < san.size(); ++i) {
            auto *cell = new QTableWidgetItem(san[i]);
            if (san[i].endsWith('?'))
                cell->setForeground(QColor(211, 47, 47));
            else if (san[i].endsWith('!'))
                cell->setForeground(QColor(46, 158, 68));
            t->setItem(i / 2, 1 + i % 2, cell);
        }
        if (!result.isEmpty()) {
            t->insertRow(rows);
            t->setSpan(rows, 0, 1, 4);
            auto *cell = new QTableWidgetItem(result);
            cell->setTextAlignment(Qt::AlignCenter);
            cell->setForeground(QColor(130, 130, 130));
            cell->setFlags(Qt::NoItemFlags);
            t->setItem(rows, 0, cell);
        }
        t->setFixedSize(330, t->rowCount() * t->verticalHeader()->defaultSectionSize() + 2 * t->frameWidth());
        t->show();
        QApplication::processEvents();
        return t;
    };
    const QStringList legal = {"e4", "e5", "Nf3", "d6", "Bc4", "Bg4", "Nc3", "h6", "Nxe5", "Bxd1?", "Bxf7+!", "Ke7",
                               "Nd5#"};
    const QStringList legalNotes = {"", "", "", "", "Legal's trap: will he?", "", "Mate!"};
    auto moveList = [&](bool over) { return table(legal, legalNotes, over ? "Checkmate: you win!" : ""); };
    {
        QTableWidget *t = moveList(true);
        t->grab().save(out + "movelist.png");
    }
    {   // the coach's comment: the tooltip on 5...Bxd1?
        QTableWidget *t = moveList(true);
        const QRect cell = t->visualItemRect(t->item(4, 2));
        QToolTip::showText(t->viewport()->mapToGlobal(cell.center() + QPoint(0, 14)), "Best was dxe5.", t);
        wait(300);
        QWidget *tip = nullptr;
        for (QWidget *w : QApplication::topLevelWidgets())
            if (w->inherits("QTipLabel") && w->isVisible())
                tip = w;
        QPixmap table = t->grab(), label = tip ? tip->grab() : QPixmap();
        QPixmap all(table.size());
        all.setDevicePixelRatio(table.devicePixelRatio());
        all.fill(QApplication::palette().color(QPalette::Window));
        QPainter p(&all);
        p.drawPixmap(0, 0, table);
        if (tip)
            p.drawPixmap(t->viewport()->mapTo(t, cell.center() + QPoint(-10, 12)), label);
        p.end();
        all.save(out + "movelist_tip.png");
    }
    {   // typing a note: the Notes cell of row 6, opened
        QTableWidget *t = moveList(false);
        t->item(5, 3)->setText("");
        t->editItem(t->item(5, 3));
        QApplication::processEvents();
        if (auto *editor = t->findChild<QLineEdit *>())
            editor->setText("Bishop first, then N");
        QApplication::processEvents();
        t->grab().save(out + "movelist_note.png");
    }

    {   // the app's icon (the AppImage's desktop entry, public/packaging/): a white king on a dark square, 256 px
        QPixmap icon(256, 256);
        icon.fill(Qt::transparent);
        QPainter p(&icon);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(181, 136, 99));                  // the board's dark square
        p.drawRoundedRect(QRectF(8, 8, 240, 240), 44, 44);
        QFont font("Buster Pieces");
        font.setPixelSize(int(200 * 1.1));                 // as the board sizes its pieces to the square
        p.setFont(font);
        drawPiece(p, QRect(28, 28, 200, 200), 12);         // 12: the white king
        p.end();
        icon.save(out + "buster-coach.png");
    }
    // ---- Openings: the Najdorf, you Black (bookinfo: after 5.Nc3, a6 65 % inside; g6 23 % leaves, into the Dragon) ----
    auto strip = [](const QString &name, bool inBook, bool skip) {   // the opening strip, as setupOpenings builds it
        auto *row = new QWidget;
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *label = new QLabel(name);
        label->setMinimumHeight(label->fontMetrics().height());
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setEnabled(inBook);
        layout->addWidget(label, 1);
        auto *button = new QToolButton;
        button->setIcon(row->style()->standardIcon(QStyle::SP_MediaSeekForward));
        button->setVisible(skip);
        layout->addWidget(button);
        return row;
    };
    auto column = [](const QList<QWidget *> &parts, int gap) {
        auto *w = new QWidget;
        auto *col = new QVBoxLayout(w);
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(gap);
        for (QWidget *part : parts)
            col->addWidget(part);
        w->setFixedWidth(330);
        w->adjustSize();
        w->show();
        QApplication::processEvents();
        return w;
    };
    const QStringList najdorf = {"e4", "c5", "Nf3", "d6", "d4", "cxd4", "Nxd4", "Nf6", "Nc3", "a6", "Be3", "e5"};
    column({strip("Sicilian Defense: Najdorf Variation, English Attack", true, false), table(najdorf, {}, "")}, 2)
        ->grab().save(out + "opening_strip.png");
    column({strip("Sicilian Defense: Najdorf Variation, English Attack", false, false),
            strip("Sicilian Defense", true, true)}, 10)->grab().save(out + "opening_strips.png");
    {   // the practice page: the real PracticePanel on the real names and book, searched for "Najdorf"
        OpeningNames names;
        OpeningBook book;
        QFile n(":/data/openings.tsv"), b(":/data/book.bin");
        if (n.open(QIODevice::ReadOnly))
            names.load(n.readAll().toStdString());
        if (b.open(QIODevice::ReadOnly))
            book.load(b.readAll().toStdString());
        Position model;
        const double all = book.games(model.Key(*model.FenInput(Position::StartFen)));
        QList<PracticeOpening> openings;
        for (const auto &[key, named] : names.all()) {
            const int games = book.games(key);
            openings.append({QString::fromStdString(named.name), QString::fromStdString(key),
                             games > 0 ? games / all : -1.0});
        }
        auto *page = new PracticePanel;
        page->setOpenings(openings);
        page->resize(330, 420);
        page->show();
        QApplication::processEvents();
        if (auto *search = page->findChild<QLineEdit *>())
            search->setText("Najdorf");
        if (auto *tree = page->findChild<QTreeWidget *>()) {
            tree->expandAll();
            const auto found = tree->findItems("Najdorf Variation", Qt::MatchStartsWith | Qt::MatchRecursive);
            if (!found.isEmpty())
                tree->setCurrentItem(found.first());
        }
        for (QRadioButton *button : page->findChildren<QRadioButton *>())
            if (button->text() == "Black")
                button->setChecked(true);
        QApplication::processEvents();
        page->grab().save(out + "practice_page.png");
    }
    {   // your move inside (a6) and one that leaves (g6): the box's book sign, the why card with the book's words
        const char *after5 = "rnbqkb1r/pp2pppp/3p1n2/8/3NP3/2N5/PPP2PPP/R1BQKB1R b KQkq - 2 5";
        const QString amber = QColor(230, 160, 30).name();
        flipped = true;
        for (const auto &[from, to, uci, inside, words] :
             {std::tuple{48, 40, "a7a6", true, "a6: a quiet move: no tactics. A book move: 65 % of players choose it here, "
                                               "inside the Najdorf Variation."},
              std::tuple{54, 46, "g7g6", false, "g6: a quiet move: no tactics. A book move: 23 % of players choose it "
                                                "here, but it leaves the Najdorf Variation (into Dragon Variation)."}}) {
            BoardWidget *b = board(after5);
            b->setFlipped(true);
            b->setLastMove(1, 18);                   // 5. Nc3
            b->setTargetColours({{uci, QColor(230, 160, 30)}});
            b->setBookMoves({"a7a6", "g7g6", "b8c6", "e7e6"}, {"g7g6", "b8c6", "e7e6"});   // hollow: they leave
            send(b, QEvent::MouseButtonPress, from);
            send(b, QEvent::MouseMove, to);
            stack(inside ? grabBoard(b, 0, 5, 4, 7) : grabBoard(b, 2, 7, 4, 7), cards({{amber, words}}, 360),
                  out + (inside ? "practice_inside.png" : "practice_leaves.png"));
        }
        flipped = false;
    }
    cards({{"#2a6ab0", "You left the Najdorf Variation: the book plays a6. Take back to try again."}}, 360)
        .save(out + "practice_left.png");

    // ---- Asking the coach ----
    const char *pin = "rn1qkbnr/ppp2ppp/3p4/4p3/4P1b1/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 2 4";
    const int c3 = 18, f3sq = 21, d2 = 11, d4 = 27, h4 = 31;
    const QString red = "#c62828", green = QColor(60, 180, 75).name(), blue = "#2a6ab0";
    for (const auto &[sq, file] : {std::pair{c3, "ask_count.png"}, std::pair{f3sq, "ask_grey.png"}}) {
        BoardWidget *b = board(pin);
        b->setLastMove(57, 30);                     // ...Bc8-g4
        b->setPieceVerdicts({{c3, 3}, {f3sq, 0}});
        b->setLingerTime(50);
        send(b, QEvent::MouseMove, sq);
        wait(200);
        cut(b, 2, 7, 0, 3, file);
    }
    {   // a move: d4 (green), and Nh4 (red, lingered: the flash)
        BoardWidget *b = board(pin);
        b->setTargetColours({{"d2d4", QColor(60, 180, 75)}});
        send(b, QEvent::MouseButtonPress, d2);
        send(b, QEvent::MouseMove, d4);
        stack(grabBoard(b, 2, 7, 0, 3), cards({{green, "d4: takes the centre."},
              {"gray", "Strongest line: 4... Bxf3 5. gxf3 exd4 6. Qxd4"}}, 360), out + "ask_green.png");
    }
    {
        BoardWidget *b = board(pin);
        b->setTargetColours({{"f3h4", QColor(200, 40, 40)}});
        b->setTargetReplies({{"f3h4", "g4d1"}});
        b->setLingerTime(50);
        send(b, QEvent::MouseButtonPress, f3sq);
        send(b, QEvent::MouseMove, h4);
        wait(200);
        stack(grabBoard(b, 2, 7, 0, 3), cards({{red, "Nh4: loses the queen on move 5."},
              {"gray", "Strongest line: 4... Bxd1 5. Kxd1 Qxh4 6. g3"}}, 360), out + "ask_red.png");
    }
    {   // the play-through: d4, Space, Space: after 4... Bxf3
        BoardWidget *b = board("rn1qkbnr/ppp2ppp/3p4/4p3/3PP3/2N2b2/PPP2PPP/R1BQKB1R w KQkq - 0 5");
        b->setViewing(true);
        b->setPreview(true);
        b->setLastMove(30, 21);
        cut(b, 0, 7, 0, 7, "ask_playthrough.png");
    }
    // The Two Knights, 5...Na5: the threat ...Nxc4
    const char *two = "r1bqkb1r/ppp2ppp/5n2/n2Pp1N1/2B5/8/PPPP1PPP/RNBQK2R w KQkq - 1 6";
    {
        BoardWidget *b = board(two);
        b->setLastMove(42, 32);                     // ...Nc6-a5
        b->setThreat("a5c4");
        stack(grabBoard(b, 0, 5, 3, 5), cards({{red, "Buster threatens …Nxc4, taking your undefended bishop."}}, 360),
              out + "ask_threat.png");
    }
    {   // the hanging piece's words (the signs page's picture)
        BoardWidget *b = board("4k3/8/3p4/4N3/8/8/8/4K3 w - - 0 1");
        b->setHangingMarks({36});
        b->setLingerTime(50);
        send(b, QEvent::MouseMove, 36);
        wait(200);
        stack(grabBoard(b, 1, 6, 4, 5), cards({{red, "Knight e5: attacked by the d6 pawn, not defended."}}, 360),
              out + "ask_hanging.png");
    }
    cards({{red, "Buster threatens …Nxc4, taking your undefended bishop."},
           {red, "Bishop c4: attacked by the a5 knight, not defended."},
           {green, "Bb5+: moves the bishop away from …Nxc4, the threat."},
           {"gray", "Strongest line: 6... c6 7. dxc6 bxc6 8. Qf3"}}, 360).save(out + "ask_cards.png");
    cards({{blue, "Buster missed 4... Bxd1. You got away with it."},
           {blue, "Buster slipped. There's something here."}}, 360).save(out + "ask_comments.png");
    return 0;
}
