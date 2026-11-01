// wingraph.h — the win-chance graph (graph_design.md): your win chance after every move, as a curve under the move
// list. Blue above the 50 % midline, grey below (as the eval bar); red dots on your red moves; an amber line at the
// move shown. The width covers max(40, moves played) full moves: early games fill the left part, long ones compress.
#pragma once
#include <QList>
#include <QWidget>

class WinGraph : public QWidget
{
    Q_OBJECT

signals:
    void plyClicked(int ply);                  // a click: the nearest ply (0 = the start, the last = the newest)

public:
    explicit WinGraph(QWidget *parent = nullptr);
    // The curve: one value per ply (0..1, -1 = not known yet: bridged), and which plies were your red moves.
    void setCurve(const QList<double> &values, const QList<bool> &red);
    void setShownPly(int ply);                 // the amber line (1 = after the first ply); -1 = the newest

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QList<double> m_values;
    QList<bool> m_red;
    int m_shownPly = -1;
};
