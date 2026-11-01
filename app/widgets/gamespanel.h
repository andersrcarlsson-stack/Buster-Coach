// gamespanel.h — the Open game page of the side panel (pgn_design.md "On load"): your games of a month, newest first;
// a month chooser; Other file…; Open / Cancel. It shows what it's given and reports what was picked.
#pragma once
#include <QFrame>
#include <QList>

class QComboBox;
class QListWidget;
class QPushButton;

struct GameListEntry {
    QString label;   // "25 Oct 12:16 · You – Buster 7.0.2 · 0-1 · Danish Gambit"
    QString path;    // its file
    int index;       // its place in the file (a file can hold several games)
    QString fen;     // the game's final position (the preview on the board)
    bool flipped;    // Black at the bottom (you played Black)
};

class GamesPanel : public QFrame
{
    Q_OBJECT

signals:
    void openRequested(const QString &path, int index);   // Open, or a double click: this game onto the board
    void previewRequested(const QString &fen, bool flipped);   // a row chosen: its final position on the board
    void monthChosen(const QString &month);               // the month chooser: "2026-10"
    void otherFileRequested();                            // Other file…: the caller shows a file dialog
    void cancelled();

public:
    explicit GamesPanel(QWidget *parent = nullptr);
    void setMonths(const QStringList &months);            // the month folders, newest first (the first is chosen)
    void setGames(const QList<GameListEntry> &games);     // the list (none: "No games here")

private:
    void openCurrent();                                   // the chosen row → openRequested
    QComboBox *m_month = nullptr;
    QListWidget *m_list = nullptr;
    QPushButton *m_open = nullptr;
    QList<GameListEntry> m_games;
};
