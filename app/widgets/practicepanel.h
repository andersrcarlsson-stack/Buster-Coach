// practicepanel.h — the opening practice page of the side panel (openings_design.md "Opening practice"): the named
// openings as a tree with a search field, your colour, Start / Cancel.
#pragma once
#include <QFrame>
#include <QList>

class QLineEdit;
class QPushButton;
class QRadioButton;
class QTreeWidget;
class QTreeWidgetItem;

struct PracticeOpening {
    QString name;    // "Sicilian Defense: Najdorf Variation"
    QString key;     // its position (Position::Key)
    double share;    // of all book games, 0..1; -1 = the book doesn't reach it
};

class PracticePanel : public QFrame
{
    Q_OBJECT

signals:
    void startRequested(const QString &key, bool humanIsWhite);   // Start: the chosen opening (its position), your colour
    void cancelled();        // Cancel: back to the game

public:
    explicit PracticePanel(QWidget *parent = nullptr);
    void setOpenings(QList<PracticeOpening> openings);   // by value: it sorts its own copy

private:
    QLineEdit *m_search = nullptr;     // filters the tree as you type (6.1c)
    QTreeWidget *m_tree = nullptr;     // the names: family → variation → subvariation (6.1b)
    QRadioButton *m_white = nullptr;   // your colour
    QRadioButton *m_black = nullptr;
    QPushButton *m_start = nullptr;
    bool filter(QTreeWidgetItem *item, const QString &text);   // hide what doesn't match; true if item or one below matches

};
