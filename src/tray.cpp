#include "tray.h"

#include "player.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryFile>
#include <QTimer>
#include <QMenu>
#include <QSettings>
#include <QSystemTrayIcon>

namespace {
const QString kEnabled = QStringLiteral("tray/enabled");
const QString kCloseToTray = QStringLiteral("tray/closeToTray");
const QString kStartInTray = QStringLiteral("tray/startInTray");
const QString kMiniOnTop = QStringLiteral("miniPlayer/stayOnTop");
const QString kKWinService = QStringLiteral("org.kde.KWin");
const QString kKWinScripting = QStringLiteral("org.kde.kwin.Scripting");

QString jsString(const QString &s)
{
    const QByteArray json = QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}
}

Tray::Tray(Player *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
    if (s_instance)
        qFatal("Kanzi: Tray constructed twice");
    s_instance = this;

    QSettings settings;
    m_enabled = settings.value(kEnabled, true).toBool();
    m_closeToTray = settings.value(kCloseToTray, true).toBool();
    m_startInTray = settings.value(kStartInTray, false).toBool();
    m_miniPlayerOnTop = settings.value(kMiniOnTop, false).toBool();
    if (auto *bus = QDBusConnection::sessionBus().interface())
        m_kwin = bus->isServiceRegistered(kKWinService);

    m_menu = new QMenu;
    m_nowPlaying = m_menu->addAction(tr("Nothing playing"));
    m_nowPlaying->setEnabled(false);
    m_menu->addSeparator();
    m_playPause = m_menu->addAction(QIcon::fromTheme(QStringLiteral("media-playback-start")), tr("Play"),
                                    player, &Player::togglePlayPause);
    m_next = m_menu->addAction(QIcon::fromTheme(QStringLiteral("media-skip-forward")), tr("Next"),
                               player, &Player::next);
    m_previous = m_menu->addAction(QIcon::fromTheme(QStringLiteral("media-skip-backward")), tr("Previous"),
                                   player, &Player::previous);
    m_menu->addSeparator();
    m_menu->addAction(tr("Show Kanzi"), this, &Tray::showWindowRequested);
    m_menu->addAction(tr("Mini Player"), this, &Tray::miniPlayerRequested);
    m_menu->addSeparator();
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), tr("Quit"), qApp, &QCoreApplication::quit);

    m_icon = new QSystemTrayIcon(this);
    m_icon->setIcon(QIcon::fromTheme(QStringLiteral("dev.kanzi.Kanzi"),
                                     QIcon(QStringLiteral(":/qt/qml/Kanzi/resources/kanzi.svg"))));
    m_icon->setContextMenu(m_menu);
    connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            emit toggleWindowRequested();
        else if (reason == QSystemTrayIcon::MiddleClick)
            m_player->togglePlayPause();
    });

    connect(player, &Player::nowPlayingChanged, this, &Tray::updateNowPlaying);
    connect(player, &Player::stateChanged, this, &Tray::updateNowPlaying);
    connect(player, &Player::queueChanged, this, &Tray::updateNowPlaying);
    updateNowPlaying();
    updateIcon();
}

Tray::~Tray()
{
    delete m_menu;
    s_instance = nullptr;
}

Tray *Tray::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool Tray::available() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void Tray::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    QSettings().setValue(kEnabled, on);
    updateIcon();
    emit settingsChanged();
}

void Tray::setCloseToTray(bool on)
{
    if (on == m_closeToTray)
        return;
    m_closeToTray = on;
    QSettings().setValue(kCloseToTray, on);
    emit settingsChanged();
}

void Tray::setStartInTray(bool on)
{
    if (on == m_startInTray)
        return;
    m_startInTray = on;
    QSettings().setValue(kStartInTray, on);
    emit settingsChanged();
}

void Tray::setMiniPlayerOnTop(bool on)
{
    if (on == m_miniPlayerOnTop)
        return;
    m_miniPlayerOnTop = on;
    QSettings().setValue(kMiniOnTop, on);
    emit settingsChanged();
}

void Tray::setKeepAbove(const QString &caption, bool on)
{
    if (!m_kwin)
        return;

    // The script lives only as long as KWin needs to read it.
    auto *file = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/kanzi-keepabove-XXXXXX.js"), this);
    if (!file->open()) {
        delete file;
        return;
    }
    const QString script = QStringLiteral(R"js(
(function () {
    const windows = workspace.windowList ? workspace.windowList() : workspace.clientList();
    for (const w of windows) {
        if (w.pid === %1 && w.caption === %2)
            w.keepAbove = %3;
    }
})();
)js").arg(QCoreApplication::applicationPid()).arg(jsString(caption), on ? QStringLiteral("true") : QStringLiteral("false"));
    file->write(script.toUtf8());
    file->flush();

    const QString plugin = QStringLiteral("kanzi-keepabove-%1-%2").arg(QCoreApplication::applicationPid()).arg(++m_scriptSerial);
    QDBusMessage load = QDBusMessage::createMethodCall(kKWinService, QStringLiteral("/Scripting"), kKWinScripting,
                                                       QStringLiteral("loadScript"));
    load << file->fileName() << plugin;

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(load), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, file, plugin] {
        watcher->deleteLater();
        const QDBusPendingReply<int> reply = *watcher;
        const auto cleanup = [file, plugin] {
            QDBusMessage unload = QDBusMessage::createMethodCall(kKWinService, QStringLiteral("/Scripting"),
                                                                 kKWinScripting, QStringLiteral("unloadScript"));
            unload << plugin;
            QDBusConnection::sessionBus().asyncCall(unload);
            file->deleteLater();
        };
        if (reply.isError() || reply.value() < 0) {
            qWarning("Kanzi: KWin refused the keep-above script: %s", qPrintable(reply.error().message()));
            return cleanup();
        }
        // KWin 6 exposes each script at /Scripting/Script<id>; KWin 5 at /<id>.
        // Scripting.start() runs any loaded-but-idle script and covers both.
        QDBusMessage run = QDBusMessage::createMethodCall(kKWinService, QStringLiteral("/Scripting/Script%1").arg(reply.value()),
                                                          QStringLiteral("org.kde.kwin.Script"), QStringLiteral("run"));
        auto *runWatcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(run), this);
        connect(runWatcher, &QDBusPendingCallWatcher::finished, this, [runWatcher, cleanup] {
            runWatcher->deleteLater();
            if (runWatcher->isError()) {
                QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
                    kKWinService, QStringLiteral("/Scripting"), kKWinScripting, QStringLiteral("start")));
            }
            QTimer::singleShot(2000, cleanup);
        });
    });
}

void Tray::updateIcon()
{
    m_icon->setVisible(active());
}

void Tray::updateNowPlaying()
{
    const QVariantMap np = m_player->nowPlaying();
    const bool has = m_player->hasTrack();
    const QString title = np.value(QStringLiteral("title")).toString();
    const QString artist = np.value(QStringLiteral("artist")).toString();

    m_nowPlaying->setText(has ? QStringLiteral("%1 — %2").arg(title, artist) : tr("Nothing playing"));
    m_playPause->setText(m_player->playing() ? tr("Pause") : tr("Play"));
    m_playPause->setIcon(QIcon::fromTheme(m_player->playing() ? QStringLiteral("media-playback-pause")
                                                              : QStringLiteral("media-playback-start")));
    m_playPause->setEnabled(has);
    m_next->setEnabled(has && m_player->canGoNext());
    m_previous->setEnabled(has);
    m_icon->setToolTip(has ? QStringLiteral("Kanzi\n%1 — %2").arg(title, artist) : QStringLiteral("Kanzi"));
}
