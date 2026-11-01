// theme.h — light or dark (Looks, 2026-10-21): the application's style and palette. Standard widgets draw with the
// palette's colour roles (Window, Base, Text, ...), so they all follow; our own widgets should ask palette() too.
#pragma once
#include <QFileDialog>

void applyTheme(bool dark);

// The file dialogs' options for the current theme. Dark: Qt's own dialog (it follows our palette; the desktop's dialog
// follows the desktop's theme, often light). Light: the desktop's own dialog, as other programs show it.
QFileDialog::Options fileDialogOptions();


class QWidget;

// The window's title bar, drawn by the desktop: asks for its dark variant (X11: the _GTK_THEME_VARIANT hint, which
// Cinnamon/GNOME honour). Elsewhere it does nothing (Windows: todo.md Windows notes).
void applyTitleBar(QWidget *window, bool dark);
