#include "notifications.h"

#include "mpris.h"
#include "player.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QSettings>

namespace {

const QString kService = QStringLiteral("org.freedesktop.Notifications");
const QString kPath = QStringLiteral("/org/freedesktop/Notifications");
const QString kInterface = QStringLiteral("org.freedesktop.Notifications");
const QString kEnabled = QStringLiteral("notifications/trackChanges");
const QString kWhenFocused = QStringLiteral("notifications/whenFocused");
// How long to hold a notification back for its cover to finish downloading.
const int kArtWaitMs = 1500;

} // namespace

Notifications::Notifications(Player *player, Mpris *mpris, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_mpris(mpris)
{
    if (s_instance)
        qFatal("Kanzi: Notifications constructed twice");
    s_instance = this;

    QSettings settings;
    m_enabled = settings.value(kEnabled, false).toBool();
    m_whenFocused = settings.value(kWhenFocused, false).toBool();

    m_artWait.setSingleShot(true);
    m_artWait.setInterval(kArtWaitMs);
    connect(&m_artWait, &QTimer::timeout, this, &Notifications::show);
    connect(mpris, &Mpris::artChanged, this, [this] {
        if (!m_pendingId.isEmpty())
            show();
    });

    // Mpris is constructed first, so its art lookup for a new track has already run by now.
    connect(player, &Player::nowPlayingChanged, this, &Notifications::onTrackMaybeChanged);
    connect(player, &Player::stateChanged, this, &Notifications::onTrackMaybeChanged);

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(kService, kPath, kInterface, QStringLiteral("ActionInvoked"), this,
                SLOT(onActionInvoked(uint, QString)));
    bus.connect(kService, kPath, kInterface, QStringLiteral("NotificationClosed"), this,
                SLOT(onClosed(uint, uint)));
    queryServer();
}

Notifications::~Notifications()
{
    s_instance = nullptr;
}

Notifications *Notifications::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void Notifications::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    QSettings().setValue(kEnabled, on);
    emit settingsChanged();
    if (on)
        queryServer(); // a notification daemon may have started since launch
}

void Notifications::setWhenFocused(bool on)
{
    if (on == m_whenFocused)
        return;
    m_whenFocused = on;
    QSettings().setValue(kWhenFocused, on);
    emit settingsChanged();
}

void Notifications::queryServer()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("GetCapabilities"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QStringList> reply = *w;
        const bool ok = !reply.isError();
        m_caps = ok ? reply.value() : QStringList();
        if (ok != m_available) {
            m_available = ok;
            emit availableChanged();
        }
    });
}

void Notifications::onTrackMaybeChanged()
{
    const QString id = m_player->nowPlaying().value(QStringLiteral("id")).toString();
    if (id.isEmpty() || id == m_notifiedId)
        return;
    // Announce a track once it actually starts, not when a session is restored paused.
    if (!m_player->playing() && !m_player->buffering())
        return;
    m_notifiedId = id;
    m_pendingId.clear();
    m_artWait.stop();

    if (!m_enabled || !m_available)
        return;
    if (!m_whenFocused && QGuiApplication::applicationState() == Qt::ApplicationActive)
        return;

    m_pendingId = id;
    const bool hasArtwork = !m_player->nowPlaying().value(QStringLiteral("artwork")).toString().isEmpty();
    if (m_mpris->artUrl().isEmpty() && hasArtwork)
        m_artWait.start();
    else
        show();
}

void Notifications::show()
{
    m_artWait.stop();
    const QVariantMap np = m_player->nowPlaying();
    const QString id = np.value(QStringLiteral("id")).toString();
    if (m_pendingId.isEmpty() || id != m_pendingId)
        return;
    m_pendingId.clear();

    const bool markup = m_caps.contains(QLatin1String("body-markup"));
    const auto text = [markup](const QString &s) { return markup ? s.toHtmlEscaped() : s; };

    const QString title = np.value(QStringLiteral("title")).toString();
    const QString artist = np.value(QStringLiteral("artist")).toString();
    const QString album = np.value(QStringLiteral("album")).toString();
    QStringList body;
    if (!artist.isEmpty())
        body << text(artist);
    if (!album.isEmpty())
        body << text(album);

    QVariantMap hints{
        {QStringLiteral("desktop-entry"), QStringLiteral("dev.kanzi.Kanzi")},
        {QStringLiteral("category"), QStringLiteral("x-gnome.music")},
        {QStringLiteral("urgency"), QVariant::fromValue(uchar(0))}, // low
        {QStringLiteral("transient"), true}, // keep track changes out of the notification history
    };
    const QUrl art = m_mpris->artUrl();
    if (!art.isEmpty())
        hints.insert(QStringLiteral("image-path"), art.toString());

    QStringList actions;
    if (m_caps.contains(QLatin1String("actions"))) {
        actions << QStringLiteral("default") << tr("Show Kanzi");
        if (m_player->canGoNext())
            actions << QStringLiteral("next") << tr("Skip");
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("Notify"));
    msg << QStringLiteral("Kanzi") << m_notificationId << QStringLiteral("dev.kanzi.Kanzi")
        << (title.isEmpty() ? tr("Now playing") : title) << body.join(QStringLiteral(" — ")) << actions << hints
        << qint32(-1);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<uint> reply = *w;
        if (reply.isError())
            qWarning().noquote() << "Kanzi: notification failed:" << reply.error().message();
        else
            m_notificationId = reply.value(); // the next track replaces this one instead of stacking
    });
}

void Notifications::onActionInvoked(uint id, const QString &action)
{
    if (id == 0 || id != m_notificationId)
        return;
    if (action == u"default")
        m_player->requestRaise();
    else if (action == u"next")
        m_player->next();
}

void Notifications::onClosed(uint id, uint)
{
    if (id == m_notificationId)
        m_notificationId = 0;
}
