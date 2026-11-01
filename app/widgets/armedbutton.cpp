// armedbutton.cpp — a button that asks before it acts (see armedbutton.h).

#include "armedbutton.h"
#include <QApplication>
#include <QEvent>

ArmedButton *ArmedButton::s_armed = nullptr;

ArmedButton::ArmedButton(const QString &text, const QString &question, const QString &hint, QWidget *parent)
    : QPushButton(text, parent), m_text(text), m_question(question), m_hint(hint) {
    m_timer.setSingleShot(true);
    m_timer.setInterval(3000);
    connect(&m_timer, &QTimer::timeout, this, &ArmedButton::disarm);
    connect(this, &QPushButton::clicked, this, [this]() {
        if (!m_arming) {
            emit fired();               // nothing to lose: no question
        } else if (m_armed) {
            disarm();
            emit fired();               // the second click
        } else {
            arm();                      // the first click
        }
    });
}

void ArmedButton::setArming(bool on) {
    m_arming = on;
    if (!on)
        disarm();
}

void ArmedButton::arm() {
    if (s_armed && s_armed != this)
        s_armed->disarm();              // one at a time
    s_armed = this;
    m_armed = true;
    setText(m_question);
    setStyleSheet("QPushButton { background: #c62828; color: white; font-weight: bold; }");
    m_timer.start();
    qApp->installEventFilter(this);     // see every event in the app (eventFilter) until disarmed
    emit armedChanged(true);
}

void ArmedButton::disarm() {
    if (!m_armed)
        return;
    m_armed = false;
    if (s_armed == this)
        s_armed = nullptr;
    setText(m_text);
    setStyleSheet(QString());
    m_timer.stop();
    qApp->removeEventFilter(this);
    emit armedChanged(false);
}

// While armed, the whole app's events pass here first. A mouse press on any OTHER widget disarms. (The press first
// reaches the window itself, which isn't a QWidget's press: qobject_cast filters that out; then the widget clicked.)
bool ArmedButton::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget != this)
            disarm();
    }
    return false;                       // never swallow the event: the click still does its own job
}
