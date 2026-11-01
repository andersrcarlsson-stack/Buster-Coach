// coach.cpp — the general coach: scoring the human's possible moves with the coach engine, in passes.
//
// Sections:
//   1. Settings: the deepest pass
//   2. Construction
//   3. A new turn: the moves to score (game-ending moves scored at once)
//   4. The engine's answers: info lines, bestmove
//   5. The queue and the passes; cancel
//   6. Attention: the move under the box, the master's move
//   7. The engine's handshake

#include "coach.h"
#include "engine.h"
#include "move.h"
#include <algorithm>
 #include <cstdlib>
 
// =================================================================================================
// 1. SETTINGS: the deepest pass
// =================================================================================================
// The deepest pass. Deeper passes cost more per move but score fewer candidates;
// the player's move cancels the rest anyway.
static const int maxDepth = 14;

// "0000" is UCI's null move. In the queue it stands for the pass (the threat search), never one of your moves.
static const QString passMove = "0000";


// =================================================================================================
// 2. CONSTRUCTION
// =================================================================================================
Coach::Coach(Engine *engine, QObject *parent)
    : QObject(parent), m_engine(engine)
{
    connect(m_engine, &Engine::infoReceived, this, &Coach::onInfo);
    connect(m_engine, &Engine::bestMove, this, &Coach::onBestMove);
    connect(m_engine, &Engine::ready, this, &Coach::onEngineReady);
}

// =================================================================================================
// 3. A NEW TURN: the moves to score (game-ending moves scored at once)
// =================================================================================================
// The human is to move: queue all their legal moves and start scoring them one by one.
void Coach::onHumanTurnStarted(const QString &startFen, const QStringList &moves, const Position::Chess &position) {
    cancel(); // an old analysis may still be running
    m_forOpponent = false;   // your turn: your moves, all passes, the threat
    m_position = position;   // kept: lineText() plays the lines from here
    m_prefix = "position fen " + startFen + " moves ";
    if (!moves.isEmpty())
        m_prefix += moves.join(' ') + " ";
    m_scores.clear(); // a new position: the old scores mean nothing here
    m_moves.clear();
    Moves mover;
    for (Position::Move m : m_model.LegalMoves(position).moves) {   // a copy: make() takes a non-const Move&
        const QString uci = QString::fromStdString(m_model.MoveToUci(m));
        const Position::GameState after = m_model.State(mover.make(position, m));
        if (after == Position::GameState::Ongoing) {
            m_moves << uci;                       // the engine scores it
        } else {
            // The move ends the game: exact, no search (the engine would answer "bestmove 0000" with no
            // score at all: G8). Depth 99 = exact, never deepened.
            const UciInfo end = (after == Position::GameState::Checkmate) ? UciInfo{99, true, 1}    // you mate
                                                                         : UciInfo{99, false, 0};  // a draw
            m_scores.insert(uci, end);
            emit moveScored(uci, end);
        }
    }
    m_queue = m_moves;   // pass 1: every move
    m_depth = 6;
    m_masterMove.clear();   // the master starts its search of this position from scratch
    // The threat first: the same position with the opponent to move again. Not when you're in check (PassTurn:
    // none), nor when the opponent would have no moves there (the engine would answer "bestmove 0000": G8).
    m_pass = UciInfo();
    m_threatReal = false;
    m_commonReply.clear();     // the window sends this turn's after the first verdicts
    m_commonCount = 0;
    m_passPosition.clear();
    std::optional<Position::Chess> passed = m_model.PassTurn(position);
    if (passed && m_model.State(*passed) == Position::GameState::Ongoing) {
        m_passPosition = "position fen " + QString::fromStdString(m_model.FenOutput(*passed));
        m_queue.prepend(passMove);
    }
    if (m_engineReady)
        scoreNext(); // otherwise onEngineReady starts it
}

// The opponent's engine thinks (release_design.md 14): score the OPPONENT's moves once, at depth 6, for its pick: no
// threat, no deeper passes. Your table (m_scores, m_moves) stays as it was: the record and the cards still read it.
void Coach::onOpponentTurnStarted(const QString &startFen, const QStringList &moves, const Position::Chess &position) {
    cancel();
    m_forOpponent = true;
    m_prefix = "position fen " + startFen + " moves ";
    if (!moves.isEmpty())
        m_prefix += moves.join(' ') + " ";
    m_opponentTable.clear();
    m_queue.clear();
    Moves mover;
    for (Position::Move m : m_model.LegalMoves(position).moves) {   // a copy: make() takes a non-const Move&
        const std::string uci = m_model.MoveToUci(m);
        const Position::GameState after = m_model.State(mover.make(position, m));
        if (after == Position::GameState::Ongoing)
            m_queue << QString::fromStdString(uci);
        else                                         // ends the game: exact, no search (as for yours)
            m_opponentTable[uci] = (after == Position::GameState::Checkmate) ? UciInfo{99, true, 1} : UciInfo{99, false, 0};
    }
    m_depth = 6;
    if (m_engineReady)
        scoreNext();
}

// =================================================================================================
// 4. THE ENGINE'S ANSWERS: info lines, bestmove
// =================================================================================================
// Iterative deepening sends one info line per depth: keep the latest (the deepest).
void Coach::onInfo(const UciInfo &info) {
    if (m_staleBestMoves > 0)
        return; // progress of a cancelled search
    m_lastInfo = info;
}

// The search of the position after m_current is finished. Its score is the OPPONENT's view
// (they are to move there), so negate it for the human. Works for mate too.
void Coach::onBestMove(const QString &) {
    if (m_staleBestMoves > 0) {  // the answer to a search we cancelled: not for this analysis
        --m_staleBestMoves;
        return;
    }
    m_searching = false;
    UciInfo score = m_lastInfo;
    score.score = -score.score;
    if (m_current == passMove) {          // the threat search: kept apart, it isn't one of your moves
        m_pass = score;
        emit threatSearched(score);
    } else if (m_forOpponent) {           // the opponent's move: into its own table
        m_opponentTable[m_current.toStdString()] = score;
    } else {
        m_scores.insert(m_current, score);
        emit moveScored(m_current, score);
    }
    scoreNext();
}

// =================================================================================================
// 5. THE QUEUE AND THE PASSES; CANCEL
// =================================================================================================
// The best score in the table so far: the yardstick for the candidates and for the threat.
UciInfo Coach::bestScore() const {
    UciInfo best;   // depth 0 = still the empty default
    for (const UciInfo &s : m_scores)
        if (winChance(s) > winChance(best) || best.depth == 0)
            best = s;
    return best;
}

// One search at a time: the engine answers each "go" with exactly one bestmove.
void Coach::scoreNext() {
    if (m_queue.isEmpty()) {
        if (m_forOpponent) {
            emit opponentTableReady();    // one pass only: the pick has what it needs
            return;
        }
        if (m_pass.depth > 0) {   // a pass was searched this turn (not in check)
            // Real: passing would be a red move. Not while you have a forced mate: you mate first (and passing
            // "misses" that mate, which isn't the opponent's threat).
            const UciInfo best = bestScore();
            const bool youMate = best.mate && best.score > 0;
            const bool real = !youMate && classifyMove(best, m_pass, false) == MoveQuality::Bad;
            m_threatReal = real;   // kept for threatText()
            emit threatJudged(real ? QString::fromStdString(m_pass.pvMove) : QString());
        }
        emit analysisDone();   // a pass is done: every move has a score at this depth (colours, bar)
        if (m_depth >= maxDepth)   // the last pass
            return;
        m_depth += 2;          // the next pass, 2 plies deeper
        const UciInfo best = bestScore();   // every move has a score after pass 1
        for (const QString &move : m_moves)    // candidates only: correct and playable moves are deepened,
            if (move == m_masterMove || classifyMove(best, m_scores.value(move), false) != MoveQuality::Bad)
                m_queue << move;  // bad moves keep the score they have
        if (m_queue.isEmpty())
            return;               // nothing to deepen (e.g. every move ends the game)
        if (m_queue.isEmpty())
            return;               // nothing to deepen (e.g. every move ends the game)
        if (!m_passPosition.isEmpty())
            m_queue.prepend(passMove);   // the threat again, at this pass's depth: step 3 compares like with like
    }
    m_current = m_queue.takeFirst();
    if (m_current == passMove)
        m_engine->send(m_passPosition);          // the pass: the opponent to move again
    else
        m_engine->send(m_prefix + m_current);   // the position AFTER the human's candidate move
    m_engine->send("go depth " + QString::number(m_depth));
    m_searching = true;
}

// Drop all pending work. A running search is stopped; its bestmove (UCI: it still comes) is skipped.
void Coach::cancel() {
    m_queue.clear();
    if (m_searching) {
        m_engine->send("stop");
        ++m_staleBestMoves;
        m_searching = false;
    }
}

// =================================================================================================
// 6. ATTENTION: the move under the box, the master's move
// =================================================================================================
// The player is looking at this move: put it first in the queue, so it is scored at this pass's
// depth next, whatever its score. If the passes are finished, it gets one search of its own.
void Coach::setAttention(const QString &move) {
    if (m_forOpponent || !m_moves.contains(move) || m_scores.value(move).depth >= m_depth)
        return;                 // "" or not a move here, or already scored at this depth
    m_queue.removeAll(move);    // it may be queued further back
    m_queue.prepend(move);
    if (m_engineReady && !m_searching)
        scoreNext();            // idle (all passes done): search it now
}

// The master's best move: put it first now, and keep it in every later pass (see scoreNext).
void Coach::setMasterMove(const UciInfo &best) {
    m_masterMove = QString::fromStdString(best.pvMove);
    setAttention(m_masterMove);   // scored at this pass's depth next, unless it already is
}

void Coach::setCommonReply(const QString &reply, int count) {
    m_commonReply = reply;
    m_commonCount = count;
}

// Play the move, then its line, on a copy of the turn's position, writing each ply in SAN BEFORE making it
// (SAN needs the position before the move). Numbered like a game score: "12... Bg4 13. e3 Bxf3".
QString Coach::lineText(const QString &move, int maxPlies) {
    const UciInfo info = m_scores.value(move);
    std::optional<Position::Move> first = m_model.UciToMove(m_position, move.toStdString());
    if (!first)
        return QString();
    Moves mover;
    Position::Chess pos = mover.make(m_position, *first);       // after the human's move: the opponent to move
    QStringList parts;
    for (int i = 0; i < int(info.pv.size()) && i < maxPlies; ++i) {
        std::optional<Position::Move> step = m_model.UciToMove(pos, info.pv[i]);
        if (!step)
            break;                                              // not legal here: stop the line
        const QString san = QString::fromStdString(m_model.MoveToSan(pos, *step));
        if (!pos.pit)
            parts << QString("%1. %2").arg(pos.fullmove_number).arg(san);          // White: "13. e3"
        else
            parts << (i == 0 ? QString("%1... %2").arg(pos.fullmove_number).arg(san)   // Black first: "12... Bg4"
                             : san);                                                   // Black after White: "Bxf3"
        pos = mover.make(pos, *step);
    }
    return parts.join(' ');
}

QList<Coach::LineStep> Coach::lineSteps(const QString &move, int maxPlies) {
    QList<LineStep> steps;
    std::optional<Position::Move> first = m_model.UciToMove(m_position, move.toStdString());
    if (!first)
        return steps;                                           // not a move here: nothing to play
    Moves mover;
    Position::Chess pos = mover.make(m_position, *first);
    steps << LineStep{pos, first->from_square, first->to_square};   // step 0: your move
    const std::vector<std::string> pv = m_scores.value(move).pv;
    for (int i = 0; i < int(pv.size()) && i < maxPlies; ++i) {
        std::optional<Position::Move> step = m_model.UciToMove(pos, pv[i]);
        if (!step)
            break;                                              // not legal here: stop the line
        pos = mover.make(pos, *step);
        steps << LineStep{pos, step->from_square, step->to_square};
    }
    return steps;
}

// An even exchange in words (why good): the pieces each side took, the biggest first. The same on both sides:
// "knight and pawn each"; else what you took, for what you gave: "knight for bishop".
static QString exchangeWords(const Position::Exchange &exchange) {
    static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};   // (code + 1) / 2
    auto types = [](const std::vector<int> &codes) {
        std::vector<int> t;
        for (int code : codes)
            t.push_back((code + 1) / 2);
        std::sort(t.rbegin(), t.rend());                 // sorted backwards: the biggest first
        return t;
    };
    auto words = [](const std::vector<int> &t) {
        QStringList parts;
        for (std::size_t i = 0; i < t.size();) {
            std::size_t n = 1;                           // how many of this kind: "2 pawns"
            while (i + n < t.size() && t[i + n] == t[i])
                ++n;
            parts << (n == 1 ? QString(names[t[i]]) : QString("%1 %2s").arg(n).arg(names[t[i]]));
            i += n;
        }
        return parts.join(" and ");
    };
    const std::vector<int> yours = types(exchange.yours), theirs = types(exchange.theirs);
    return yours == theirs ? words(yours) + " each" : words(yours) + " for " + words(theirs);
}

// A sacrifice in words (why_design.md, 2026-10-29): what the line first gives, for what. "the bishop for 2 pawns" /
// "the knight" / "a pawn" (a pawn is never "the": a gambit). The biggest first, as the even exchange.
static QString sacrificeWords(const Position::Exchange &settled) {
    static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};   // (code + 1) / 2
    auto list = [](const std::vector<int> &codes, bool definite) {
        std::vector<int> t;
        for (int code : codes)
            t.push_back((code + 1) / 2);
        std::sort(t.rbegin(), t.rend());                 // the biggest first
        QStringList parts;
        for (std::size_t i = 0; i < t.size();) {
            std::size_t n = 1;
            while (i + n < t.size() && t[i + n] == t[i])
                ++n;
            const QString name = names[t[i]];
            parts << (n > 1 ? QString("%1 %2s").arg(n).arg(name)
                            : (definite && name != "pawn" ? "the " : "a ") + name);
            i += n;
        }
        return parts.join(" and ");
    };
    const QString given = list(settled.theirs, true);   // your pieces, taken in the line
    return settled.yours.empty() ? given : given + " for " + list(settled.yours, false);
}

// An amount without a plain word (4, 6, 7, …) in pieces: what each side takes along the line, up to the ply where the
// balance lands; even trades on the way cancel. "the queen and the knight for a bishop and 2 pawns" (loses),
// "the rook and a pawn for a knight" (wins: what you took first).
static QString tradeWords(const std::vector<Position::MaterialStep> &steps, const Position::Gain &gain) {
    Position::Exchange traded{gain.amount, {}, {}};
    for (int i = 0; i <= gain.ply; ++i)
        if (steps[i].capture)
            (i % 2 == 0 ? traded.yours : traded.theirs).push_back(steps[i].taken);   // even plies are yours
    for (auto it = traded.yours.begin(); it != traded.yours.end();) {   // a piece for the same kind: cancels
        const auto same = std::find_if(traded.theirs.begin(), traded.theirs.end(),
                                       [&](int code) { return (code + 1) / 2 == (*it + 1) / 2; });
        if (same != traded.theirs.end()) {
            traded.theirs.erase(same);
            it = traded.yours.erase(it);
        } else {
            ++it;
        }
    }
    if (gain.amount > 0)
        std::swap(traded.yours, traded.theirs);       // sacrificeWords says "given for taken": a gain is the other way
    return sacrificeWords(traded);
}

// The threat in words. The pass's move in SAN, played in the swapped position (no move numbers: "…exf3" for Black).
QString Coach::threatText() {
    std::optional<Position::Chess> passed = m_model.PassTurn(m_position);
    std::optional<Position::Move> move;
    if (passed && m_pass.depth > 0)
        move = m_model.UciToMove(*passed, m_pass.pvMove);
    if (!move)
        return QString();                                      // no pass this turn, or not searched yet
    const QString san = QString(passed->pit ? "…" : "") + QString::fromStdString(m_model.MoveToSan(*passed, *move));
    static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen"};   // (code + 1) / 2
    const int captured = m_model.CapturedPiece(*passed, *move);   // 0 = none
    if (m_threatReal && m_pass.mate)                           // your view: the opponent mates in -score moves
        return m_pass.score == -1 ? m_opponent + " threatens mate: " + san + "."
                                  : QString("%1 threatens mate in %2, starting %3.").arg(m_opponent).arg(-m_pass.score).arg(san);
    // The rescue: the threat move's piece is itself hanging (your game, 6...Bxc3: the "threat" ...Bd4 is the bishop
    // escaping). Checked before the capture: a rescue can capture too, and "take it" matters more.
    const bool busterWhite = !passed->pit;                       // Buster is to move in the swapped position
    const std::vector<int> busterHanging = m_model.Hanging(m_position, busterWhite);
    const bool rescue = std::find(busterHanging.begin(), busterHanging.end(), move->from_square) != busterHanging.end();
    if (m_threatReal && rescue)
        return QString("%1's %2 on %3 is hanging: take it, or it escapes (%4).")
            .arg(m_opponent)
            .arg(names[(m_position.mailbox[move->from_square] + 1) / 2])
            .arg(QString::fromStdString(m_pass.pvMove.substr(0, 2)))
            .arg(san);
    if (m_threatReal && captured) {
        const bool undefended = m_model.Attackers(m_position, move->to_square, !busterWhite).empty();
        return QString("%1 threatens %2, taking your %3%4.")
            .arg(m_opponent, san, undefended ? "undefended " : "", names[(captured + 1) / 2]);
    }
    if (m_threatReal) {
        // A quiet threat: does its line win material later? Kind 1 of "why good", from Buster's side (why_design.md).
        const std::vector<Position::MaterialStep> steps = m_model.MaterialAlongLine(*passed, m_pass.pv);
        const Position::Gain gain = Position::StableGain(steps);
        // Within 6 plies of the threat move, as for your moves (2026-10-30): a gain further away isn't what the threat
        // is; then what the move attacks says it (kind 2), or "Ignoring it costs you."
        if (gain.amount > 0 && gain.ply <= 6) {
            // The swapped position's move number: after your pass as Black, White's next move is really N + 1.
            const int base = passed->fullmove_number + (passed->pit ? 0 : 1);
            const int number = base + (gain.ply + (passed->pit ? 1 : 0)) / 2;
            return QString("%1 threatens %2: it wins %3 on move %4.").arg(m_opponent, san, tradeWords(steps, gain)).arg(number);
        }
        // Kind 2: what the threat move newly attacks, played in the swapped position (your pieces: "your ").
        const QString attacks = attackWords(*passed, *move, "your ");
        if (!attacks.isEmpty())
            return m_opponent + " threatens " + san + ": it " + attacks + ".";
        return m_opponent + " threatens " + san + ". Ignoring it costs you.";   // the cost is positional (kinds 3-4 later)
    }
    if (captured)
        return m_opponent + " can take: " + san + ". Stopping it isn't worth it.";
    return "No direct threat.";
}

QString Coach::whyText(const QString &move, bool bad, const QString &correct) {
    const std::optional<Position::Move> m = m_model.UciToMove(m_position, move.toStdString());
    if (!m || !m_scores.contains(move))
        return QString();
    // The move's name first, so the sentence can't be read as being about the position: "g4: allows mate."
    const QString name = QString::fromStdString(m_model.MoveToSan(m_position, *m)) + ": ";
    Why why = whyPhrase(move, bad);
    // A red move without material, or one that WINS material but is still red (another move does much better, e.g.
    // mates): what the best move achieves, without naming it (the hint is H's job). A gain keeps its words first:
    // "Ra7: wins a pawn on move 2, but another move leads to mate." A loss explains itself. (The red box and the card's
    // red edge already say it's a mistake: the words say only what the best move does.)
    const bool gainOnly = bad && why.phrase.startsWith("wins ");
    if ((why.phrase.isEmpty() || gainOnly) && bad && !correct.isEmpty() && correct != move) {
        const Why best = whyPhrase(correct, false);
        if (gainOnly)
            why.phrase += ", but " + (best.tactical ? "another move " + best.phrase
                                                    : QString("the best move is much better"));
        else
            why.phrase = best.tactical ? "another move " + best.phrase
                                       : QString("a worse position than the best move (no tactics)");
    }
    return why.phrase.isEmpty() ? QString() : name + why.phrase + ".";
}

// The order (why_design.md): mate, the mate threat, a sacrifice, material (kind 1), stops (kind 3), attacks (kind 2), a
// principle (kind 4).
// Red moves: mate and material only here.
Coach::Why Coach::whyPhrase(const QString &move, bool bad) {
    const std::optional<Position::Move> m = m_model.UciToMove(m_position, move.toStdString());
    if (!m || !m_scores.contains(move))
        return {};
    const UciInfo info = m_scores.value(move);
    if (info.mate)                                       // a mate decides: material doesn't matter
        return {info.score > 0 ? "leads to mate" : "allows mate", true};
    // The opponent threatens mate and this move isn't red: it deals with the mate (else the coach would score it red).
    if (!bad && m_threatReal && m_pass.mate && m_pass.score < 0) {
        const QString san = replySan(QString::fromStdString(m_pass.pvMove));
        return {m_pass.score == -1 ? "stops the mate threat (" + san + ")"
                                   : "stops the mate threat (" + san + " starts it)", true};
    }
    std::vector<std::string> line{move.toStdString()};   // your move, then its line
    line.insert(line.end(), info.pv.begin(), info.pv.end());
    const std::vector<Position::MaterialStep> steps = m_model.MaterialAlongLine(m_position, line);
    const Position::Gain gain = Position::StableGain(steps);
    // A sacrifice (2026-10-29): a move that isn't red, whose line first GIVES material (it settles below level after
    // the reply) and doesn't come out ahead. The engine judges it worth it; "loses …" or "attacks …" would mislead.
    const Position::Exchange first = Position::FirstSettled(steps);
    if (!bad && first.balance < 0 && gain.amount <= 0) {
        const QString given = "gives " + sacrificeWords(first);
        if (gain.amount == 0 && gain.ply >= 0) {        // the line gets it back: where the balance last changed
            const int number = m_position.fullmove_number + (gain.ply + (m_position.pit ? 1 : 0)) / 2;
            return {given + QString(", and wins it back on move %1").arg(number), true};
        }
        return {given + ": the coach judges the play worth it", true};
    }
    // A gain far down the line isn't why a good move is good (and the line's tail is its least sure part): only within
    // 6 plies (3 moves each). A red move keeps its loss however far: that is why it's red. (2026-10-30, "d3: wins a pawn
    // on move 10." on an amber quiet move.)
    if (gain.amount != 0 && (bad || gain.ply <= 6)) {
        // Ply 0 is your move. Black to move: its ply 0 is in move N, its ply 1 (White) in move N + 1.
        const int number = m_position.fullmove_number + (gain.ply + (m_position.pit ? 1 : 0)) / 2;
        return {QString("%1 %2 on move %3").arg(gain.amount > 0 ? "wins" : "loses",
                                                 tradeWords(steps, gain)).arg(number), true};
    }
    if (bad)
        return {};                                       // a red move without material: "misses ..." (4c)
    // Kind 3 before kind 2: the thinking routine deals with the opponent's threat first.
    if (m_threatReal) {
        const QString stops = stopWords(m_position, *m, QString::fromStdString(m_pass.pvMove), false);
        if (!stops.isEmpty())
            return {stops + ", the threat", true};
        const QString quiet = quietStopWords(*m, QString::fromStdString(m_pass.pvMove));   // a threat that attacks
        if (!quiet.isEmpty())
            return {quiet + ", the threat", true};
    }
    // The second source: the red moves' common reply, if this move's line doesn't run into it.
    if (!m_commonReply.isEmpty() && QString::fromStdString(info.pvMove) != m_commonReply) {
        const QString stops = stopWords(m_position, *m, m_commonReply, true);
        if (!stops.isEmpty())
            return {stops + QString(", which punishes %1 other moves").arg(m_commonCount), true};
    }
    // The even exchange: your capture starts a chain of captures that ends level (the material rule saw nothing).
    const Position::Exchange exchange = Position::FirstExchange(steps);
    if (!exchange.yours.empty() && exchange.balance == 0)
        return {"starts an even exchange (" + exchangeWords(exchange) + ")", false};
    // Kind 2, unless the line's reply takes your moved piece at once: then "attacks …" misses the point (the line is a
    // trade or a sacrifice, said above when it costs material).
    const bool takenAtOnce = steps.size() >= 2 && steps[1].capture && !info.pv.empty()
                             && info.pv[0].substr(2, 2) == move.mid(2, 2).toStdString();
    const QString attacks = takenAtOnce ? QString() : attackWords(m_position, *m, QString());
    if (!attacks.isEmpty())
        return {attacks, true};
    // Kind 4: a visible opening principle, else a quiet move.
    switch (m_model.MovePrinciple(m_position, *m)) {
    case Position::Principle::Castles:
        return {"castles: the king is safe, the rook joins", false};
    case Position::Principle::Develops:
        return {(m_position.mailbox[m->from_square] + 1) / 2 == 2 ? "develops the knight" : "develops the bishop", false};
    case Position::Principle::Centre:
        return {"takes the centre", false};
    case Position::Principle::None:
        break;
    }
    return {"a quiet move: no tactics", false};
}

QString Coach::attackWords(const Position::Chess &before, const Position::Move &move, const QString &owner) {
    static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};   // (code + 1) / 2
    Moves mover;
    Position::Move m = move;                                     // a copy: make() takes a non-const Move&
    const Position::Chess after = mover.make(before, m);
    const bool check = mover.in_check(after, 0, 0);              // the opponent is to move there
    const std::vector<int> fresh = m_model.NewThreats(before, move);
    QStringList targets;
    for (int sq : fresh)
        targets << (owner.isEmpty() ? QString("the ") : owner) + names[(after.mailbox[sq] + 1) / 2]
                   + " on " + QChar('a' + sq % 8) + QString::number(sq / 8 + 1);
    QString phrase;
    if (!targets.isEmpty()) {
        phrase = "attacks " + targets.join(" and ");
        if (fresh.size() == 1) {                                 // one target: why it's hanging
            const bool targetWhite = after.mailbox[fresh[0]] % 2 == 0;
            phrase += m_model.Attackers(after, fresh[0], targetWhite).empty() ? ", which is undefended"
                                                                              : ", with a cheaper piece";
        }
    }
    if (check)
        phrase = phrase.isEmpty() ? QString("gives check") : "gives check and " + phrase;
    if (int(fresh.size()) + (check ? 1 : 0) >= 2)
        phrase += ": a double attack";
    return phrase;
}

QString Coach::replySan(const QString &uci) {
    const std::optional<Position::Chess> passed = m_model.PassTurn(m_position);
    const std::optional<Position::Move> m = passed ? m_model.UciToMove(*passed, uci.toStdString()) : std::nullopt;
    if (!m)
        return uci;
    return QString(passed->pit ? "…" : "") + QString::fromStdString(m_model.MoveToSan(*passed, *m));
}

QString Coach::quietStopWords(const Position::Move &yours, const QString &threat) {
    if (threat.size() < 4)
        return QString();
    const int target = (threat[3].toLatin1() - '1') * 8 + (threat[2].toLatin1() - 'a');
    if (m_position.mailbox[target] != 0)
        return QString();                                // a capture: stopWords' case
    static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};   // (code + 1) / 2
    const Position::Answer answer = m_model.ThreatAnswer(m_position, yours, threat.toStdString());
    const QString piece = names[(answer.piece + 1) / 2];
    const QString san = replySan(threat);
    switch (answer.how) {
    case Position::How::Takes:     return "takes the " + piece + " before " + san;
    case Position::How::Prevents:  return "prevents " + san;
    case Position::How::MovesAway: return "moves the " + piece + " away before " + san;
    case Position::How::Defends:   return "defends the " + piece + " against " + san;
    case Position::How::Stops:     return "stops " + san;
    case Position::How::CostsIt:   return "makes " + san + " cost a " + piece;
    case Position::How::None:      break;
    }
    return QString();
}

QString Coach::stopWords(const Position::Chess &before, const Position::Move &yours, const QString &threat, bool keeps) {
    if (threat.size() < 4)
        return QString();
    auto square = [&threat](int at) { return (threat[at + 1].toLatin1() - '1') * 8 + (threat[at].toLatin1() - 'a'); };
    const int attacker = square(0), target = square(2);
    const int victim = before.mailbox[target];
    if (victim == 0)
        return QString();                                // not a capture: nothing of yours to save
    const bool white = victim % 2 == 0;                  // the victim's side: yours
    const std::vector<int> hangingBefore = m_model.Hanging(before, white);
    const bool wasHanging = std::find(hangingBefore.begin(), hangingBefore.end(), target) != hangingBefore.end();
    if (!wasHanging && !keeps)
        return QString();                                // not en prise to begin with: nothing we can judge
    Moves mover;
    Position::Move m = yours;                            // a copy: make() takes a non-const Move&
    const Position::Chess after = mover.make(before, m);
    const int now = (yours.from_square == target) ? yours.to_square : target;   // where the piece stands afterwards
    const std::vector<int> hangingAfter = m_model.Hanging(after, white);
    if (std::find(hangingAfter.begin(), hangingAfter.end(), now) != hangingAfter.end())
        return QString();                                // still hanging: not stopped
    static const char *names[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};   // (code + 1) / 2
    const QString piece = names[(victim + 1) / 2];
    const QString san = replySan(threat);
    if (yours.from_square == target)
        return "moves the " + piece + " away from " + san;
    if (yours.to_square == attacker)
        return "takes the attacker (" + san + ")";
    if (!wasHanging)
        return "keeps the " + piece + " safe from " + san;   // safe before, still safe: the common reply's case
    const std::vector<int> attackers = m_model.Attackers(after, target, !white);
    if (std::find(attackers.begin(), attackers.end(), attacker) == attackers.end())
        return "blocks " + san;
    return "defends the " + piece + " against " + san;
}

// =================================================================================================
// 7. THE ENGINE'S HANDSHAKE
// =================================================================================================
// The coach engine is ready. If a turn started before that, its queue is waiting: start it now.
void Coach::onEngineReady() {
    m_engineReady = true;
    if (!m_queue.isEmpty() && !m_searching)
        scoreNext();
}

void Coach::engineRestarted() {
    m_engineReady = false;      // the queue waits for the new process's "ready" (onEngineReady starts it)
    m_searching = false;        // the old search died with the old process ...
    m_staleBestMoves = 0;       // ... so no bestmove from it will come to be skipped
}
