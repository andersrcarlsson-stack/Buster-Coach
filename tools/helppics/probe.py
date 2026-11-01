#!/usr/bin/env python3
"""probe.py FEN [depth] [from-square]: every legal move (or only those from one square, e.g. f3) scored the way the
coach does it (the position after the move, go depth N, default 12), with its verdict by the app's rule; then the pass
(the threat). Quick, one depth only: coachtext gives the coach's real verdicts and words. Needs build/moves."""
import subprocess, sys, math
fen = sys.argv[1]; depth = int(sys.argv[2]) if len(sys.argv) > 2 else 12
only = sys.argv[3] if len(sys.argv) > 3 else ""
here = __file__.rsplit('/', 1)[0]
lines = subprocess.run([here + "/build/moves", fen], capture_output=True, text=True).stdout.split("\n")
eng = subprocess.Popen([here + "/../../app/build/buster"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
def send(s): eng.stdin.write(s + "\n"); eng.stdin.flush()
def search(pos):
    send(pos); send(f"go depth {depth}"); last = None
    while True:
        l = eng.stdout.readline().split()
        if not l: continue
        if l[0] == "info" and "score" in l:
            i = l.index("score"); last = (l[i+1], int(l[i+2]), l[l.index("pv")+1:] if "pv" in l else [])
        if l[0] == "bestmove": return last
send("uci"); send("isready")
while eng.stdout.readline().strip() != "readyok": pass
def wc(kind, v): return (1.0 if v > 0 else 0.0) if kind == "mate" else 1/(1+10**(-v/400))
res = []
for l in lines:
    if not l or l.startswith("pass"): continue
    uci, san = l.split()
    if only and not uci.startswith(only): continue
    kind, v, pv = search(f"position fen {fen} moves {uci}")
    res.append((san, uci, kind, -v, pv))   # negated: the mover's view
best = max(res, key=lambda r: wc(r[2], r[3]))
for san, uci, kind, v, pv in sorted(res, key=lambda r: -wc(r[2], r[3])):
    bad = wc(best[2], best[3]) - wc(kind, v) >= 0.15 or (kind == best[2] == "cp" and best[3] - v >= 200) \
          or (kind == "mate" and v < 0 and not (best[2] == "mate" and best[3] < 0)) \
          or (best[2] == "mate" and best[3] > 0 and not (kind == "mate" and v > 0))
    print(f"{san:8} {kind} {v:6}  {'RED' if bad else ('best' if (san, uci) == best[:2] else '')}  reply: {' '.join(pv[:6])}")
for l in lines:
    if l.startswith("pass") and not only:
        kind, v, pv = search(f"position fen {l[5:]}")
        print(f"PASS: opponent {kind} {v} pv {' '.join(pv[:6])}")
send("quit")
