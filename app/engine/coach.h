// coach.h — the general coach: a second engine that scores every legal move of the human, in passes
// (6 -> 8 -> ... -> 14 plies; pass 2+ only the candidates). It never talks to the opponent engine: Game
// tells it via signals when the human's turn starts and ends; the board and the master coach tell it
// which moves deserve attention.

#pragma once
#include <QObject>
#include <QStringList>
#include <QHash>
#include <map>
#include "position.h"
#include "uci_info.h"

class Engine;   // only a pointer here

class Coach : public QObject
{
    Q_OBJECT

signals:
    // One of the human's moves has been scored, from the HUMAN's point of view (cp or mate).
    void moveScored(const QString &move, const UciInfo &score);

    // A pass is done: every move has a score (at this pass's depth or deeper).
    void analysisDone();

    // The threat searched: the opponent's best move if you passed (in pvMove), and the score for YOU after it.
    void threatSearched(const UciInfo &pass);

    // After every pass: the threat judged against your best move with the red-move rule.
    // The threat move if ignoring it would be a red move, "" if there's no direct threat.
    void threatJudged(const QString &move);

    // The opponent's turn: all its moves are scored (one pass, depth 6): opponentTable() is complete.
    void opponentTableReady();

public:
    // --- Construction ----------------------------------------------------------------------------
    Coach(Engine *engine, QObject *parent = nullptr);

public slots:
    // --- Slots: the turn, attention --------------------------------------------------------------
    // From Game: the human is to move. Analyse their possible moves.
    void onHumanTurnStarted(const QString &startFen, const QStringList &moves, const Position::Chess &position);
    // From Game: the opponent's engine thinks. Score the OPPONENT's moves once, for its pick (release_design.md 14).
    void onOpponentTurnStarted(const QString &startFen, const QStringList &moves, const Position::Chess &position);
    void cancel(); // drop the queue; stop a running search
    void engineRestarted();   // the coach engine was restarted: forget the old process, wait for its "ready"
    // From the board: the move under the box ("" = none). Deepened next, even a bad move.
    void setAttention(const QString &move);

    // From the master coach: its best move so far. Deepened in every pass, whatever its score.
    void setMasterMove(const UciInfo &best);

    // From the window: this turn's common reply among the red moves ("" = none) and how many it punishes (kind 3).
    void setCommonReply(const QString &reply, int count);


public:
    // --- The result ------------------------------------------------------------------------------
    // The scores found so far in this turn (move -> score, human's view). Empty at the start of a turn.
    const QHash<QString, UciInfo> &scores() const { return m_scores; }
    
    // The opponent's moves this turn (move -> score, the OPPONENT's view), in the type pickMove takes.
    const std::map<std::string, UciInfo> &opponentTable() const { return m_opponentTable; }

    // The strongest line after this move, in SAN, starting with the opponent's reply, at most maxPlies long
    // (e.g. "12... Bg4 13. e3 Bxf3"). "" if the move isn't scored or has no line (a game-ending move).
    QString lineText(const QString &move, int maxPlies = 4);

    // The same line as positions, for playing it through on the board: first your move, then up to maxPlies
    // of the line. Each step: the position AFTER the ply, and the ply's squares (for the last-move highlight).
    struct LineStep { Position::Chess position; int from; int to; };
    QList<LineStep> lineSteps(const QString &move, int maxPlies = 4);

    // The threat in words, for the coach line (learning_design.md): "Buster threatens …exf3, taking your knight.",
    // "No direct threat." ... "" if there's no pass this turn (in check) or it isn't searched yet.
    QString threatText();
    // The opponent's short name for the threat words ("Stockfish threatens ..."): the opponent's engine, not ours.
    void setOpponentName(const QString &name) { m_opponent = name; }

    // Why this move (why_design.md): the kind of good, or what a red move loses. So far kind 1: material won or lost
    // along its line, and the move where it lands ("Wins a piece on move 16."). "" = nothing to say (yet).
    // bad: a red move (it stops nothing: no kind 3). correct: this turn's correct move, for a red move without
    // material ("another move wins a pawn on move 5", never naming it: that would be a free hint).
    QString whyText(const QString &move, bool bad, const QString &correct);


private:
    // --- The engine and the rules ----------------------------------------------------------------
    Engine *m_engine;   // not owned: the main window owns the coach engine
    Position m_model;   // the rules: the human's legal moves
    Position::Chess m_position;   // the turn's position (the human to move): lines are played from here

    // --- The search running now ------------------------------------------------------------------
    QString m_current;  // the move being scored right now
    UciInfo m_lastInfo;  // the deepest info line so far for it
    void onInfo(const UciInfo &info);  // from Engine::infoReceived
    void onBestMove(const QString &);  // from Engine::bestMove: this move is done

    // --- The turn's work: queue, passes, scores --------------------------------------------------
    QString m_prefix;   // "position fen ... moves ..." for the current game (the human to move)
    QStringList m_queue;  // the human's moves still to score
    void scoreNext();  // start the next move in the queue, or report that all are done
    bool m_searching = false;   // a "go" is running (between go and its bestmove)
    int m_staleBestMoves = 0;  // bestmoves still to come from cancelled searches: skip them
    QHash<QString, UciInfo> m_scores;   // this turn's results so far
    bool m_forOpponent = false;         // this turn's work scores the opponent's moves (one pass, no threat)
    std::map<std::string, UciInfo> m_opponentTable;   // ... into here; m_scores (yours) stays untouched
    QStringList m_moves;   // all the human's legal moves this turn (each pass refills the queue from it)
    int m_depth = 6;       // the depth of the current pass
    QString m_masterMove;  // the master coach's best move this turn ("" = none yet)
    UciInfo m_pass;        // the pass's result this turn (the threat search; depth 0 = not searched yet)
    bool m_threatReal = false;   // the latest judgment: ignoring the threat would be a red move
    QString m_commonReply;     // this turn's common reply among the red moves ("" = none), from the window
    int m_commonCount = 0;     // how many red moves it punishes
    QString m_passPosition;  // "position fen ..." with the opponent to move again ("" = no pass this turn)
    UciInfo bestScore() const;   // the best score in the table so far (an empty UciInfo if none)
    // What a move newly attacks (why_design.md, kind 2), as a phrase: "attacks the bishop on c5, which is
    // undefended" / "gives check and attacks the rook on a8: a double attack". "" = nothing new. owner: "" or "your ".
    QString attackWords(const Position::Chess &before, const Position::Move &move, const QString &owner);
    // The why without the move's name (why_design.md): its phrase ("wins a pawn on move 5") and whether it's tactical
    // (mate, kinds 1-3) or positional (kind 4). An empty phrase = nothing to say.
    struct Why { QString phrase; bool tactical = false; };
    Why whyPhrase(const QString &move, bool bad);
    // A Buster move in words ("…exf3"), played in the swapped position; the plain UCI if it isn't legal there.
    QString replySan(const QString &uci);
    // How your move stops the capture `threat` (why_design.md, kind 3): "moves the knight away from …exf3" /
    // "takes the attacker (…exf3)" / "blocks …Bxf3" / "defends the knight against …exf3". "" = it doesn't stop it,
    // keeps: a target that wasn't hanging and still isn't counts too: "keeps the knight safe from …Bxf3" (the common reply).
    QString stopWords(const Position::Chess &before, const Position::Move &yours, const QString &threat, bool keeps);
    // How your move answers a QUIET `threat` (one that attacks rather than captures; Position::ThreatAnswer):
    // "takes the pawn before …d5" / "prevents …d5" / "moves the knight away before …d5" / "defends the bishop
    // against …Ng4" / "stops …d5" / "makes …Ng4 cost a knight". "" = not a quiet attacking threat, or not stopped.
    QString quietStopWords(const Position::Move &yours, const QString &threat);

    // --- The engine's handshake ------------------------------------------------------------------
    bool m_engineReady = false;   // the coach engine has finished its uci/isready handshake
    QString m_opponent = "Engine";   // the opponent's short name (setOpponentName)
    void onEngineReady();         // from Engine::ready: start any analysis that's waiting
};
