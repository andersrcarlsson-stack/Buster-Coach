// evalbar.h — a vertical bar beside the board: the player's winning chance, filled from the
// bottom (higher = better for you). A custom widget that paints itself, like BoardWidget.

#pragma once
#include <QWidget>
#include <algorithm>

class EvalBar : public QWidget
{
    Q_OBJECT

public:
    explicit EvalBar(QWidget *parent = nullptr);

    // The player's win chance, 0.0 .. 1.0; the bar repaints. text: shown instead of the percentage (e.g. "M3").
    void setWinChance(double chance, const QString &text = QString());

    // "If you play this": a frame up to this win chance, coloured by the move's drop vs the best (0 = best).
    void setPreview(double chance, double drop);
    void clearPreview();

    // Whose king sits at the bottom (you); Buster's sits at the top.
    void setPlayerWhite(bool white);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double m_chance = 0.5;   // the player's win chance, 0.0 .. 1.0 (from the master coach)
    QString m_text;          // shown at the fill's edge instead of the percentage ("" = the percentage)
    bool m_playerWhite = true;   // you play White (Game's default at start)
    double m_previewChance = -1.0;   // < 0: no preview shown
    double m_previewDrop = 0.0;
};
