// gamespanel.cpp — the Open game page: a month's games as a list, the month chooser, Other file…, Open / Cancel.
#include "gamespanel.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

GamesPanel::GamesPanel(QWidget *parent)
    : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);                 // the same frame as the other pages
    auto *column = new QVBoxLayout(this);
    auto *title = new QLabel("Open a game");
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    column->addWidget(title);

    auto *monthRow = new QHBoxLayout;
    monthRow->addWidget(new QLabel("Your games:"));
    m_month = new QComboBox;
    monthRow->addWidget(m_month, 1);
    auto *other = new QPushButton("Other file…");
    monthRow->addWidget(other);
    column->addLayout(monthRow);

    m_list = new QListWidget;
    column->addWidget(m_list, 1);                       // the list takes the spare height

    auto *buttonRow = new QHBoxLayout;
    m_open = new QPushButton("Open");
    auto *cancel = new QPushButton("Cancel");
    buttonRow->addStretch();
    buttonRow->addWidget(m_open);
    buttonRow->addWidget(cancel);
    column->addLayout(buttonRow);

    connect(m_open, &QPushButton::clicked, this, &GamesPanel::openCurrent);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &GamesPanel::openCurrent);   // its argument: dropped
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        const bool game = row >= 0 && row < m_games.size();
        m_open->setEnabled(game);
        if (game)
            emit previewRequested(m_games[row].fen, m_games[row].flipped);   // mouse or arrow keys alike
    });
    connect(m_month, &QComboBox::currentTextChanged, this, &GamesPanel::monthChosen);
    connect(other, &QPushButton::clicked, this, &GamesPanel::otherFileRequested);
    connect(cancel, &QPushButton::clicked, this, &GamesPanel::cancelled);
}

void GamesPanel::setMonths(const QStringList &months) {
    const QSignalBlocker quiet(m_month);                // filling it isn't a choice: no monthChosen
    m_month->clear();
    m_month->addItems(months);
}

void GamesPanel::setGames(const QList<GameListEntry> &games) {
    m_games = games;
    m_list->clear();
    for (const GameListEntry &game : games)
        m_list->addItem(game.label);
    if (games.isEmpty()) {
        m_list->addItem("No games here");
        m_list->item(0)->setFlags(Qt::NoItemFlags);     // shown, but not selectable
    }
    m_list->setCurrentRow(games.isEmpty() ? -1 : 0);    // the newest chosen
    m_open->setEnabled(!games.isEmpty());
}

void GamesPanel::openCurrent() {
    const int row = m_list->currentRow();
    if (row >= 0 && row < m_games.size())
        emit openRequested(m_games[row].path, m_games[row].index);
}
