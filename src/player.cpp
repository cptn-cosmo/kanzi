#include "player.h"

#include "applemusic.h"
#include "plex.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QtMath>

namespace {
QString readResource(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("Kanzi: missing resource %s", qPrintable(path));
        return {};
    }
    return QString::fromUtf8(f.readAll());
}
} // namespace

Player::Player(AppleMusic *api, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
    if (s_instance)
        qFatal("Kanzi: %s constructed twice", "Player");
    s_instance = this;
    QSettings settings;
    m_volume = settings.value(QStringLiteral("player/volume"), 1.0).toDouble();
    m_crossfade = qBound(0, settings.value(QStringLiteral("player/crossfade"), 0).toInt(), 12);
    m_automix = settings.value(QStringLiteral("player/automix"), false).toBool();
    m_source = settings.value(QStringLiteral("player/source")).toString() == QLatin1String("plex") ? PlexSource : AppleMusicSource;

    // The page bakes the developer token in, so a new token means a reload.
    connect(api, &AppleMusic::developerTokenChanged, this, [this] {
        if (m_source != AppleMusicSource)
            return;
        m_ready = false;
        emit readyChanged();
        emit reloadRequested();
    });
    connect(api, &AppleMusic::signOutRequested, this, [this] {
        if (m_source == AppleMusicSource)
            send(QStringLiteral("unauthorize"));
    });
}

void Player::connectPlex(Plex *plex)
{
    // The Plex page is loaded under the server's origin, so a new server means a reload.
    connect(plex, &Plex::serverUrlChanged, this, [this] {
        if (m_source != PlexSource)
            return;
        resetState();
        emit reloadRequested();
    });
}

void Player::setSource(Source source)
{
    if (source == m_source)
        return;
    m_source = source;
    QSettings().setValue(QStringLiteral("player/source"),
                         source == PlexSource ? QStringLiteral("plex") : QStringLiteral("apple"));
    resetState();
    emit sourceChanged();
    emit reloadRequested();
}

// Forget everything the old page reported; the new page reports afresh once it's up.
void Player::resetState()
{
    m_ready = false;
    m_state = None;
    m_position = m_duration = 0;
    m_nowPlaying.clear();
    m_queue.clear();
    m_queuePosition = -1;
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorStringChanged();
    }
    emit readyChanged();
    emit stateChanged();
    emit nowPlayingChanged();
    emit positionChanged();
    emit queueChanged();
}

Player::~Player()
{
    s_instance = nullptr;
}

Player *Player::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool Player::canLoad() const
{
    if (m_source == PlexSource)
        return Plex::instance() && !Plex::instance()->serverUrl().isEmpty();
    return !m_api->developerToken().isEmpty();
}

QString Player::playerBaseUrl() const
{
    if (m_source == PlexSource && Plex::instance())
        return Plex::instance()->serverUrl() + u'/';
    return QStringLiteral("https://music.apple.com/");
}

QString Player::playerHtml() const
{
    QString html = readResource(m_source == PlexSource ? QStringLiteral(":/qt/qml/Kanzi/resources/plex.html")
                                                       : QStringLiteral(":/qt/qml/Kanzi/resources/player.html"));
    const QString channel = readResource(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
    const auto json = [](const QString &s) {
        return QString::fromUtf8(QJsonDocument(QJsonArray{s}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
    };
    html.replace(QStringLiteral("/*QWEBCHANNEL_JS*/"), channel);
    html.replace(QStringLiteral("\"%DEVELOPER_TOKEN%\""), json(m_api->developerToken()));
    html.replace(QStringLiteral("\"%USER_TOKEN%\""), json(m_api->userToken()));
    html.replace(QStringLiteral("\"%VERSION%\""), json(QStringLiteral(KANZI_VERSION)));
    html.replace(QStringLiteral("%VOLUME%"), QString::number(m_volume));
    html.replace(QStringLiteral("%TRANSITIONS%"),
                 QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(transitionArgs())).toJson(QJsonDocument::Compact)));
    return html;
}

void Player::send(const QString &name, const QVariantMap &args)
{
    if (!m_ready && name != QLatin1String("unauthorize")) {
        qWarning("Kanzi: player not ready, dropping %s", qPrintable(name));
        return;
    }
    emit command(name, args);
}

void Player::playCollection(const QString &type, const QVariant &id, int startIndex, bool shuffle)
{
    send(QStringLiteral("setQueue"), {{QStringLiteral("type"), type},
                                      {QStringLiteral("id"), id},
                                      {QStringLiteral("startIndex"), startIndex},
                                      {QStringLiteral("shuffle"), shuffle}});
}

void Player::playNext(const QString &type, const QString &id)
{
    send(QStringLiteral("playNext"), {{QStringLiteral("type"), type}, {QStringLiteral("id"), id}});
}

void Player::playLater(const QString &type, const QString &id)
{
    send(QStringLiteral("playLater"), {{QStringLiteral("type"), type}, {QStringLiteral("id"), id}});
}

void Player::playTracks(const QVariantList &items, int startIndex, bool shuffle)
{
    send(QStringLiteral("setQueue"), {{QStringLiteral("items"), items},
                                      {QStringLiteral("startIndex"), startIndex},
                                      {QStringLiteral("shuffle"), shuffle}});
}

void Player::queueTracks(const QVariantList &items, bool next)
{
    send(next ? QStringLiteral("playNext") : QStringLiteral("playLater"), {{QStringLiteral("items"), items}});
}

void Player::jumpTo(int queueIndex)
{
    send(QStringLiteral("jumpTo"), {{QStringLiteral("index"), queueIndex}});
}

void Player::play() { send(QStringLiteral("play")); }
void Player::pause() { send(QStringLiteral("pause")); }
void Player::togglePlayPause() { playing() ? pause() : play(); }
void Player::next() { send(QStringLiteral("next")); }

void Player::previous()
{
    // Behave like every other player: restart the track unless we're near its start.
    if (m_position > 3.0 || m_queuePosition <= 0)
        seek(0);
    else
        send(QStringLiteral("previous"));
}

void Player::seek(double seconds)
{
    seconds = qBound(0.0, seconds, m_duration > 0 ? m_duration : seconds);
    m_position = seconds;
    emit positionChanged();
    emit seeked(seconds);
    send(QStringLiteral("seek"), {{QStringLiteral("time"), seconds}});
}

void Player::setVolume(double v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(v, m_volume))
        return;
    m_volume = v;
    QSettings().setValue(QStringLiteral("player/volume"), v);
    emit volumeChanged();
    send(QStringLiteral("volume"), {{QStringLiteral("value"), v}});
}

QVariantMap Player::transitionArgs() const
{
    return {{QStringLiteral("crossfade"), m_crossfade}, {QStringLiteral("automix"), m_automix}};
}

void Player::sendTransitions()
{
    emit transitionsChanged();
    if (m_ready)
        emit command(QStringLiteral("transitions"), transitionArgs());
}

void Player::setCrossfade(int seconds)
{
    seconds = qBound(0, seconds, 12);
    if (seconds == m_crossfade)
        return;
    m_crossfade = seconds;
    QSettings().setValue(QStringLiteral("player/crossfade"), seconds);
    sendTransitions();
}

void Player::setAutomix(bool on)
{
    if (on == m_automix)
        return;
    m_automix = on;
    QSettings().setValue(QStringLiteral("player/automix"), on);
    sendTransitions();
}

void Player::setShuffle(bool on)
{
    send(QStringLiteral("shuffle"), {{QStringLiteral("value"), on}});
}

void Player::setRepeatMode(RepeatMode mode)
{
    send(QStringLiteral("repeat"), {{QStringLiteral("value"), int(mode)}});
}

void Player::cycleRepeat()
{
    switch (m_repeat) {
    case RepeatNone: setRepeatMode(RepeatAll); break;
    case RepeatAll: setRepeatMode(RepeatOne); break;
    case RepeatOne: setRepeatMode(RepeatNone); break;
    }
}

void Player::authorize()
{
    if (m_source == AppleMusicSource)
        send(QStringLiteral("authorize"));
}

// ---- page → native ----

void Player::jsReady()
{
    m_ready = true;
    if (!m_error.isEmpty()) {
        m_error.clear();
        emit errorStringChanged();
    }
    emit readyChanged();
}

void Player::jsError(const QString &message)
{
    qWarning("Kanzi player: %s", qPrintable(message));
    m_error = message;
    emit errorStringChanged();
}

void Player::jsLog(const QString &message)
{
    qDebug("Kanzi player: %s", qPrintable(message));
}

void Player::jsState(int state)
{
    if (m_state == State(state))
        return;
    m_state = State(state);
    emit stateChanged();
}

void Player::jsTime(double position, double duration)
{
    m_position = position;
    if (duration > 0)
        m_duration = duration;
    emit positionChanged();
}

void Player::jsVolume(double volume)
{
    if (qFuzzyCompare(volume, m_volume))
        return;
    m_volume = volume;
    emit volumeChanged();
}

void Player::jsModes(bool shuffle, int repeat)
{
    if (m_shuffle == shuffle && m_repeat == RepeatMode(repeat))
        return;
    m_shuffle = shuffle;
    m_repeat = RepeatMode(repeat);
    emit modesChanged();
    emit queueChanged(); // canGoNext depends on repeat
}

void Player::jsNowPlaying(const QVariantMap &item)
{
    m_nowPlaying = item;
    m_duration = item.value(QStringLiteral("duration")).toDouble();
    m_position = 0;
    emit nowPlayingChanged();
    emit positionChanged();
}

void Player::jsQueue(const QVariantList &items, int position)
{
    m_queue = items;
    m_queuePosition = position;
    emit queueChanged();
}

void Player::jsUserToken(const QString &token)
{
    if (m_source == AppleMusicSource)
        m_api->setUserToken(token);
}
