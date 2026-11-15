#include "discordpresence.h"

#include "applemusic.h"
#include "player.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSettings>
#include <QUuid>
#include <QtEndian>

namespace {

const QString kEnabled = QStringLiteral("discord/enabled");
const QString kClientId = QStringLiteral("discord/clientId");
const QString kShowPaused = QStringLiteral("discord/showWhenPaused");
// Kanzi's own Discord application; can be overridden in Settings or KANZI_DISCORD_CLIENT_ID.
const QString kDefaultClientId = QStringLiteral("1571594211579404319");

// Discord rejects strings shorter than 2 or longer than 128 characters.
QString clamp(QString s)
{
    s = s.simplified();
    if (s.size() > 128)
        s = s.left(127) + u'…';
    while (s.size() < 2)
        s += u' ';
    return s;
}

// Every place a Discord client (native, Flatpak, Snap, Vesktop/arRPC) puts its socket.
QStringList socketCandidates()
{
    QStringList bases;
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!runtime.isEmpty()) {
        bases << runtime
              << runtime + QStringLiteral("/app/com.discordapp.Discord")
              << runtime + QStringLiteral("/app/com.discordapp.DiscordCanary")
              << runtime + QStringLiteral("/app/dev.vencord.Vesktop")
              << runtime + QStringLiteral("/snap.discord");
    }
    bases << qEnvironmentVariable("TMPDIR") << QStringLiteral("/tmp");

    QStringList out;
    for (const QString &base : std::as_const(bases)) {
        if (base.isEmpty())
            continue;
        for (int i = 0; i < 10; ++i)
            out << QStringLiteral("%1/discord-ipc-%2").arg(base).arg(i);
    }
    return out;
}

} // namespace

DiscordPresence::DiscordPresence(Player *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
    if (s_instance)
        qFatal("Kanzi: DiscordPresence constructed twice");
    s_instance = this;

    QSettings settings;
    m_enabled = settings.value(kEnabled, true).toBool();
    m_showWhenPaused = settings.value(kShowPaused, false).toBool();
    m_clientId = qEnvironmentVariable("KANZI_DISCORD_CLIENT_ID", settings.value(kClientId).toString());
    if (m_clientId.isEmpty())
        m_clientId = kDefaultClientId;

    m_retry.setInterval(15000);
    connect(&m_retry, &QTimer::timeout, this, &DiscordPresence::tryConnect);
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(600);
    connect(&m_debounce, &QTimer::timeout, this, &DiscordPresence::pushActivity);

    connect(player, &Player::nowPlayingChanged, this, &DiscordPresence::scheduleUpdate);
    connect(player, &Player::stateChanged, this, &DiscordPresence::scheduleUpdate);
    connect(player, &Player::seeked, this, &DiscordPresence::scheduleUpdate);
    // Duration often arrives a moment after the track itself.
    connect(player, &Player::positionChanged, this, [this] {
        static double lastDuration = 0;
        if (!qFuzzyCompare(lastDuration + 1, m_player->duration() + 1)) {
            lastDuration = m_player->duration();
            scheduleUpdate();
        }
    });

    reconnect();
}

DiscordPresence::~DiscordPresence()
{
    s_instance = nullptr;
}

DiscordPresence *DiscordPresence::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void DiscordPresence::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    QSettings().setValue(kEnabled, on);
    emit settingsChanged();
    reconnect();
}

void DiscordPresence::setClientId(const QString &id)
{
    const QString trimmed = id.trimmed().isEmpty() ? kDefaultClientId : id.trimmed();
    if (trimmed == m_clientId)
        return;
    m_clientId = trimmed;
    QSettings().setValue(kClientId, trimmed);
    emit settingsChanged();
    reconnect();
}

void DiscordPresence::setShowWhenPaused(bool on)
{
    if (on == m_showWhenPaused)
        return;
    m_showWhenPaused = on;
    QSettings().setValue(kShowPaused, on);
    emit settingsChanged();
    scheduleUpdate();
}

void DiscordPresence::setStatus(const QString &status, bool ready)
{
    if (status == m_status && ready == m_ready)
        return;
    m_status = status;
    m_ready = ready;
    emit statusChanged();
}

// ---- connection ----

void DiscordPresence::reconnect()
{
    disconnectSocket(QString());
    if (!m_enabled)
        return setStatus(tr("Off"), false);
    if (m_clientId.isEmpty())
        return setStatus(tr("Needs a Discord application ID"), false);
    m_retry.start();
    tryConnect();
}

void DiscordPresence::disconnectSocket(const QString &status)
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_buffer.clear();
    m_candidates.clear();
    if (!status.isNull())
        setStatus(status, false);
    else
        m_ready = false;
}

void DiscordPresence::tryConnect()
{
    if (m_socket || !m_enabled || m_clientId.isEmpty())
        return;
    if (m_candidates.isEmpty()) {
        for (const QString &path : socketCandidates()) {
            if (QFileInfo::exists(path))
                m_candidates << path;
        }
        if (m_candidates.isEmpty())
            return setStatus(tr("Discord isn't running"), false);
    }

    const QString path = m_candidates.takeFirst();
    m_socket = new QLocalSocket(this);
    connect(m_socket, &QLocalSocket::connected, this, [this] {
        send(Handshake, {{QStringLiteral("v"), 1}, {QStringLiteral("client_id"), m_clientId}});
    });
    connect(m_socket, &QLocalSocket::readyRead, this, &DiscordPresence::onReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, [this] {
        // Try the next socket path right away; otherwise wait for the retry timer.
        const bool more = !m_candidates.isEmpty();
        disconnectSocket(more ? QString() : tr("Discord isn't running"));
        if (more)
            QMetaObject::invokeMethod(this, &DiscordPresence::tryConnect, Qt::QueuedConnection);
    });
    connect(m_socket, &QLocalSocket::disconnected, this, [this] {
        disconnectSocket(tr("Disconnected from Discord"));
    });
    m_socket->connectToServer(path);
}

void DiscordPresence::send(Opcode op, const QJsonObject &payload)
{
    if (!m_socket || m_socket->state() != QLocalSocket::ConnectedState)
        return;
    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray frame(8, Qt::Uninitialized);
    qToLittleEndian<quint32>(op, frame.data());
    qToLittleEndian<quint32>(quint32(json.size()), frame.data() + 4);
    m_socket->write(frame + json);
}

void DiscordPresence::onReadyRead()
{
    m_buffer += m_socket->readAll();
    while (m_buffer.size() >= 8) {
        const quint32 op = qFromLittleEndian<quint32>(m_buffer.constData());
        const quint32 len = qFromLittleEndian<quint32>(m_buffer.constData() + 4);
        if (quint32(m_buffer.size()) < 8 + len)
            return;
        const QJsonObject msg = QJsonDocument::fromJson(m_buffer.mid(8, len)).object();
        m_buffer.remove(0, 8 + len);
        handleFrame(op, msg);
        if (!m_socket)
            return; // handleFrame may have closed us
    }
}

void DiscordPresence::handleFrame(quint32 op, const QJsonObject &msg)
{
    switch (op) {
    case Ping:
        send(Pong, msg);
        break;
    case Close: {
        // Typically an invalid application ID.
        const QString reason = msg.value(u"message").toString();
        disconnectSocket(reason.isEmpty() ? tr("Discord closed the connection") : tr("Discord: %1").arg(reason));
        break;
    }
    case Frame: {
        const QString evt = msg.value(u"evt").toString();
        if (msg.value(u"cmd").toString() == u"DISPATCH" && evt == u"READY") {
            const QString user = msg.value(u"data").toObject().value(u"user").toObject().value(u"username").toString();
            setStatus(user.isEmpty() ? tr("Connected") : tr("Connected as %1").arg(user), true);
            pushActivity();
        } else if (evt == u"ERROR") {
            qWarning().noquote() << "Kanzi: Discord rejected the activity:"
                                 << msg.value(u"data").toObject().value(u"message").toString();
        }
        break;
    }
    default:
        break;
    }
}

// ---- activity ----

void DiscordPresence::scheduleUpdate()
{
    if (m_ready)
        m_debounce.start();
}

void DiscordPresence::pushActivity()
{
    if (!m_ready)
        return;

    QJsonObject args{{QStringLiteral("pid"), qint64(QCoreApplication::applicationPid())}};
    const bool playing = m_player->playing() || m_player->buffering();
    const bool show = m_player->hasTrack() && (playing || (m_showWhenPaused && m_player->state() == Player::Paused));

    if (show) {
        const QVariantMap np = m_player->nowPlaying();
        const QString title = np.value(QStringLiteral("title")).toString();
        const QString artist = np.value(QStringLiteral("artist")).toString();
        const QString album = np.value(QStringLiteral("album")).toString();

        QJsonObject activity{
            {QStringLiteral("type"), 2}, // "Listening to"
            {QStringLiteral("details"), clamp(title)},
            {QStringLiteral("state"), clamp(playing ? artist : tr("Paused · %1").arg(artist))},
        };

        QJsonObject assets;
        const QString art = AppleMusic::instance()->artworkUrl(np.value(QStringLiteral("artwork")), 512);
        if (art.startsWith(u"https://"))
            assets.insert(QStringLiteral("large_image"), art);
        if (!album.isEmpty())
            assets.insert(QStringLiteral("large_text"), clamp(album));
        if (!assets.isEmpty())
            activity.insert(QStringLiteral("assets"), assets);

        if (playing && m_player->duration() > 0) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            const qint64 start = now - qint64(m_player->position() * 1000);
            activity.insert(QStringLiteral("timestamps"),
                            QJsonObject{{QStringLiteral("start"), start},
                                        {QStringLiteral("end"), start + qint64(m_player->duration() * 1000)}});
        }

        const QString catalogId = np.value(QStringLiteral("catalogId")).toString();
        if (!catalogId.isEmpty()) {
            const QString url = QStringLiteral("https://music.apple.com/%1/song/%2")
                                    .arg(AppleMusic::instance()->storefront(), catalogId);
            activity.insert(QStringLiteral("buttons"),
                            QJsonArray{QJsonObject{{QStringLiteral("label"), tr("Open in Apple Music")},
                                                   {QStringLiteral("url"), url}}});
        }
        args.insert(QStringLiteral("activity"), activity);
    }

    send(Frame, {{QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY")},
                 {QStringLiteral("args"), args},
                 {QStringLiteral("nonce"), QUuid::createUuid().toString(QUuid::WithoutBraces)}});
}
