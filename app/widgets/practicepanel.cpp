// practicepanel.cpp — the opening practice page: the names as a tree, a search field, your colour, Start / Cancel.
#include "practicepanel.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <algorithm>

PracticePanel::PracticePanel(QWidget *parent)
    : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);                 // the same frame as the other pages
    auto *column = new QVBoxLayout(this);
    auto *title = new QLabel("Practise an opening");
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    column->addWidget(title);

    m_search = new QLineEdit;
    m_search->setPlaceholderText("Search: Najdorf, Italian, …");
    m_search->setClearButtonEnabled(true);              // a small ✕ in the field empties it
    column->addWidget(m_search);

    m_tree = new QTreeWidget;
    m_tree->setHeaderHidden(true);                      // one column: no header needed
    m_tree->setColumnCount(2);                          // the name, its share
    m_tree->header()->setStretchLastSection(false);     // the hidden header still sizes the columns:
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);            // the name takes the width,
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);   // the share just what it needs
    column->addWidget(m_tree, 1);                       // the tree takes the spare height

    // Radio buttons with the same parent widget exclude each other by themselves: one click, one colour.
    auto *colourRow = new QHBoxLayout;
    colourRow->addWidget(new QLabel("You play:"));
    m_white = new QRadioButton("White");
    m_black = new QRadioButton("Black");
    m_white->setChecked(true);
    colourRow->addWidget(m_white);
    colourRow->addWidget(m_black);
    colourRow->addStretch();
    column->addLayout(colourRow);

    auto *buttonRow = new QHBoxLayout;
    m_start = new QPushButton("Start");
    m_start->setEnabled(false);                         // until an opening is chosen (6.1b)
    auto *cancel = new QPushButton("Cancel");
    buttonRow->addStretch();
    buttonRow->addWidget(m_start);
    buttonRow->addWidget(cancel);
    column->addLayout(buttonRow);

    connect(m_start, &QPushButton::clicked, this, [this]() {   // Start is only enabled on a named opening
        emit startRequested(m_tree->currentItem()->data(0, Qt::UserRole).toString(), m_white->isChecked());
    });
    connect(cancel, &QPushButton::clicked, this, &PracticePanel::cancelled);

    // Start only on a named opening (a node with a key), not on a node that only groups deeper names.
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        m_start->setEnabled(item && !item->data(0, Qt::UserRole).toString().isEmpty());
    });
    
    // Search as you type: every keystroke filters the whole tree (3,865 names: fast enough to do it all each time).
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
            filter(m_tree->topLevelItem(i), text.trimmed());
    });
}

// "Family: Variation, Subvariation" → three levels of the tree. The last part is the named opening itself: its item
// keeps the position key (Qt::UserRole). The levels on the way are shared between names (one "Sicilian Defense").
void PracticePanel::setOpenings(QList<PracticeOpening> openings) {
    // The most played first (the tree keeps the order it's built in); the ones the book doesn't reach last, by name.
    std::sort(openings.begin(), openings.end(), [](const PracticeOpening &a, const PracticeOpening &b) {
        return a.share != b.share ? a.share > b.share : a.name < b.name;
    });
    m_tree->clear();
    QHash<QString, QTreeWidgetItem *> nodes;           // the path so far ("|Sicilian Defense|Najdorf Variation") → item
    for (const auto &[name, key, share] : openings) {
        const int colon = name.indexOf(':');
        QStringList parts{name.left(colon)};            // no colon (-1): left() gives the whole name, the family
        if (colon != -1)
            parts += name.mid(colon + 1).trimmed().split(", ");
        QTreeWidgetItem *parent = nullptr;
        QString path;
        for (const QString &part : parts) {
            path += "|" + part;
            QTreeWidgetItem *&node = nodes[path];       // a reference INTO the hash: a new path starts as nullptr
            if (!node) {
                node = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
                node->setText(0, part);
            }
            parent = node;
        }
        if (!parent->data(0, Qt::UserRole).toString().isEmpty())
            continue;                                   // a name shared by several move orders: the most played came first
        parent->setData(0, Qt::UserRole, key);          // the named opening itself
        parent->setText(1, share < 0 ? "–" : share < 0.01 ? "<1 %" : QString("%1 %").arg(qRound(share * 100)));
        parent->setForeground(1, palette().color(QPalette::Disabled, QPalette::Text));   // faint: light or dark
        parent->setTextAlignment(1, Qt::AlignRight);
    }
}

// An item stays visible if its text matches, or if one below it does (then it's opened, to show the way). A match on
// the item itself keeps its whole branch visible ("Sicilian" shows every Sicilian variation). Recursive: depth first.
bool PracticePanel::filter(QTreeWidgetItem *item, const QString &text) {
    const bool selfMatches = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive);
    bool childMatches = false;
    for (int i = 0; i < item->childCount(); ++i)
        childMatches |= filter(item->child(i), selfMatches ? QString() : text);   // a match above: all below show
    item->setHidden(!selfMatches && !childMatches);
    item->setExpanded(!text.isEmpty() && childMatches && !selfMatches);       // open only to show a match deeper down
    return selfMatches || childMatches;
}
