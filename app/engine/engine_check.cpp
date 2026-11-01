// engine_check.cpp — the engine tests from the command line: ./build/engine_check /usr/games/stockfish
// The same EngineTester the engine page uses, without a window (QCoreApplication: an event loop, no widgets).

#include <QCoreApplication>
#include <cstdio>
#include "enginetester.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::printf("usage: engine_check <engine file>\n");
        return 1;
    }
    static const char *marks[] = {"✓", "⚠", "✗"};   // Good, Warn, No
    EngineTester tester;
    QObject::connect(&tester, &EngineTester::progress, [](const QString &step) {
        std::printf("  ... %s\n", qPrintable(step));
    });
    QObject::connect(&tester, &EngineTester::finished,
                     [&app](const QString &name, const TestResult &coach, const TestResult &opponent) {
        std::printf("%s\n  coach:    %s %s\n  opponent: %s %s\n", name.isEmpty() ? "(not a UCI engine)" : qPrintable(name),
                    marks[static_cast<int>(coach.fitness)], coach.reason.c_str(),
                    marks[static_cast<int>(opponent.fitness)], opponent.reason.c_str());
        app.quit();
    });
    tester.start(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}
