#include "windowshadow.h"

#include <QWindow>

#ifdef KANZI_HAVE_KWINDOWSYSTEM

#include <KWindowShadow>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <vector>

namespace {

// Breeze-like look, but even on every edge: a wide, faint ambient shadow plus a
// tighter, darker key shadow, both centred on the window.
constexpr int kSize = 40;   // how far the shadow can reach past the window
constexpr int kSpread = 8;  // how far the key shadow's shape extends past the window

// Three box blurs of the alpha channel approximate a Gaussian.
void blurAlpha(QImage &img, int radius)
{
    if (radius < 1)
        return;
    const int w = img.width(), h = img.height();
    std::vector<int> a(size_t(w) * h), tmp(a.size());
    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < w; ++x)
            a[size_t(y) * w + x] = qAlpha(line[x]);
    }
    const int r = std::max(1, radius / 3);
    const int span = 2 * r + 1;
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) { // horizontal
            int sum = 0;
            for (int x = -r; x <= r; ++x)
                sum += a[size_t(y) * w + std::clamp(x, 0, w - 1)];
            for (int x = 0; x < w; ++x) {
                tmp[size_t(y) * w + x] = sum / span;
                sum += a[size_t(y) * w + std::min(x + r + 1, w - 1)] - a[size_t(y) * w + std::max(x - r, 0)];
            }
        }
        for (int x = 0; x < w; ++x) { // vertical
            int sum = 0;
            for (int y = -r; y <= r; ++y)
                sum += tmp[size_t(std::clamp(y, 0, h - 1)) * w + x];
            for (int y = 0; y < h; ++y) {
                a[size_t(y) * w + x] = sum / span;
                sum += tmp[size_t(std::min(y + r + 1, h - 1)) * w + x] - tmp[size_t(std::max(y - r, 0)) * w + x];
            }
        }
    }
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x)
            line[x] = qRgba(0, 0, 0, a[size_t(y) * w + x]); // black, so premultiplied == straight
    }
}

QImage layer(QSize size, const QRectF &rect, qreal radius, qreal opacity, int blur)
{
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, qRound(255 * opacity)));
    p.drawRoundedRect(rect, radius, radius);
    p.end();
    blurAlpha(img, blur);
    return img;
}

KWindowShadowTile::Ptr tile(const QImage &source, const QRect &rect)
{
    auto t = KWindowShadowTile::Ptr::create();
    t->setImage(source.copy(rect));
    return t;
}

void configure(KWindowShadow *shadow, int radius)
{
    // A tiny "window" with the shadow around it, then cut into the 8 tiles KWin stretches.
    const int core = 2 * radius + 1;
    const QMargins pad(kSize, kSize, kSize, kSize);
    const QSize size(pad.left() + core + pad.right(), pad.top() + core + pad.bottom());
    const QRectF win(pad.left(), pad.top(), core, core);

    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    const qreal s = kSpread;
    p.drawImage(0, 0, layer(size, win.adjusted(-3, -3, 3, 3), radius + 3, 0.20, 34)); // ambient
    p.drawImage(0, 0, layer(size, win.adjusted(-s, -s, s, s), radius + s, 0.38, 14)); // key
    // Nothing under the window itself: its rounded corners are transparent.
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.drawRoundedRect(win, radius, radius);
    p.end();

    const int L = pad.left(), T = pad.top(), R = pad.right(), B = pad.bottom();
    shadow->setTopLeftTile(tile(img, QRect(0, 0, L + radius, T + radius)));
    shadow->setTopTile(tile(img, QRect(L + radius, 0, 1, T)));
    shadow->setTopRightTile(tile(img, QRect(L + radius + 1, 0, R + radius, T + radius)));
    shadow->setLeftTile(tile(img, QRect(0, T + radius, L, 1)));
    shadow->setRightTile(tile(img, QRect(L + core, T + radius, R, 1)));
    shadow->setBottomLeftTile(tile(img, QRect(0, T + radius + 1, L + radius, B + radius)));
    shadow->setBottomTile(tile(img, QRect(L + radius, T + core, 1, B)));
    shadow->setBottomRightTile(tile(img, QRect(L + radius + 1, T + radius + 1, R + radius, B + radius)));
    shadow->setPadding(pad);
}

KWindowShadow *shadowOf(QWindow *window)
{
    return window->findChild<KWindowShadow *>(QString(), Qt::FindDirectChildrenOnly);
}

} // namespace

bool WindowShadow::available()
{
    const QString platform = QGuiApplication::platformName();
    return platform.startsWith(u"wayland") || platform == u"xcb";
}

bool WindowShadow::apply(QWindow *window, int radius)
{
    if (!window || !available())
        return false;
    KWindowShadow *shadow = shadowOf(window);
    if (!shadow || shadow->property("kanziRadius").toInt() != radius) {
        delete shadow;
        shadow = new KWindowShadow(window);
        shadow->setProperty("kanziRadius", radius);
        configure(shadow, radius);
        shadow->setWindow(window);
    }
    // The native surface may be new since the last show; always re-create.
    shadow->destroy();
    const bool ok = shadow->create();
    if (!ok)
        qWarning("Kanzi: the compositor didn't accept the window shadow");
    return ok;
}

void WindowShadow::remove(QWindow *window)
{
    if (window)
        delete shadowOf(window);
}

#else

bool WindowShadow::available() { return false; }
bool WindowShadow::apply(QWindow *, int) { return false; }
void WindowShadow::remove(QWindow *) {}

#endif
