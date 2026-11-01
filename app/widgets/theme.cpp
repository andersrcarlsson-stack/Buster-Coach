// theme.cpp — light or dark (see theme.h).

#include "theme.h"
#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOption>
#include <QWidget>
#if QT_CONFIG(xcb)                  // Qt was built with X11 support: the xcb library is there
#include <xcb/xcb.h>
#include <cstdlib>
#include <cstring>
#endif

// The app's style: Fusion, with two small changes; everything else is still Fusion's.
// 1. Fusion draws a menu's separator in a fixed, faint black: invisible on a dark menu. This draws that one line
//    itself, in the palette's Mid colour.
// 2. Fusion draws every menu icon at 16 px. A menu with a "menuIconSize" property gets its icons at that size instead
//    (the promotion menu's pieces). Not "iconSize": Qt's own widgets have that property (QComboBox::iconSize asks the
//    style, and reading the property here would ask again: endless recursion, a crash at start-up).
class BusterStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;                 // the constructors: BusterStyle(QStyle *base)
    int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override {
        if (metric == PM_SmallIconSize && qobject_cast<const QMenu *>(widget)
            && widget->property("menuIconSize").isValid())
            return widget->property("menuIconSize").toInt();
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override {
        const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        if (element != CE_MenuItem || !item || item->menuItemType != QStyleOptionMenuItem::Separator) {
            QProxyStyle::drawControl(element, option, painter, widget);   // anything else: Fusion as before
            return;
        }
        QProxyStyle::drawControl(element, option, painter, widget);       // the menu's background behind it
        const int margin = 5, y = item->rect.center().y();
        painter->setPen(option->palette.color(QPalette::Mid));
        painter->drawLine(item->rect.left() + margin, y, item->rect.right() - margin, y);   // over Fusion's line
    }
};

static bool s_dark = false;   // the theme applyTheme set last (for fileDialogOptions)
void applyTheme(bool dark) {
    s_dark = dark;
    // Fusion: the same style on every desktop, and it follows any palette (some platform styles ignore a dark one).
    QApplication::setStyle(new BusterStyle(QStyleFactory::create("Fusion")));   // Fusion, with our two changes
    QPalette p = QApplication::style()->standardPalette();   // Fusion's own light palette
    if (dark) {
        const QColor text(230, 230, 230), disabled(127, 127, 127);
        p.setColor(QPalette::Window, QColor(53, 53, 53));      // the window's background
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, QColor(35, 35, 35));        // inside lists and text fields
        p.setColor(QPalette::AlternateBase, QColor(60, 60, 60));
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, QColor(60, 60, 60));
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::Mid, QColor(100, 100, 100));     // the menus' separators: a quiet grey
        p.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
        p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::BrightText, Qt::red);
        p.setColor(QPalette::Link, QColor(42, 130, 218));
        p.setColor(QPalette::Highlight, QColor(42, 130, 218)); // the selection
        p.setColor(QPalette::HighlightedText, Qt::black);
        p.setColor(QPalette::PlaceholderText, QColor(140, 140, 140));
        p.setColor(QPalette::Disabled, QPalette::Text, disabled);
        p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
        p.setColor(QPalette::Disabled, QPalette::Light, QColor(53, 53, 53));   // Fusion etches disabled text in Light:
                                                                               // the window's colour hides the etch
    }
    QApplication::setPalette(p);
}

void applyTitleBar(QWidget *window, bool dark) {
#if QT_CONFIG(xcb)
    auto *x11 = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11 || !x11->connection())
        return;                     // not on X11 (Wayland), or no X connection (offscreen): leave it to the desktop
    xcb_connection_t *connection = x11->connection();
    auto atom = [connection](const char *name) {   // X11 names its properties by numbers ("atoms"): look one up
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
            connection, xcb_intern_atom(connection, false, std::strlen(name), name), nullptr);
        const xcb_atom_t found = reply ? reply->atom : xcb_atom_t(XCB_ATOM_NONE);
        std::free(reply);           // xcb replies are C memory: free, not delete
        return found;
    };
    const xcb_window_t id = xcb_window_t(window->winId());   // the window's X11 id (created here if needed)
    if (dark) {
        const QByteArray variant = "dark";
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, id, atom("_GTK_THEME_VARIANT"), atom("UTF8_STRING"),
                            8, variant.size(), variant.constData());
    } else {
        xcb_delete_property(connection, id, atom("_GTK_THEME_VARIANT"));   // no hint: the desktop's normal frame
    }
    xcb_flush(connection);          // send it now, not with the next batch
#else
    Q_UNUSED(window);
    Q_UNUSED(dark);
#endif
}

QFileDialog::Options fileDialogOptions() {
    return s_dark ? QFileDialog::DontUseNativeDialog : QFileDialog::Options();   // Options(): none, the default
}
