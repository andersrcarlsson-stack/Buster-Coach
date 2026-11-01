// help.cpp — Help → How the coach works (release_design.md 5, step 9): the pages built into the app (app/help/,
// made from help.md by tools/help_pages.py), shown in a window of their own beside the board; the welcome page opens
// by itself once, on the first run.

#include "mainwindow.h"
#include <QAction>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QApplication>
#include <QMessageBox>
#include "engine.h"
#include <QEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

// The help's browser: its pictures follow the zoom (Ctrl + wheel). QTextBrowser's zoom changes only the font, and the
// pictures keep the width their page gives them; so on every font change each picture's width is set to its page
// width times the zoom (the font's size now, over the size at the start).
class HelpBrowser : public QTextBrowser {
public:
    HelpBrowser() : m_baseSize(font().pointSizeF()) {
        // A new page (a link, Back, Forward, Contents) comes with its pictures at the page's width: zoom them too.
        connect(this, &QTextBrowser::sourceChanged, this, &HelpBrowser::scalePictures);
    }
    void scalePictures() {
        const double zoom = font().pointSizeF() / m_baseSize;
        QList<QPair<int, QTextImageFormat>> pictures;   // where each picture is, and its new format: collected
        for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {   // first, changed after
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                QTextImageFormat picture = it.fragment().charFormat().toImageFormat();
                if (!picture.isValid())
                    continue;                                   // text, not a picture
                if (!picture.hasProperty(QTextFormat::UserProperty))
                    picture.setProperty(QTextFormat::UserProperty, picture.width());   // the page's width, kept
                picture.setWidth(picture.property(QTextFormat::UserProperty).toDouble() * zoom);
                pictures << qMakePair(it.fragment().position(), picture);
            }
        }
        QTextCursor cursor(document());
        for (const auto &[position, picture] : pictures) {
            cursor.setPosition(position);
            cursor.setPosition(position + 1, QTextCursor::KeepAnchor);   // a picture is one character
            cursor.setCharFormat(picture);
        }
        // Qt 6.4 stops laying out a long page with many pictures partway: the rest of its text gets no height (only
        // the pictures show). A fresh layout of the whole document, once the browser has settled, fixes it.
        QTimer::singleShot(0, this, [this]() { document()->markContentsDirty(0, document()->characterCount()); });
    }
protected:
    void changeEvent(QEvent *event) override {
        QTextBrowser::changeEvent(event);                       // the browser's own handling first
        if (event->type() == QEvent::FontChange)
            scalePictures();                                    // Ctrl + wheel changed the font
    }

private:
    double m_baseSize;                                          // the font's size before any zoom
};

void MainWindow::setupHelp() {
    QMenu *helpMenu = menuBar()->addMenu("&Help");   // made last: the rightmost menu
    QAction *howAction = helpMenu->addAction("How the coach &works");
    howAction->setShortcut(QKeySequence::HelpContents);   // F1
    connect(howAction, &QAction::triggered, this, &MainWindow::showHelp);

    // About: the version, the licence, the credits (release_design.md 10); Qt's own About as well (it asks for credit).
    helpMenu->addSeparator();
    QAction *aboutAction = helpMenu->addAction("&About Buster Coach");
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About Buster Coach",
            "<h3>Buster Coach " + QCoreApplication::applicationVersion() + "</h3>"
            "<p>A chess opponent with a coach beside you.<br>© 2026 Anders R Carlsson · MIT licence</p>"
            "<p><b>Opponent now:</b> " + m_engine->name() + "</p>"
            "<p><b>Credits</b><br>"
            "Buster, the built-in engine (MIT).<br>"
            "Opening names and book: the lichess database and lichess's chess-openings (CC0). Thank you, lichess.<br>"
            "Chess pieces: GNU FreeSerif (GPLv3 with the font exception).<br>"
            "Built with Qt " QT_VERSION_STR " (LGPLv3; its source at qt.io).</p>");
    });
    helpMenu->addAction("About &Qt", qApp, &QApplication::aboutQt);

    // The first run: the welcome page once, shortly after the window is up (QSettings remembers it was shown).
    if (!QSettings().value("personal/helpShown", false).toBool()) {
        QSettings().setValue("personal/helpShown", true);
        QTimer::singleShot(800, this, &MainWindow::showHelp);
    }
}

// The help window, made the first time it's asked for: Back, Forward and Contents above a QTextBrowser, which follows
// the pages' links by itself and keeps their history.
void MainWindow::showHelp() {
    if (!m_helpWindow) {
        m_helpWindow = new QWidget(this, Qt::Window);   // a window of its own, closed with the main window
        m_helpWindow->setWindowTitle("Buster Coach — Help");
        auto *column = new QVBoxLayout(m_helpWindow);
        auto *buttons = new QHBoxLayout;
        auto *back = new QPushButton("← Back");
        auto *forward = new QPushButton("Forward →");
        auto *contents = new QPushButton("Contents");
        back->setEnabled(false);                         // nothing to go back to yet
        forward->setEnabled(false);
        for (QPushButton *button : {back, forward, contents})
            buttons->addWidget(button);
        buttons->addStretch();
        column->addLayout(buttons);
        auto *browser = new HelpBrowser;
        column->addWidget(browser);
        connect(back, &QPushButton::clicked, browser, &QTextBrowser::backward);
        connect(forward, &QPushButton::clicked, browser, &QTextBrowser::forward);
        connect(contents, &QPushButton::clicked, browser, &QTextBrowser::home);   // home = the first page shown
        connect(browser, &QTextBrowser::backwardAvailable, back, &QPushButton::setEnabled);
        connect(browser, &QTextBrowser::forwardAvailable, forward, &QPushButton::setEnabled);
        browser->setSource(QUrl("qrc:/help/index.html"));
        m_helpWindow->resize(90 * fontMetrics().averageCharWidth(), 40 * fontMetrics().height());   // font units
    }
    m_helpWindow->show();
    m_helpWindow->raise();                               // already open: to the front
    m_helpWindow->activateWindow();
}
