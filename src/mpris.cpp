#include "mpris.h"

#include "applemusic.h"
#include "player.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>

namespace {
const QString kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kPlayerIface = QStringLiteral("org.mpris.MediaPlayer2.Player");
}

Mpris::Mpris(Player *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
    new MprisRoot(this);
    auto *adaptor = new MprisPlayer(this);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    bus.registerObject(kPath, this);
    QString service = QStringLiteral("org.mpris.MediaPlayer2.kanzi");
    if (!bus.registerService(service))
        bus.registerService(service + QStringLiteral(".instance%1").arg(QCoreApplication::applicationPid()));

    connect(player, &Player::stateChanged, this, [this, adaptor] {
        notifyChanged(kPlayerIface, {{QStringLiteral("PlaybackStatus"), adaptor->playbackStatus()},
                                     {QStringLiteral("CanPlay"), adaptor->canPlay()},
                                     {QStringLiteral("CanPause"), adaptor->canPlay()},
                                     {QStringLiteral("CanSeek"), adaptor->canPlay()}});
    });
    connect(player, &Player::nowPlayingChanged, this, [this, adaptor] {
        notifyChanged(kPlayerIface, {{QStringLiteral("Metadata"), adaptor->metadata()}});
    });
    connect(player, &Player::queueChanged, this, [this, adaptor] {
        notifyChanged(kPlayerIface, {{QStringLiteral("CanGoNext"), adaptor->canGoNext()},
                                     {QStringLiteral("CanGoPrevious"), adaptor->canGoPrevious()}});
    });
    connect(player, &Player::modesChanged, this, [this, adaptor] {
        notifyChanged(kPlayerIface, {{QStringLiteral("Shuffle"), adaptor->shuffle()},
                                     {QStringLiteral("LoopStatus"), adaptor->loopStatus()}});
    });
    connect(player, &Player::volumeChanged, this, [this, adaptor] {
        notifyChanged(kPlayerIface, {{QStringLiteral("Volume"), adaptor->volume()}});
    });
    connect(player, &Player::seeked, adaptor, [adaptor](double pos) {
        emit adaptor->Seeked(qlonglong(pos * 1e6));
    });
}

void Mpris::notifyChanged(const QString &interface, const QVariantMap &changed)
{
    QDBusMessage msg = QDBusMessage::createSignal(kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                  QStringLiteral("PropertiesChanged"));
    msg << interface << changed << QStringList();
    QDBusConnection::sessionBus().send(msg);
}

// ---- org.mpris.MediaPlayer2 ----

MprisRoot::MprisRoot(Mpris *parent)
    : QDBusAbstractAdaptor(parent)
    , m_mpris(parent)
{
}

void MprisRoot::Raise()
{
    m_mpris->player()->requestRaise();
}

void MprisRoot::Quit()
{
    QCoreApplication::quit();
}

// ---- org.mpris.MediaPlayer2.Player ----

MprisPlayer::MprisPlayer(Mpris *parent)
    : QDBusAbstractAdaptor(parent)
    , m_mpris(parent)
    , m_player(parent->player())
{
}

QString MprisPlayer::playbackStatus() const
{
    if (!m_player->hasTrack())
        return QStringLiteral("Stopped");
    switch (m_player->state()) {
    case Player::Playing:
    case Player::Loading:
    case Player::Waiting:
    case Player::Stalled:
    case Player::Seeking:
        return QStringLiteral("Playing");
    case Player::Paused:
        return QStringLiteral("Paused");
    default:
        return QStringLiteral("Stopped");
    }
}

QString MprisPlayer::loopStatus() const
{
    switch (m_player->repeatMode()) {
    case Player::RepeatOne: return QStringLiteral("Track");
    case Player::RepeatAll: return QStringLiteral("Playlist");
    default: return QStringLiteral("None");
    }
}

void MprisPlayer::setLoopStatus(const QString &s)
{
    if (s == u"Track")
        m_player->setRepeatMode(Player::RepeatOne);
    else if (s == u"Playlist")
        m_player->setRepeatMode(Player::RepeatAll);
    else
        m_player->setRepeatMode(Player::RepeatNone);
}

bool MprisPlayer::shuffle() const { return m_player->shuffle(); }
void MprisPlayer::setShuffle(bool on) { m_player->setShuffle(on); }
double MprisPlayer::volume() const { return m_player->volume(); }
void MprisPlayer::setVolume(double v) { m_player->setVolume(v); }
qlonglong MprisPlayer::position() const { return qlonglong(m_player->position() * 1e6); }
bool MprisPlayer::canGoNext() const { return m_player->canGoNext(); }
bool MprisPlayer::canGoPrevious() const { return m_player->canGoPrevious(); }
bool MprisPlayer::canPlay() const { return m_player->hasTrack(); }

QDBusObjectPath MprisPlayer::trackId() const
{
    QString id = m_player->nowPlaying().value(QStringLiteral("id")).toString();
    // Object paths only allow [A-Za-z0-9_].
    for (QChar &c : id) {
        if (!c.isLetterOrNumber() || c.unicode() > 127)
            c = u'_';
    }
    if (id.isEmpty())
        return QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack"));
    return QDBusObjectPath(QStringLiteral("/dev/kanzi/Kanzi/track/t") + id);
}

QVariantMap MprisPlayer::metadata() const
{
    const QVariantMap np = m_player->nowPlaying();
    if (np.isEmpty())
        return {{QStringLiteral("mpris:trackid"), QVariant::fromValue(trackId())}};

    QVariantMap md;
    md.insert(QStringLiteral("mpris:trackid"), QVariant::fromValue(trackId()));
    md.insert(QStringLiteral("mpris:length"), qlonglong(np.value(QStringLiteral("duration")).toDouble() * 1e6));
    md.insert(QStringLiteral("xesam:title"), np.value(QStringLiteral("title")).toString());
    md.insert(QStringLiteral("xesam:album"), np.value(QStringLiteral("album")).toString());
    md.insert(QStringLiteral("xesam:artist"), QStringList{np.value(QStringLiteral("artist")).toString()});
    const QString art = AppleMusic::instance()->artworkUrl(np.value(QStringLiteral("artwork")), 600);
    if (!art.isEmpty())
        md.insert(QStringLiteral("mpris:artUrl"), art);
    return md;
}

void MprisPlayer::Next() { m_player->next(); }
void MprisPlayer::Previous() { m_player->previous(); }
void MprisPlayer::Pause() { m_player->pause(); }
void MprisPlayer::PlayPause() { m_player->togglePlayPause(); }
void MprisPlayer::Stop() { m_player->pause(); }
void MprisPlayer::Play() { m_player->play(); }

void MprisPlayer::Seek(qlonglong offset)
{
    m_player->seek(m_player->position() + double(offset) / 1e6);
}

void MprisPlayer::SetPosition(const QDBusObjectPath &id, qlonglong position)
{
    if (id == trackId())
        m_player->seek(double(position) / 1e6);
}
