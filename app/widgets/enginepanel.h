// enginepanel.h — the engine setup page of the side panel (engine_design.md): the library of engines and the two
// roles (Opponent, Coach): the library with its tests (Add…, Remove, Test again), the roles, OK / Cancel; remembered in
// QSettings (engines/library, engines/opponent, engines/coach).
#pragma once
#include <QFrame>
#include <QList>
#include "uci_info.h"   // TestResult

class EngineTester;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QComboBox;

class EnginePanel : public QFrame
{
    Q_OBJECT

signals:
    void accepted();    // OK: use the choices (the changed roles restart; the game goes on)
    void cancelled();   // Cancel: nothing changes

public:
    explicit EnginePanel(QWidget *parent = nullptr);
    // The page is shown: the library and the roles as last saved. message: why it's shown, if not by the menu
    // (the first run, a missing engine file).
    void open(const QString &message = QString());
    // The engine that ships with the app (MainWindow::bundledEngine; "" = none): always in the library.
    void setBuiltIn(const QString &path) { m_builtIn = path; }
    // A saved engine value as a file for this run. The built-in Buster is saved as builtInWord, not as its path (an
    // AppImage unpacks itself to a new folder every run); an older app's saved built-in (a /tmp/.mount_ path whose
    // file is gone) counts as the built-in too. "" = no such engine.
    static QString resolve(const QString &saved, const QString &builtIn);
    static inline const QString builtInWord = QStringLiteral("builtin");

private:
    struct EngineEntry {                          // one engine in the library
        QString path;
        QString name;                             // its "id name"
        TestResult coach;                         // the tests' verdicts and reasons
        TestResult opponent;
    };
    QList<EngineEntry> m_library;
    QString m_testing;                            // the file being tested ("" = none)
    QString m_builtIn;                            // the built-in engine's file ("" = none)

    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;                   // the test's progress, or a message
    QProgressBar *m_busy = nullptr;               // Qt's busy animation while a test runs
    QPushButton *m_add = nullptr;
    QPushButton *m_ok = nullptr;
    EngineTester *m_tester = nullptr;

    void addEngine(const QString &path);          // test it; added to the library when it's a UCI engine
    void setTesting(bool on);                     // the busy bar shows; Add and OK wait
    void showLibrary();                           // the list from m_library
    void showRoles();                             // the drop-downs from m_library: only engines that passed the role's test
    void showWhy();                               // the selected engine's results in words, under the list
    void startTest(const QString &path);          // run the tests on a file (Add…, Test again)
    void updateOk();                              // OK only with an engine in both roles, and no test running
    void save();                                  // OK: the library and the roles into QSettings

    QComboBox *m_opponent = nullptr;              // the roles: an engine's path as each item's data
    QComboBox *m_coach = nullptr;
    QPushButton *m_remove = nullptr;
    QPushButton *m_retest = nullptr;

};

