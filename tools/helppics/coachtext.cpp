// coachtext <fen> [move ...]: the real Coach's words for a position (all five passes, the built-in Buster): each move's
// verdict, why and line; the threat. The sentences the Help's cards show (help pictures: helppics.cpp).
#include <QCoreApplication>
#include <QTimer>
#include "coach.h"
#include "engine.h"
#include <iostream>
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    Engine engine;
    Coach coach(&engine);
    coach.setOpponentName("Buster");
    const QString fen = argv[1];
    int passes = 0;
    QObject::connect(&coach, &Coach::analysisDone, [&]() { ++passes; });
    QObject::connect(&engine, &Engine::ready, [&]() {
        coach.onHumanTurnStarted(fen, {}, *Position().FenInput(fen.toStdString()));
    });
    engine.start(BUSTER);
    QTimer poll; poll.start(100);
    QTimer::singleShot(60000, [&]() { passes = 5; });   // a mating move ends the passes early: report what there is
    QObject::connect(&poll, &QTimer::timeout, [&]() {
        if (passes < 5) return;
        const auto &scores = coach.scores();
        QString best; for (auto it = scores.begin(); it != scores.end(); ++it)
            if (best.isEmpty() || winChance(it.value()) > winChance(scores.value(best))) best = it.key();
        std::cout << "best " << best.toStdString() << "  threat: " << coach.threatText().toStdString() << "\n";
        QStringList moves; for (int i = 2; i < argc; ++i) moves << argv[i];
        if (moves.isEmpty()) moves = scores.keys();
        for (const QString &m : moves) {
            const MoveQuality q = classifyMove(scores.value(best), scores.value(m), m == best);
            const bool bad = q == MoveQuality::Bad;
            std::cout << m.toStdString() << " [" << (bad ? "RED" : q == MoveQuality::Correct ? "green" : "amber") << "] "
                      << coach.whyText(m, bad, best).toStdString() << "  | " << coach.lineText(m).toStdString() << "\n";
        }
        app.quit();
    });
    return app.exec();
}
