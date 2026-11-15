#pragma once

class QWindow;

// Compositor-drawn drop shadows for frameless windows (KWin's org_kde_kwin_shadow on
// Wayland, _KDE_NET_WM_SHADOW on X11) via KWindowSystem's KWindowShadow. Without
// KWindowSystem at build time these are no-ops.
namespace WindowShadow {

bool available();
// (Re)attaches a Breeze-style shadow that hugs a rounded window with corner `radius`.
// Call it again whenever the window is shown: the native surface may have been recreated.
bool apply(QWindow *window, int radius);
void remove(QWindow *window);

} // namespace WindowShadow
