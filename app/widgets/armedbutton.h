// armedbutton.h — a button that asks before it acts (buttons_design.md): the first click ARMS it (red, a question),
// a second click within 3 s FIRES it. Waiting, a click anywhere else, or another armed button disarms it quietly.
// With arming off (no game in progress), a click fires at once.
#pragma once
#include <QPushButton>
#include <QTimer>

class ArmedButton : public QPushButton
{
    Q_OBJECT

signals:
    void fired();                       // the action: the second click (or the first, with arming off)
    void armedChanged(bool armed);      // for a hint line next to the buttons

public:
    ArmedButton(const QString &text, const QString &question, const QString &hint, QWidget *parent = nullptr);
    void setArming(bool on);            // false: a click fires at once
    QString hint() const { return m_hint; }   // "Click again within 3 s to resign."

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;   // while armed: a click elsewhere disarms

private:
    void arm();
    void disarm();

    QString m_text;                     // "Resign"
    QString m_question;                 // "Resign? ✓"
    QString m_hint;
    bool m_arming = true;
    bool m_armed = false;
    QTimer m_timer;                     // 3 s, then it disarms
    static ArmedButton *s_armed;        // the one armed button (one at a time)
};
