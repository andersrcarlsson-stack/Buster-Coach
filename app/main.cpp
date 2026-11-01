// main.cpp — creates the application and the main window, and runs the event loop.
// Everything else starts in MainWindow (window/mainwindow.h).

#include <QApplication>
#include <QFontDatabase>
#include <QDebug>
#include <QSettings>
#include <QCommandLineParser>
#include "mainwindow.h"
#include "theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);   // exactly one, created before any widget
    // The piece font, built into the program (resources.qrc): no system font needed (R3). -1 = it didn't load.
    if (QFontDatabase::addApplicationFont(":/fonts/BusterPieces.ttf") == -1)
        qWarning() << "The piece font didn't load from the resources";
    // Who we are, for QSettings. Linux: ~/.config/Buster/Buster Coach.conf; Windows: the registry.
    // Set BEFORE the window: it reads its saved geometry and settings while it is built.
    QCoreApplication::setOrganizationName("Buster");
    QCoreApplication::setApplicationName("Buster Coach");
    QCoreApplication::setApplicationVersion(BUSTER_COACH_VERSION);   // from CMake's project(VERSION …)

    // The command line: --help and --version print their answer and quit; an unknown option stops with a message.

    // After the name and version (the parser shows them). Qt's own options (-style, -platform) are taken by QApplication.
    QCommandLineParser parser;
    parser.setApplicationDescription("A chess opponent with a coach beside you.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    const bool dark = QSettings().value("personal/theme", "light").toString() == "dark";   // the remembered appearance
    applyTheme(dark);
    MainWindow window;   // builds the window and wires the parts together
    applyTitleBar(&window, dark);   // after the window exists: it needs its X11 id
    window.show();       // only a request; drawing happens in app.exec()
    return app.exec();   // the event loop: runs until the window is closed
}
