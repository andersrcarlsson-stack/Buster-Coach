// editorpanel.cpp — the board editor's side panel page (see editorpanel.h).

#include "editorpanel.h"
#include <QButtonGroup>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QCheckBox>
#include "piecepainter.h"
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>



EditorPanel::EditorPanel(QWidget *parent) : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);                 // the same frame as the play page
    auto *column = new QVBoxLayout(this);
    auto *title = new QLabel("Set up a position");
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    column->addWidget(title);
    // The palette: twelve pieces and an eraser. One tool at a time (a button group is exclusive), and it stays
    // selected: eight pawns are eight clicks. A button's id is the piece code it places (0 = the eraser).
    auto *pieces = new QGroupBox("Pieces");
    auto *grid = new QGridLayout(pieces);
    m_tools = new QButtonGroup(this);
    const int order[] = {12, 10, 8, 6, 4, 2};                // K Q R B N P: White's codes (Black's are one less)
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 6; ++col) {
            auto *tool = new QToolButton;
            tool->setCheckable(true);
            tool->setAutoRaise(true);                         // flat until hovered or selected
            tool->setIconSize(QSize(34, 34));
            grid->addWidget(tool, row, col);
            m_tools->addButton(tool, order[col] - row);       // row 0 White, row 1 Black
        }
    }
    auto *eraser = new QToolButton;
    eraser->setCheckable(true);
    eraser->setAutoRaise(true);
    eraser->setText("✕");
    eraser->setToolTip("Eraser: clears a square (as a right-click does)");
    grid->addWidget(eraser, 2, 0);
    m_tools->addButton(eraser, 0);
    auto *hint = new QLabel("Right-click a square to clear it");
    hint->setStyleSheet("color: gray;");
    grid->addWidget(hint, 2, 1, 1, 5);                        // row 2, from column 1, one row high, five wide
    column->addWidget(pieces);
    m_tools->button(m_tool)->setChecked(true);                // the white queen, as before
    updateToolIcons();
    connect(m_tools, &QButtonGroup::idClicked, this, [this](int id) { m_tool = id; });
    // Side to move. Radio buttons with the same parent are exclusive by themselves: no button group needed.
    auto *sideBox = new QGroupBox("Side to move");
    auto *sideRow = new QHBoxLayout(sideBox);
    m_whiteToMove = new QRadioButton("White");
    m_blackToMove = new QRadioButton("Black");
    sideRow->addWidget(m_whiteToMove);
    sideRow->addWidget(m_blackToMove);
    sideRow->addStretch();
    sideRow->addWidget(new QLabel("e.p."));
    m_ep = new QLineEdit;
    m_ep->setMaxLength(2);
    m_ep->setFixedWidth(m_ep->fontMetrics().horizontalAdvance("e6") + 16);   // two characters and the frame
    m_ep->setPlaceholderText("–");                          // shown while empty
    // Only a file and rank 3 or 6, or nothing. The "?" matters: without it an empty field would be "unfinished",
    // and an unfinished field never reports editingFinished: the square could never be cleared again.
    m_ep->setValidator(new QRegularExpressionValidator(QRegularExpression("([a-h][36])?"), m_ep));
    sideRow->addWidget(m_ep);
    column->addWidget(sideBox);
    // White's toggled() fires for both: clicking Black un-checks White. One connection covers the pair.
    connect(m_whiteToMove, &QRadioButton::toggled, this, [this](bool white) {
        m_position.pit = !white;
        changed();
    });
    connect(m_ep, &QLineEdit::editingFinished, this, [this]() {   // Enter, or leaving the field
        const QString text = m_ep->text();
        m_position.en_passant_target = text.isEmpty() ? -1
                                     : (text[1].toLatin1() - '1') * 8 + (text[0].toLatin1() - 'a');
        changed();
    });
    // Castling: one box per right. Only enabled while its king and rook stand at home (changed() sees to it).
    auto *castleBox = new QGroupBox("Castling");
    auto *castleGrid = new QGridLayout(castleBox);
    castleGrid->addWidget(new QLabel("White"), 0, 0);
    castleGrid->addWidget(new QLabel("Black"), 1, 0);
    for (int i = 0; i < 4; ++i) {
        m_castling[i] = new QCheckBox(i % 2 == 0 ? "O-O" : "O-O-O");
        castleGrid->addWidget(m_castling[i], i / 2, 1 + i % 2);
        connect(m_castling[i], &QCheckBox::toggled, this, [this, i](bool on) {
            bool *rights[] = {&m_position.K, &m_position.Q, &m_position.k, &m_position.q};   // in the boxes' order
            *rights[i] = on;
            changed();
        });
    }
    column->addWidget(castleBox);
    // The FEN: it follows every change, and Enter loads a typed or pasted one. Only Enter: leaving the field with a
    // half-typed FEN mustn't load it. A FEN that doesn't parse turns red and changes nothing.
    auto *fenBox = new QGroupBox("FEN");
    auto *fenRow = new QVBoxLayout(fenBox);
    m_fen = new QLineEdit;
    fenRow->addWidget(m_fen);
    column->addWidget(fenBox);
    m_validity = new QLabel;                                  // changed() fills it
    column->addWidget(m_validity);
    connect(m_fen, &QLineEdit::returnPressed, this, [this]() {
        const std::optional<Position::Chess> parsed = m_model.FenInput(m_fen->text().trimmed().toStdString());
        if (!parsed) {
            m_fen->setStyleSheet("color: #c62828;");          // red: not a FEN at all (validity comes later)
            return;
        }
        m_position = *parsed;
        m_fen->clearFocus();                                  // so changed() may write the field again
        changed();
    });
    column->addStretch();                               // the controls come here, step by step
    auto *boardRow = new QHBoxLayout;
    auto *startButton = new QPushButton("Start position");
    auto *clearButton = new QPushButton("Clear");
    auto *flipButton = new QPushButton("Flip");
    boardRow->addWidget(startButton);
    boardRow->addWidget(clearButton);
    boardRow->addWidget(flipButton);
    column->addLayout(boardRow);
    connect(startButton, &QPushButton::clicked, this, [this]() {
        start(Position::StartFen);
    });
    connect(clearButton, &QPushButton::clicked, this, [this]() {   // an empty board; the side to move stays
        m_position.mailbox.fill(0);                       // std::array: every square 0
        m_position.en_passant_target = -1;
        m_position.halfmove_clock = 0;
        m_position.fullmove_number = 1;
        changed();                                        // the castling rights go by themselves: no kings, no rooks
    });
    connect(flipButton, &QPushButton::clicked, this, &EditorPanel::flipRequested);
    auto *playRow = new QHBoxLayout;
    m_playWhite = new QPushButton("Play as White");
    m_playBlack = new QPushButton("Play as Black");
    playRow->addWidget(m_playWhite);
    playRow->addWidget(m_playBlack);
    column->addLayout(playRow);
    for (QPushButton *play : {m_playWhite, m_playBlack})
        connect(play, &QPushButton::clicked, this, [this, play]() {
            emit playRequested(QString::fromStdString(m_model.FenOutput(m_position)), play == m_playWhite);
        });
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();                              // Cancel at the right
    auto *cancel = new QPushButton("Cancel");
    buttons->addWidget(cancel);
    column->addLayout(buttons);
    // A signal connected to a signal: a click is passed on as our own cancelled(). MainWindow needn't know the button.
    connect(cancel, &QPushButton::clicked, this, &EditorPanel::cancelled);

}

void EditorPanel::start(const QString &fen) {
    m_position = m_model.FenInput(fen.toStdString()).value();   // the game's own FEN: always valid
    changed();
}

void EditorPanel::squareClicked(int square, bool right) {
    // The board never takes the keyboard focus, so a click there leaves it in a text field, where changed()
    // won't write. Editing the board means leaving the fields: an e.p. being typed is applied (editingFinished),
    // a half-typed FEN is dropped.
    m_ep->clearFocus();
    m_fen->clearFocus();
    m_position.mailbox[square] = right ? 0 : m_tool;   // right-click clears, whatever the tool
    changed();
}


void EditorPanel::updateToolIcons() {
    for (QAbstractButton *button : m_tools->buttons()) {
        const int code = m_tools->id(button);
        if (code != 0)                                        // the eraser keeps its ✕
            button->setIcon(QIcon(piecePixmap(code, 68, panelRim(palette(), code))));   // twice the size: sharp
    }
}

void EditorPanel::changeEvent(QEvent *event) {
    QFrame::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && m_tools)   // m_tools: a palette change can come during construction
        updateToolIcons();
}

void EditorPanel::changed() {
    // A castling right needs its king and rook at home; if one has gone, the box is greyed and the right goes too.
    struct Home { int king, kingCode, rook, rookCode; };
    static const Home homes[4] = {{4, 12, 7, 8}, {4, 12, 0, 8}, {60, 11, 63, 7}, {60, 11, 56, 7}};   // e1 h1, e1 a1, ...
    bool *rights[] = {&m_position.K, &m_position.Q, &m_position.k, &m_position.q};
    for (int i = 0; i < 4; ++i) {
        const bool home = m_position.mailbox[homes[i].king] == homes[i].kingCode
                          && m_position.mailbox[homes[i].rook] == homes[i].rookCode;
        if (!home)
            *rights[i] = false;
        const QSignalBlocker block(m_castling[i]);          // from the position: no echo
        m_castling[i]->setEnabled(home);
        m_castling[i]->setChecked(*rights[i]);
    }
    {
        // Setting the buttons FROM the position must not echo back as a user's click: block their signals.
        const QSignalBlocker blockWhite(m_whiteToMove);
        m_whiteToMove->setChecked(!m_position.pit);
        m_blackToMove->setChecked(m_position.pit);
    }                                                       // the blocker's destructor: the signals flow again
    const int ep = m_position.en_passant_target;
    if (!m_ep->hasFocus())                                  // never overwrite what you're typing
        m_ep->setText(ep == -1 ? QString() : QString("%1%2").arg(QChar('a' + ep % 8)).arg(ep / 8 + 1));
    if (!m_fen->hasFocus()) {                               // never overwrite what you're typing
        m_fen->setText(QString::fromStdString(m_model.FenOutput(m_position)));
        m_fen->setCursorPosition(0);                        // show the start (the pieces), not the end
        m_fen->setStyleSheet(QString());                    // a FEN from the position is never red
    }
    // The validity line: the first problem only (editor_design.md), in the enum's order.
    static const char *problems[] = {"", "White needs exactly one king", "Black needs exactly one king",
                                     "A pawn can't stand on rank 1 or 8", "",   // the check: worded below
                                     "A castling right needs its king and rook at home",
                                     "The e.p. square isn't possible here"};
    const Position::Invalid invalid = m_model.Validate(m_position);
    const bool ok = invalid == Position::Invalid::None;
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    QString text = ok ? QString("✓ A legal position") : QString(problems[static_cast<int>(invalid)]);
    if (invalid == Position::Invalid::NotToMoveInCheck) {   // e.g. "Black is in check, but it's White's move"
        const QString toMove = m_position.pit ? "Black" : "White", other = m_position.pit ? "White" : "Black";
        text = other + " is in check, but it's " + toMove + "'s move";
    }
    m_validity->setText(text);
    m_validity->setStyleSheet(QString("font-weight: bold; color: %1;")
                                  .arg(ok ? (dark ? "#7fd67f" : "#2e8b3a") : (dark ? "#ff7070" : "#c62828")));
    m_playWhite->setEnabled(ok);                            // no game from an impossible position
    m_playBlack->setEnabled(ok);
    emit positionChanged(m_position);
}
