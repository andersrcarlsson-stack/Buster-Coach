// enginepanel.cpp — the engine setup page (see enginepanel.h).

#include "enginepanel.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QComboBox>
#include <QFormLayout>
#include <QSettings>
#include <algorithm>
#include "enginetester.h"
#include "theme.h"


static QString mark(Fitness fitness) {
    static const char *marks[] = {"✓", "⚠", "✗"};   // Good, Warn, No
    return marks[static_cast<int>(fitness)];
}

EnginePanel::EnginePanel(QWidget *parent) : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);                 // the same frame as the other pages
    auto *column = new QVBoxLayout(this);
    auto *title = new QLabel("Engines");
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    title->setFont(titleFont);
    column->addWidget(title);

    // The library: every engine added, with its test results. Add… tests a file first (enginetester.h).
    auto *library = new QGroupBox("Library");
    auto *libraryColumn = new QVBoxLayout(library);
    m_list = new QListWidget;
    m_list->setSpacing(3);                              // a little air around each engine
    m_list->setAlternatingRowColors(true);              // every other engine shaded: its two lines read as one
    libraryColumn->addWidget(m_list);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    libraryColumn->addWidget(m_status);
    m_busy = new QProgressBar;
    m_busy->setRange(0, 0);                             // no range = "busy": Qt animates it by itself
    m_busy->setTextVisible(false);
    m_busy->hide();
    libraryColumn->addWidget(m_busy);
    auto *libraryButtons = new QHBoxLayout;
    m_add = new QPushButton("Add…");
    m_remove = new QPushButton("Remove");
    m_retest = new QPushButton("Test again");
    libraryButtons->addWidget(m_add);
    libraryButtons->addWidget(m_remove);
    libraryButtons->addWidget(m_retest);
    libraryColumn->addLayout(libraryButtons);
    column->addWidget(library);

    // The roles: who plays you, who coaches. An engine is offered only for a role whose test it passed.
    auto *roles = new QGroupBox("Roles");
    auto *form = new QFormLayout(roles);                // a label, a field; a label, a field
    m_opponent = new QComboBox;
    m_coach = new QComboBox;
    form->addRow("Opponent", m_opponent);
    form->addRow("Coach", m_coach);
    auto *note = new QLabel("The coach runs as two processes (the general and the master coach).");
    note->setWordWrap(true);
    note->setStyleSheet("color: gray;");
    form->addRow(note);                                 // one widget alone: it spans both columns
    column->addWidget(roles);
    column->addStretch();
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();                              // OK and Cancel at the right
    m_ok = new QPushButton("OK");
    auto *cancel = new QPushButton("Cancel");
    buttons->addWidget(m_ok);
    buttons->addWidget(cancel);
    column->addLayout(buttons);
    connect(m_ok, &QPushButton::clicked, this, [this]() {
        save();
        emit accepted();
    });
    connect(cancel, &QPushButton::clicked, this, [this]() {
        m_tester->cancel();                             // a running test stops with the page
        setTesting(false);
        emit cancelled();
    });

    // Add…: the one small window (the system's file picker); then the tests, then the list.
    m_tester = new EngineTester(this);
    connect(m_add, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, "Add an engine", QDir::homePath(), QString(),
                                                          nullptr, fileDialogOptions());   // dark: Qt's own dialog
        if (!path.isEmpty())                            // "" = the picker was cancelled
            addEngine(path);
    });
    connect(m_list, &QListWidget::currentRowChanged, this, [this]() { showWhy(); });
    connect(m_retest, &QPushButton::clicked, this, [this]() {
        if (m_list->currentRow() >= 0)
            startTest(m_library[m_list->currentRow()].path);
    });
    connect(m_remove, &QPushButton::clicked, this, [this]() {
        const int row = m_list->currentRow();
        if (row < 0)
            return;
        const QString path = m_library[row].path;
        if (path == m_builtIn) {
            m_status->setText(m_library[row].name + " comes with Buster Coach: it can't be removed (other engines "
                              "can be chosen over it).");
            return;
        }
        if (path == m_opponent->currentData().toString() || path == m_coach->currentData().toString()) {
            m_status->setText(m_library[row].name + " is in use: choose another engine for its role first.");
            return;
        }
        m_library.removeAt(row);
        showLibrary();
        showRoles();
    });
    connect(m_tester, &EngineTester::progress, this, [this](const QString &step) {
        m_status->setText("Testing " + QFileInfo(m_testing).fileName() + "… " + step);
    });
    connect(m_tester, &EngineTester::finished, this,
            [this](const QString &name, const TestResult &coach, const TestResult &opponent) {
        setTesting(false);
        if (name.isEmpty()) {                           // not a UCI engine: not added, and why
            m_status->setText(QFileInfo(m_testing).fileName() + ": " + QString::fromStdString(coach.reason));
            return;
        }
        int row = 0;
        while (row < m_library.size() && m_library[row].path != m_testing)
            ++row;                                      // Test again: its row; Add…: one past the end
        if (row == m_library.size())
            m_library.append({m_testing, name, coach, opponent});
        else
            m_library[row] = {m_testing, name, coach, opponent};
        showLibrary();
        showRoles();
        m_list->setCurrentRow(row);                     // selected: showWhy() explains its results
    });
}

void EnginePanel::addEngine(const QString &path) {
    for (const EngineEntry &entry : m_library)
        if (entry.path == path) {
            m_status->setText(entry.name + " is already in the library.");
            return;
        }
    startTest(path);
}

void EnginePanel::startTest(const QString &path) {
    m_testing = path;
    setTesting(true);
    m_tester->start(path);
}

void EnginePanel::setTesting(bool on) {
    m_busy->setVisible(on);
    m_add->setEnabled(!on);
    m_remove->setEnabled(!on);
    m_retest->setEnabled(!on);
    updateOk();
}

void EnginePanel::showLibrary() {
    m_list->clear();
    for (const EngineEntry &entry : m_library)        // two lines: the name and the results, then the file
        m_list->addItem(entry.name + (entry.path == m_builtIn ? " (built in)" : "") + "    Coach "
                        + mark(entry.coach.fitness) + "  Opponent " + mark(entry.opponent.fitness) + "\n" + entry.path);
}

void EnginePanel::showRoles() {
    // Refill both drop-downs, keeping each one's choice if that engine is still offered.
    auto fill = [this](QComboBox *combo, bool coachRole) {
        const QString chosen = combo->currentData().toString();
        combo->clear();
        for (const EngineEntry &entry : m_library) {
            const Fitness fitness = coachRole ? entry.coach.fitness : entry.opponent.fitness;
            if (fitness == Fitness::No)
                continue;                               // refused for this role: not offered at all
            combo->addItem(entry.name + (fitness == Fitness::Warn ? " (⚠)" : ""), entry.path);
        }
        const int index = combo->findData(chosen);      // the item whose data is that path, -1 = gone
        combo->setCurrentIndex(index >= 0 ? index : 0);
    };
    fill(m_opponent, false);
    fill(m_coach, true);
    updateOk();
}

void EnginePanel::showWhy() {
    const int row = m_list->currentRow();
    if (m_busy->isVisible())
        return;                                         // a test is using the line
    if (row < 0) {
        m_status->clear();                              // nothing selected (e.g. after Remove): nothing to explain
        return;
    }
    const EngineEntry &entry = m_library[row];
    QStringList lines;
    if (entry.coach.fitness != Fitness::Good)
        lines << "Coach " + mark(entry.coach.fitness) + " " + QString::fromStdString(entry.coach.reason);
    if (entry.opponent.fitness != Fitness::Good)
        lines << "Opponent " + mark(entry.opponent.fitness) + " " + QString::fromStdString(entry.opponent.reason);
    m_status->setText(lines.isEmpty() ? entry.name + " is ready for both roles." : lines.join('\n'));
}

QString EnginePanel::resolve(const QString &saved, const QString &builtIn) {
    if (saved == builtInWord)
        return builtIn;
    if (saved.startsWith("/tmp/.mount_") && !QFileInfo::exists(saved))
        return builtIn;
    return saved;
}

// QSettings keys: engines/library (an array: path, name, and per role the verdict and its reason), engines/opponent and
// engines/coach (the chosen engines' paths; the built-in Buster as the word builtInWord, see resolve). Cancel saves
// nothing: the next open() reads the last saved state.
void EnginePanel::open(const QString &message) {
    QSettings settings;
    m_library.clear();
    const int count = settings.beginReadArray("engines/library");   // how many were written
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QString path = resolve(settings.value("path").toString(), m_builtIn);
        const bool seen = std::any_of(m_library.cbegin(), m_library.cend(),
                                      [&path](const EngineEntry &entry) { return entry.path == path; });
        if (path.isEmpty() || seen)
            continue;                                   // no such engine, or an old built-in copy already listed
        m_library.append({path, settings.value("name").toString(),
                          {static_cast<Fitness>(settings.value("coach").toInt()),
                           settings.value("coachReason").toString().toStdString()},
                          {static_cast<Fitness>(settings.value("opponent").toInt()),
                           settings.value("opponentReason").toString().toStdString()}});
    }
    settings.endArray();
    // The built-in Buster (release_design.md 12): in the library from the start, tested like any other the first time
    // (the roles offer only engines that passed); the result is saved with the library on OK.
    const bool known = std::any_of(m_library.cbegin(), m_library.cend(),
                                   [this](const EngineEntry &entry) { return entry.path == m_builtIn; });
    if (!m_builtIn.isEmpty() && !known)
        startTest(m_builtIn);
    showLibrary();
    showRoles();
    for (auto [combo, key] : {std::pair{m_opponent, "engines/opponent"}, {m_coach, "engines/coach"}}) {
        const int index = combo->findData(resolve(settings.value(key).toString(), m_builtIn));
        if (index >= 0)
            combo->setCurrentIndex(index);              // the remembered choice, if it's still offered
    }
    m_status->setText(message);                         // "" from the menu: an empty line
    updateOk();
}

void EnginePanel::save() {
    QSettings settings;
    settings.remove("engines/library");                 // a shorter list leaves no old entries behind
    settings.beginWriteArray("engines/library", m_library.size());
    for (int i = 0; i < m_library.size(); ++i) {
        const EngineEntry &entry = m_library[i];
        settings.setArrayIndex(i);
        settings.setValue("path", entry.path == m_builtIn ? builtInWord : entry.path);
        settings.setValue("name", entry.name);
        settings.setValue("coach", static_cast<int>(entry.coach.fitness));
        settings.setValue("coachReason", QString::fromStdString(entry.coach.reason));
        settings.setValue("opponent", static_cast<int>(entry.opponent.fitness));
        settings.setValue("opponentReason", QString::fromStdString(entry.opponent.reason));
    }
    settings.endArray();
    for (auto [combo, key] : {std::pair{m_opponent, "engines/opponent"}, {m_coach, "engines/coach"}}) {
        const QString path = combo->currentData().toString();
        settings.setValue(key, !m_builtIn.isEmpty() && path == m_builtIn ? builtInWord : path);
    }
}

void EnginePanel::updateOk() {
    m_ok->setEnabled(!m_busy->isVisible() && m_opponent->count() > 0 && m_coach->count() > 0);
}
