// pgn.cpp — writing a game record as PGN (pgn_design.md "Decisions").
#include "pgn.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <optional>
#include "move.h"
#include "position.h"

namespace {   // helpers for this file only (an unnamed namespace: invisible to the other files)

// One tag pair: [Name "value"]. PGN's rule: a quote or a backslash inside the value gets a backslash before it.
std::string tag(const std::string &name, const std::string &value) {
    std::string escaped;
    for (char c : value) {
        if (c == '"' || c == '\\')
            escaped += '\\';
        escaped += c;
    }
    return "[" + name + " \"" + escaped + "\"]\n";
}

// A win chance (0..1, from your side) as PGN's [%eval] in pawns from White's side. The win chance is
// 1 / (1 + 10^(-cp/400)) (lib/uci), so cp = -400 · log10(1/p - 1). Exactly 0 or 1 (a mate, or a score too large to
// tell apart): ±20 pawns, since the record keeps no mate distance.
std::string evalText(double chance, bool youAreWhite) {
    const double white = youAreWhite ? chance : 1.0 - chance;
    double pawns = white <= 0.0 ? -20.0 : white >= 1.0 ? 20.0 : -4.0 * std::log10(1.0 / white - 1.0);
    pawns = std::clamp(pawns, -20.0, 20.0);
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << pawns;           // always two decimals: "0.35", "-1.23"
    return "[%eval " + text.str() + "]";
}

// Words → lines under 80 characters (PGN's export form); a line breaks only between words.
std::string wrap(const std::vector<std::string> &words) {
    std::string out, line;
    for (const std::string &word : words) {
        if (!line.empty() && line.size() + 1 + word.size() > 79) {
            out += line + "\n";
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
    }
    return out + line + "\n";
}

// --- Reading -------------------------------------------------------------------------------------------------------
// The pieces of PGN text. Tag: [name "value"]; Comment: { … } or ; to the line's end; Open/Close: a side line's ( );
// Nag: $2; Result: 1-0, 0-1, 1/2-1/2, *; Word: anything else (a move number, a move).
enum class Kind { Tag, Comment, Open, Close, Nag, Result, Word };
struct Token {
    Kind kind;
    std::string text;      // a tag's name, a comment's text, a word
    std::string value {};  // a tag's value
};

std::vector<Token> tokenize(const std::string &s) {
    std::vector<Token> out;
    std::size_t i = 0;
    auto lineEnd = [&s](std::size_t from) { const std::size_t e = s.find('\n', from); return e == std::string::npos ? s.size() : e; };
    while (i < s.size()) {
        const char c = s[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
        } else if (c == '%' && (i == 0 || s[i - 1] == '\n')) {   // an escape line: PGN says skip it
            i = lineEnd(i);
        } else if (c == ';') {                                    // a comment to the end of the line
            const std::size_t e = lineEnd(i);
            out.push_back({Kind::Comment, s.substr(i + 1, e - i - 1)});
            i = e;
        } else if (c == '{') {                                    // a comment in braces (no nesting in PGN)
            const std::size_t e = s.find('}', i);
            const std::size_t end = e == std::string::npos ? s.size() : e;
            out.push_back({Kind::Comment, s.substr(i + 1, end - i - 1)});
            i = end + 1;
        } else if (c == '[') {                                    // a tag: [Name "value"], \" and \\ inside
            std::size_t j = i + 1;
            while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) ++j;
            const std::size_t nameStart = j;
            while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j])) && s[j] != '"' && s[j] != ']') ++j;
            Token tag{Kind::Tag, s.substr(nameStart, j - nameStart)};
            j = s.find('"', j);
            if (j != std::string::npos) {
                for (++j; j < s.size() && s[j] != '"'; ++j) {
                    if (s[j] == '\\' && j + 1 < s.size())
                        ++j;                                      // an escaped character: take the next one as it is
                    tag.value += s[j];
                }
            }
            const std::size_t close = s.find(']', j == std::string::npos ? i : j);
            out.push_back(tag);
            i = close == std::string::npos ? s.size() : close + 1;
        } else if (c == '(' || c == ')') {
            out.push_back({c == '(' ? Kind::Open : Kind::Close, std::string(1, c)});
            ++i;
        } else if (c == '$') {                                    // a numeric mark: $2 = "?"
            std::size_t j = i + 1;
            while (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) ++j;
            out.push_back({Kind::Nag, s.substr(i + 1, j - i - 1)});
            i = j;
        } else {                                                  // a word: up to a space or a special character
            std::size_t j = i;
            while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))
                   && std::string("{}()[];$").find(s[j]) == std::string::npos)
                ++j;
            const std::string word = s.substr(i, j - i);
            const bool result = word == "1-0" || word == "0-1" || word == "1/2-1/2" || word == "*";
            out.push_back({result ? Kind::Result : Kind::Word, word});
            i = j;
        }
    }
    return out;
}

// A tag into the record's info (the tags we write, and the common ones from elsewhere); unknown tags are skipped.
void applyTag(GameInfo &info, const std::string &name, const std::string &value) {
    if (name == "White") info.white = value;
    else if (name == "Black") info.black = value;
    else if (name == "Date") info.date = value;
    else if (name == "Time") info.time = value.substr(0, 5);           // "11:47:00" → "11:47"
    else if (name == "Result") info.result = value;
    else if (name == "TimeControl") info.timeControl = value;
    else if (name == "Level") info.level = std::atoi(value.c_str());
    else if (name == "ECO") info.eco = value;
    else if (name == "Opening") info.opening = value;
    else if (name == "FEN") info.startFen = value;
}


// [%eval x] (pawns, White's side; "#3" / "#-3" a mate) → the win chance from your side; -1 = no value in the comment.
double evalChance(const std::string &comment, bool youAreWhite) {
    const std::size_t at = comment.find("[%eval ");
    if (at == std::string::npos)
        return -1.0;
    const std::string value = comment.substr(at + 7, comment.find(']', at) - at - 7);
    double white;
    if (!value.empty() && value[0] == '#')
        white = std::atoi(value.c_str() + 1) > 0 ? 1.0 : 0.0;               // a mate: decided
    else
        white = 1.0 / (1.0 + std::pow(10.0, -std::atof(value.c_str()) * 100.0 / 400.0));   // pawns → win chance
    return youAreWhite ? white : 1.0 - white;
}

// A comment without its [%…] commands (the eval, lichess's clock, …), trimmed.
std::string commentText(const std::string &comment) {
    std::string text;
    for (std::size_t i = 0; i < comment.size(); ++i) {
        if (comment.compare(i, 2, "[%") == 0) {
            const std::size_t close = comment.find(']', i);
            if (close == std::string::npos)
                break;
            i = close;                                                  // skip the command
        } else if (std::isspace(static_cast<unsigned char>(comment[i]))) {
            if (!text.empty() && text.back() != ' ')
                text += ' ';                                    // a line break (the wrapping) or several spaces: one
        } else {
            text += comment[i];
        }
    }
    const std::size_t first = text.find_first_not_of(" \t\n"), last = text.find_last_not_of(" \t\n");
    return first == std::string::npos ? "" : text.substr(first, last - first + 1);
}

// A move as read, with what came around it: the position before it, a mark, its comments (interpreted later).
struct Read {
    MoveEntry move;
    Position::Chess before;
    bool marked = false;                 // "?", "??", "?!": a mistake
    bool only = false;                   // "!", "!!": the only good move
    std::vector<std::string> comments;
};
// A side line's tokens, and the main move it follows (it's an alternative to that move).
struct Side {
    std::size_t follows;
    std::vector<Token> tokens;
};

// Moves from tokens, played from `pos`: numbers skipped, marks and comments kept with their move. A move that doesn't
// play stops it (`problem` says which). Side lines: collected into `sides` (if given), else skipped.
std::vector<Read> playMoves(Position &model, const std::vector<Token> &tokens, Position::Chess pos, bool youAreWhite,
                            std::string &problem, std::vector<Side> *sides) {
    Moves mover;
    std::vector<Read> moves;
    int depth = 0;
    bool collecting = false;                                  // inside a side line that's being kept
    for (const Token &t : tokens) {
        if (t.kind == Kind::Open) {
            if (depth == 0) {
                collecting = sides && !moves.empty();         // a side line before any move: nothing to follow
                if (collecting)
                    sides->push_back({moves.size() - 1, {}});
            } else if (collecting) {
                sides->back().tokens.push_back(t);            // a nested one: kept inside its parent's tokens
            }
            ++depth;
            continue;
        }
        if (t.kind == Kind::Close) {
            depth = std::max(0, depth - 1);
            if (depth > 0 && collecting)
                sides->back().tokens.push_back(t);
            continue;
        }
        if (depth > 0) {
            if (collecting)
                sides->back().tokens.push_back(t);
            continue;
        }
        if (!problem.empty())
            continue;
        if (t.kind == Kind::Comment) {
            if (!moves.empty())
                moves.back().comments.push_back(t.text);      // a comment before the first move: skipped
        } else if (t.kind == Kind::Nag) {
            if (!moves.empty() && (t.text == "2" || t.text == "4" || t.text == "6"))
                moves.back().marked = true;                   // $2 ?, $4 ??, $6 ?!
            if (!moves.empty() && (t.text == "1" || t.text == "3"))
                moves.back().only = true;                     // $1 !, $3 !!
        } else if (t.kind == Kind::Word) {
            std::string word = t.text;
            const std::size_t digits = word.find_first_not_of("0123456789");
            if (digits == std::string::npos)
                continue;                                     // a bare number
            if (digits != 0 && word[digits] == '.') {         // a number first: "12." / "12..." / "1.e4"
                const std::size_t start = word.find_first_not_of('.', digits);
                if (start == std::string::npos)
                    continue;                                 // only the number ("12."): nothing to play
                word = word.substr(start);                    // "1.e4" → "e4"
            }
            std::optional<Position::Move> move = model.SanToMove(pos, word);
            if (!move) {
                problem = "move " + std::to_string(pos.fullmove_number) + ": no legal " + word;
                continue;
            }
            // The mark after the SAN: "?", "??", "?!" a mistake; "!", "!!" the only good move; "!?" neither.
            const std::size_t at = word.find_first_of("?!");
            const std::string mark = at == std::string::npos ? "" : word.substr(at);
            Read r{MoveEntry{}, pos, mark == "?" || mark == "??" || mark == "?!", mark == "!" || mark == "!!", {}};
            r.move.uci = model.MoveToUci(*move);
            r.move.san = model.MoveToSan(pos, *move);
            r.move.human = (!pos.pit) == youAreWhite;
            moves.push_back(r);
            pos = mover.make(pos, *move);
        }
    }
    return moves;
}

// A read move's mark and comments into its MoveEntry: the value, "Best was …" (the best move, UCI), our "Taken back."
// (dropped: the record's flag says it), the rest as notes.
void interpret(Position &model, Read &r, bool youAreWhite) {
    r.move.scored = r.marked || r.only;
    if (r.marked)
        r.move.quality = MoveQuality::Bad;
    if (r.only) {
        r.move.quality = MoveQuality::Correct;
        r.move.onlyMove = true;
    }
    for (const std::string &comment : r.comments) {
        const double chance = evalChance(comment, youAreWhite);
        if (chance >= 0)
            r.move.after = chance;
        std::string text = commentText(comment);
        if (text.rfind("Note: ", 0) == 0) {      // your own note (movelist_design.md 5), not the coach's
            r.move.yourNote = text.substr(6);
            continue;
        }
        const std::size_t taken = text.find("Taken back.");
        if (taken != std::string::npos)
            text = commentText(text.erase(taken, 11));
        const std::size_t best = text.find("Best was ");
        if (best != std::string::npos) {
            const std::size_t end = text.find('.', best + 9);
            const std::string san = text.substr(best + 9, end - best - 9);
            if (const std::optional<Position::Move> m = model.SanToMove(r.before, san))
                r.move.bestMove = model.MoveToUci(*m);
            text = commentText(text.erase(best, end == std::string::npos ? std::string::npos : end - best + 1));
        }
        if (!text.empty())
            r.move.notes.push_back(text);
    }
}

// A game's moves into its record: the main line, our taken-back tries (as the writer wrote them), in the order played.
void readMoves(PgnGame &game, const std::vector<Token> &tokens) {
    GameRecord &record = game.record;
    GameInfo &info = record.info();
    Position model;
    Moves mover;
    const std::optional<Position::Chess> start =
        model.FenInput(info.startFen.empty() ? Position::StartFen : info.startFen);
    if (!start) {
        game.problem = "the FEN can't be read";
        return;
    }
    const bool youAreWhite = info.black != "You";             // our files name you "You"; others: seen from White
    std::vector<Side> sides;
    std::vector<Read> moves = playMoves(model, tokens, *start, youAreWhite, game.problem, &sides);

    // The ending: the writer puts it as the last comment after the last move (when the move has one of its own too).
    std::vector<std::vector<std::vector<Read>>> tries(moves.size() + 1);   // [i]: the tries in place of main move i
    if (!moves.empty()) {
        std::vector<std::string> &last = moves.back().comments;
        if (last.size() >= 2) {
            info.ending = commentText(last.back());
            last.pop_back();
        }
        // A try at the very end, written as a comment: { Taken back: c3 }; played from the final position.
        for (auto it = last.begin(); it != last.end(); ++it) {
            const std::string text = commentText(*it);
            if (text.rfind("Taken back:", 0) != 0)
                continue;
            Position::Chess end = moves.back().before;
            std::optional<Position::Move> lastMove = model.UciToMove(end, moves.back().move.uci);
            end = mover.make(end, *lastMove);
            std::vector<Token> words;
            std::istringstream in(text.substr(11));
            for (std::string w; in >> w;)
                words.push_back({Kind::Word, w});
            std::string ignored;
            tries[moves.size()].push_back(playMoves(model, words, end, youAreWhite, ignored, nullptr));
            last.erase(it);
            break;
        }
    }
    // Our tries: side lines whose first move says "Taken back." (others, e.g. analysis lines, are skipped).
    for (const Side &side : sides) {
        std::string ignored;
        std::vector<Read> line = playMoves(model, side.tokens, moves[side.follows].before, youAreWhite, ignored, nullptr);
        if (!line.empty() && !line[0].comments.empty() && line[0].comments[0].find("Taken back.") != std::string::npos)
            tries[side.follows].push_back(line);
    }
    // Into the record in the order played: at each point the tries first (added, then taken back), then the move.
    for (std::size_t i = 0; i <= moves.size(); ++i) {
        for (std::vector<Read> &line : tries[i]) {
            for (Read &r : line) {
                interpret(model, r, youAreWhite);
                record.addMove(r.move);
            }
            record.takeBack(int(line.size()));
        }
        if (i < moves.size()) {
            interpret(model, moves[i], youAreWhite);
            record.addMove(moves[i].move);
        }
    }
}

} // namespace

std::string writePgn(const GameRecord &record) {
    const GameInfo &info = record.info();
    std::string out;
    // The Seven Tag Roster first, in its order (every PGN reader expects them), then the extras.
    out += tag("Event", "Buster Coach game");
    out += tag("Site", "Buster Coach");
    out += tag("Date", info.date.empty() ? "????.??.??" : info.date);   // PGN's "unknown" forms
    out += tag("Round", "-");
    out += tag("White", info.white.empty() ? "?" : info.white);
    out += tag("Black", info.black.empty() ? "?" : info.black);
    out += tag("Result", info.result);
    if (!info.time.empty())
        out += tag("Time", info.time + ":00");                   // PGN's form: HH:MM:SS
    out += tag("TimeControl", info.timeControl);
    if (info.level > 0)
        out += tag("Level", std::to_string(info.level));         // our own tag: other programs skip it
    if (!info.eco.empty()) {
        out += tag("ECO", info.eco);
        out += tag("Opening", info.opening);
    }
    if (!info.startFen.empty()) {                                // a set-up position: where the moves start
        out += tag("SetUp", "1");
        out += tag("FEN", info.startFen);
    }
    out += tag("Annotator", "Buster Coach");                     // who wrote the comments
    out += "\n";                                                 // a blank line between the tags and the moves

    // The moves of the game as it stands (taken-back ones: variations, step 2c), each as words for the wrapping.
    std::vector<std::string> words;
    auto say = [&words](const std::string &text) {               // free text: split into words
        std::istringstream in(text);
        for (std::string word; in >> word;)
            words.push_back(word);
    };
    // The game as it stands, and the tries taken back: a try is a run of taken-back moves that directly followed each
    // other in the record, at consecutive plies (two tries at the same point stay two).
    std::vector<const MoveEntry *> live;
    std::vector<std::vector<const MoveEntry *>> tries;
    const MoveEntry *previous = nullptr;
    for (const MoveEntry &m : record.moves()) {
        if (!m.takenBack)
            live.push_back(&m);
        else if (previous && previous->takenBack && previous->ply + 1 == m.ply)
            tries.back().push_back(&m);              // the same try goes on
        else
            tries.push_back({&m});                   // a new try, starting at its own ply
        previous = &m;
    }
    Position model;
    Moves mover;
    Position::Chess status =
        model.FenInput(info.startFen.empty() ? Position::StartFen
                                             : info.startFen).value();
    // Your side, from the first move: whose turn it was, and whether it was yours.
    const bool youAreWhite = live.empty() || live[0]->human == !status.pit;

    // One move: its number (White's always; Black's first and after a comment), the SAN with "?" on bad moves ("!" on
    // an only good move),
    // the comment (the value, `extra`, "Best was …", the notes); then `pos` moves on. False: not legal (can't happen).
    auto writeMove = [&](const MoveEntry &m, Position::Chess &pos, bool &needNumber, const std::string &extra) {
        if (!pos.pit)
            words.push_back(std::to_string(pos.fullmove_number) + ".");
        else if (needNumber)
            words.push_back(std::to_string(pos.fullmove_number) + "...");
        const bool bad = m.scored && m.quality == MoveQuality::Bad;   // yours, or the opponent's mistake on purpose
        words.push_back(m.san + (bad ? "?" : m.onlyMove ? "!" : ""));   // "!": your only good move
        const std::size_t before = words.size();
        words.push_back("{");
        if (m.after >= 0)
            words.push_back(evalText(m.after, youAreWhite));     // one word: "[%eval x]" is never split
        say(extra);
        if (bad && !m.bestMove.empty() && m.bestMove != m.uci)
            if (const std::optional<Position::Move> best = model.UciToMove(pos, m.bestMove))
                say("Best was " + model.MoveToSan(pos, *best) + ".");
        for (const std::string &note : m.notes)
            say(note);
        needNumber = words.size() > before + 1;  // anything said: the comment stays, and Black's move needs its number
        if (needNumber)
            words.push_back("}");
        else
            words.pop_back();                    // nothing to say: no empty { }
        if (!m.yourNote.empty()) {               // your own note: a comment of its own (movelist_design.md 5)
            std::string note = m.yourNote;
            std::replace(note.begin(), note.end(), '}', ')');   // a "}" would end the comment early
            words.push_back("{");
            say("Note: " + note);
            words.push_back("}");
            needNumber = true;
        }
        std::optional<Position::Move> move = model.UciToMove(pos, m.uci);
        if (!move)
            return false;
        pos = mover.make(pos, *move);
        return true;
    };

    bool needNumber = true;
    for (const MoveEntry *m : live) {
        const Position::Chess here = status;     // before the move: where its tries start too
        if (!writeMove(*m, status, needNumber, ""))
            break;                               // can't happen: the record holds the moves as played
        // The tries taken back at this point, as side lines after the move that replaced them: ( 4. Nxe5? … ).
        for (const auto &line : tries) {
            if (line.front()->ply != m->ply)
                continue;
            Position::Chess pos = here;
            bool number = true;                  // a side line always starts with its number
            words.push_back("(");
            for (const MoveEntry *t : line)
                if (!writeMove(*t, pos, number, t == line.front() ? "Taken back." : ""))
                    break;
            words.push_back(")");
            needNumber = true;                   // the game goes on after it: Black's move needs its number again
        }
    }
    // A try at the very end (taken back, then the game ended): no move to hang a side line on, so a comment.
    for (const auto &line : tries) {
        if (line.front()->ply < int(live.size()))
            continue;
        std::string text = "Taken back:";
        for (const MoveEntry *t : line)
            text += " " + t->san;
        words.push_back("{");
        say(text);
        words.push_back("}");
    }
    if (!info.ending.empty()) {                  // how it ended, in words, before the result
        words.push_back("{");
        say(info.ending);
        words.push_back("}");
    }
    words.push_back(info.result);
    out += wrap(words);
    return out;
}


// Every game in the text. A game starts with its tags (or with moves, if it has none) and ends with its result.
std::vector<PgnGame> readPgn(const std::string &text) {
    std::vector<PgnGame> games;
    std::vector<std::vector<Token>> movetext;   // each game's own tokens after its tags: read when it's complete
    bool inMoves = false;          // the current game's moves have begun: a tag now belongs to the next game
    bool ended = true;             // the current game's result has come: anything now starts the next game
    for (const Token &t : tokenize(text)) {
        const bool newGame = t.kind == Kind::Tag ? (ended || inMoves) : ended;
        if (newGame) {
            games.emplace_back();
            games.back().record.start();
            movetext.emplace_back();
            inMoves = ended = false;
        }
        GameInfo &info = games.back().record.info();
        if (t.kind == Kind::Tag) {
            applyTag(info, t.text, t.value);
        } else {
            inMoves = true;
            movetext.back().push_back(t);
            if (t.kind == Kind::Result) {
                info.result = t.text;              // the movetext's result wins over the tag (they should agree)
                ended = true;
            }
        }
    }
    for (std::size_t i = 0; i < games.size(); ++i)
        readMoves(games[i], movetext[i]);
    return games;

}
