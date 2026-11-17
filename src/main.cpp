#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "applemusic.h"
#include "audiooutput.h"
#include "discordpresence.h"
#include "mpris.h"
#include "networkfactory.h"
#include "player.h"
#include "plex.h"
#include "tray.h"

static QString findWidevine()
{
    if (const QByteArray env = qgetenv("KANZI_WIDEVINE_PATH"); !env.isEmpty())
        return QString::fromLocal8Bit(env);

    const QStringList candidates = {
        QStringLiteral("/opt/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"), // AUR: widevine
        QStringLiteral("/opt/google/chrome/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"),
        QStringLiteral("/usr/lib/chromium/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"),
        QStringLiteral("/usr/lib/chromium/libwidevinecdm.so"),
        QStringLiteral("/usr/lib/qt6/plugins/ppapi/libwidevinecdm.so"),
        QStringLiteral("/usr/lib64/chromium-browser/WidevineCdm/_platform_specific/linux_x64/libwidevinecdm.so"),
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return c;
    }
    return {};
}

int main(int argc, char *argv[])
{
    // Prefer the native Wayland backend whenever we're in a Wayland session.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "wayland;xcb");

    QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    const QString widevine = findWidevine();
    if (!widevine.isEmpty())
        flags += " --widevine-path=" + QFile::encodeName(widevine);
    flags += " --autoplay-policy=no-user-gesture-required";
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags.trimmed());
    // KANZI_DEVTOOLS=1 exposes the player and sign-in pages at http://localhost:9222
    if (qEnvironmentVariableIsSet("KANZI_DEVTOOLS") && qEnvironmentVariableIsEmpty("QTWEBENGINE_REMOTE_DEBUGGING"))
        qputenv("QTWEBENGINE_REMOTE_DEBUGGING", "9222");

    QCoreApplication::setOrganizationName(QStringLiteral("Kanzi"));
    QCoreApplication::setApplicationName(QStringLiteral("Kanzi"));
    QCoreApplication::setApplicationVersion(QStringLiteral(KANZI_VERSION));

    QtWebEngineQuick::initialize();

    // QApplication (not QGuiApplication) because the tray icon's menu is a QMenu.
    QApplication app(argc, argv);
    // Windows get hidden to the tray and the mini player; quitting is explicit.
    QApplication::setQuitOnLastWindowClosed(false);
    // Becomes the xdg-shell app_id on Wayland, which compositors use to match the .desktop file.
    QGuiApplication::setDesktopFileName(QStringLiteral("dev.kanzi.Kanzi"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Kanzi/resources/kanzi.svg")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    if (widevine.isEmpty())
        qWarning("Kanzi: no Widevine CDM found; playback will fail. Set KANZI_WIDEVINE_PATH.");

    AppleMusic api(nullptr);
    Player player(&api);
    Plex plex(&player, nullptr);
    player.connectPlex(&plex);
    Mpris mpris(&player);
    AudioOutput audioOutput(nullptr);
    Tray tray(&player, nullptr);
    DiscordPresence discord(&player, nullptr);

    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(new CachingNetworkFactory);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Kanzi", "Main");

    return app.exec();
}
