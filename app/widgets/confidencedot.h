// confidencedot.h — how settled the coach's verdicts are (coach_design.md "Coach Confidence: a dot"): a small dot in
// the status bar, from a yellow glow (still searching / still changing) to deep green (settled).
#pragma once
#include <QWidget>

class ConfidenceDot : public QWidget
{
public:
    explicit ConfidenceDot(QWidget *parent = nullptr);
    // Stable passes in a row: 0 (yellow) .. 3 (deep green, settled).
    void setStable(int stable);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int m_stable = 0;
};
