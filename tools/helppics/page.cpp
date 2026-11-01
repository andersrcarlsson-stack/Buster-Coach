// page <page.html> <out.png> [height]: a Help page as QTextBrowser shows it, 800 px wide, saved as a picture: a look
// at a page's layout without starting the app (after python3 tools/help_pages.py).
#include <QApplication>
#include <QTextBrowser>
int main(int argc, char *argv[]) {
    if (argc < 3)
        return 1;
    QApplication app(argc, argv);
    QTextBrowser b;
    b.resize(800, argc > 3 ? atoi(argv[3]) : 4200);    // tall enough for the whole page: no scrolling
    b.setSearchPaths({HELP_DIR});
    b.setSource(QUrl::fromLocalFile(QString(HELP_DIR "/") + argv[1]));
    b.show();
    app.processEvents();
    b.grab().save(argv[2]);
}
