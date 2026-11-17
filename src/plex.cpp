#include "plex.h"

#include "player.h"

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QSysInfo>
#include <QUrlQuery>
#include <QUuid>
#include <memory>

using namespace Qt::StringLiterals;

namespace {
const QString kPlexTv = u"https://plex.tv"_s;
constexpr int kPageSize = 100;
constexpr int kProbeTimeoutMs = 4000;
constexpr int kRequestTimeoutMs = 20000;

bool debug() { return qEnvironmentVariableIsSet("KANZI_DEBUG"); }

QString str(const QJsonObject &o, QStringView key)
{
    const QJsonValue v = o.value(key);
    return v.isDouble() ? QString::number(qint64(v.toDouble())) : v.toString();
}

QJsonArray metadataOf(const QJsonObject &container)
{
    return container.value(u"Metadata").toArray();
}

QJsonObject firstOf(const QJsonArray &array)
{
    return array.isEmpty() ? QJsonObject() : array.first().toObject();
}

// Codecs QtWebEngine's Chromium decodes natively; anything else goes through the
// server's transcoder.
bool directPlayable(const QString &codec)
{
    static const QStringList ok = {u"mp3"_s, u"aac"_s, u"flac"_s, u"opus"_s, u"vorbis"_s, u"pcm"_s, u"wav"_s};
    return codec.isEmpty() || ok.contains(codec.toLower());
}

// [mm:ss.xx] timestamps; a line may carry several.
QVariantList parseLyrics(const QString &text, bool synced)
{
    struct Line { qint64 begin; QString text; };
    QList<Line> lines;
    static const QRegularExpression stamp(u"\\[(\\d+):(\\d+(?:[.:]\\d+)?)\\]"_s);
    static const QRegularExpression tag(u"^\\[[a-zA-Z]+:.*\\]$"_s);
    for (QString raw : text.split(u'\n')) {
        raw = raw.trimmed();
        if (!synced) {
            lines.append({-1, raw});
            continue;
        }
        if (tag.match(raw).hasMatch())
            continue; // [ar:…], [ti:…] and friends
        QList<qint64> times;
        qsizetype end = 0;
        for (auto it = stamp.globalMatch(raw); it.hasNext();) {
            const auto m = it.next();
            if (m.capturedStart() != end)
                break;
            end = m.capturedEnd();
            const double secs = m.captured(2).replace(u':', u'.').toDouble();
            times.append(m.captured(1).toLongLong() * 60000 + qint64(secs * 1000));
        }
        const QString body = raw.mid(end).trimmed();
        for (qint64 t : times)
            lines.append({t, body});
    }
    if (synced)
        std::stable_sort(lines.begin(), lines.end(), [](const Line &a, const Line &b) { return a.begin < b.begin; });

    QVariantList out;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const qint64 end = synced && i + 1 < lines.size() ? lines[i + 1].begin : -1;
        out.append(QVariantMap{{u"begin"_s, lines[i].begin}, {u"end"_s, end}, {u"text"_s, lines[i].text}});
    }
    return out;
}
} // namespace

Plex::Plex(Player *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
    if (s_instance)
        qFatal("Kanzi: %s constructed twice", "Plex");
    s_instance = this;

    QSettings s;
    m_clientId = s.value(u"plex/clientId"_s).toString();
    if (m_clientId.isEmpty()) {
        m_clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue(u"plex/clientId"_s, m_clientId);
    }
    m_accountToken = s.value(u"plex/accountToken"_s).toString();
    m_accountName = s.value(u"plex/accountName"_s).toString();
    m_serverId = s.value(u"plex/serverId"_s).toString();
    m_serverName = s.value(u"plex/serverName"_s).toString();
    m_serverUrl = s.value(u"plex/serverUrl"_s).toString();
    m_serverToken = s.value(u"plex/serverToken"_s).toString();
    m_sectionId = s.value(u"plex/sectionId"_s).toString();

    m_pinTimer.setInterval(2000);
    connect(&m_pinTimer, &QTimer::timeout, this, &Plex::pollPin);

    // Report playback to the server so it shows under "Now Playing" and in history.
    m_timelineTimer.setInterval(10000);
    connect(&m_timelineTimer, &QTimer::timeout, this, [this] { reportTimeline(u"playing"_s); });
    connect(player, &Player::positionChanged, this, [this] { m_lastPosition = m_player->position(); });
    connect(player, &Player::nowPlayingChanged, this, [this] {
        const QVariantMap np = m_player->nowPlaying();
        const QString id = np.value(u"source"_s) == u"plex"_s ? np.value(u"id"_s).toString() : QString();
        if (id == m_reportedTrack)
            return;
        reportTimeline(u"stopped"_s);
        m_reportedTrack = id;
        m_reportedDuration = np.value(u"duration"_s).toDouble();
        m_lastPosition = 0;
        if (!id.isEmpty())
            reportTimeline(m_player->playing() ? u"playing"_s : u"paused"_s);
    });
    connect(player, &Player::stateChanged, this, [this] {
        if (m_player->playing()) {
            m_timelineTimer.start();
            reportTimeline(u"playing"_s);
        } else {
            m_timelineTimer.stop();
            if (m_player->state() == Player::Paused)
                reportTimeline(u"paused"_s);
            else if (m_player->state() == Player::Stopped || m_player->state() == Player::Ended)
                reportTimeline(u"stopped"_s);
        }
    });

    QTimer::singleShot(0, this, &Plex::reconnect);
}

Plex::~Plex()
{
    s_instance = nullptr;
}

Plex *Plex::create(QQmlEngine *qml, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    s_instance->m_engine = qml;
    return s_instance;
}

QVariantList Plex::servers() const
{
    QVariantList out;
    for (const Resource &r : m_resources)
        out.append(QVariantMap{{u"id"_s, r.id}, {u"name"_s, r.name}, {u"owned"_s, r.owned}});
    return out;
}

void Plex::setStatus(Status s, const QString &error)
{
    if (s == m_status && error == m_error)
        return;
    m_status = s;
    m_error = error;
    if (!error.isEmpty())
        qWarning("Kanzi Plex: %s", qPrintable(error));
    emit statusChanged();
}

void Plex::persist()
{
    QSettings s;
    const auto put = [&s](const QString &key, const QString &value) {
        if (value.isEmpty())
            s.remove(key);
        else
            s.setValue(key, value);
    };
    put(u"plex/accountToken"_s, m_accountToken);
    put(u"plex/accountName"_s, m_accountName);
    put(u"plex/serverId"_s, m_serverId);
    put(u"plex/serverName"_s, m_serverName);
    put(u"plex/serverUrl"_s, m_serverUrl);
    put(u"plex/serverToken"_s, m_serverToken);
    put(u"plex/sectionId"_s, m_sectionId);
}

void Plex::setServer(const QString &id, const QString &name, const QString &url, const QString &token)
{
    QString clean = url.trimmed();
    while (clean.endsWith(u'/'))
        clean.chop(1);
    const bool urlChanged = clean != m_serverUrl;
    const bool changed = urlChanged || id != m_serverId || name != m_serverName || token != m_serverToken;
    m_serverId = id;
    m_serverName = name;
    m_serverUrl = clean;
    m_serverToken = token;
    persist();
    if (changed)
        emit serverChanged();
    if (urlChanged)
        emit serverUrlChanged();
}

void Plex::setSectionId(const QString &id)
{
    if (id == m_sectionId)
        return;
    m_sectionId = id;
    persist();
    emit sectionChanged();
}

// ---------------------------------------------------------------------------
// Sign-in and server discovery
// ---------------------------------------------------------------------------

QNetworkRequest Plex::plexTvRequest(const QString &path) const
{
    QNetworkRequest req(QUrl(kPlexTv + path));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("X-Plex-Product", "Kanzi");
    req.setRawHeader("X-Plex-Version", KANZI_VERSION);
    req.setRawHeader("X-Plex-Client-Identifier", m_clientId.toUtf8());
    req.setRawHeader("X-Plex-Platform", "Linux");
    req.setRawHeader("X-Plex-Device-Name", ("Kanzi on " + QSysInfo::machineHostName()).toUtf8());
    if (!m_accountToken.isEmpty())
        req.setRawHeader("X-Plex-Token", m_accountToken.toUtf8());
    req.setTransferTimeout(kRequestTimeoutMs);
    return req;
}

void Plex::reconnect()
{
    if (!m_serverUrl.isEmpty() && !m_serverToken.isEmpty()) {
        setStatus(Connecting);
        loadSections();
    } else if (!m_accountToken.isEmpty()) {
        setStatus(Connecting);
        fetchResources();
    } else {
        setStatus(SignedOut);
    }
}

void Plex::signIn()
{
    cancelSignIn();
    QNetworkReply *reply = m_nam.post(plexTvRequest(u"/api/v2/pins?strong=true"_s), QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject pin = QJsonDocument::fromJson(reply->readAll()).object();
        m_pinId = qint64(pin.value(u"id").toDouble());
        m_pinCode = pin.value(u"code").toString();
        if (reply->error() != QNetworkReply::NoError || !m_pinId || m_pinCode.isEmpty()) {
            m_pinId = 0;
            setStatus(Error, tr("Couldn't reach plex.tv: %1").arg(reply->errorString()));
            return;
        }
        m_pinPolls = 0;
        m_pinTimer.start();
        emit signingInChanged();

        QUrlQuery q;
        q.addQueryItem(u"clientID"_s, m_clientId);
        q.addQueryItem(u"code"_s, m_pinCode);
        q.addQueryItem(u"context[device][product]"_s, u"Kanzi"_s);
        QDesktopServices::openUrl(QUrl(u"https://app.plex.tv/auth#?"_s + q.toString(QUrl::FullyEncoded)));
    });
}

void Plex::cancelSignIn()
{
    m_pinTimer.stop();
    if (m_pinId) {
        m_pinId = 0;
        emit signingInChanged();
    }
}

void Plex::pollPin()
{
    if (!m_pinId)
        return;
    if (++m_pinPolls > 300) { // ten minutes
        cancelSignIn();
        return;
    }
    QNetworkReply *reply = m_nam.get(plexTvRequest(u"/api/v2/pins/%1"_s.arg(m_pinId)));
    const qint64 pinId = m_pinId;
    connect(reply, &QNetworkReply::finished, this, [this, reply, pinId] {
        reply->deleteLater();
        if (pinId != m_pinId)
            return;
        const QString token = QJsonDocument::fromJson(reply->readAll()).object().value(u"authToken").toString();
        if (token.isEmpty())
            return;
        cancelSignIn();
        m_accountToken = token;
        persist();
        emit accountChanged();
        setStatus(Connecting);
        fetchAccount();
        fetchResources();
    });
}

void Plex::fetchAccount()
{
    QNetworkReply *reply = m_nam.get(plexTvRequest(u"/api/v2/user"_s));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject user = QJsonDocument::fromJson(reply->readAll()).object();
        const QString name = user.value(u"username").toString(user.value(u"title").toString());
        if (name.isEmpty())
            return;
        m_accountName = name;
        persist();
        emit accountChanged();
    });
}

void Plex::fetchResources()
{
    QNetworkReply *reply = m_nam.get(plexTvRequest(u"/api/v2/resources?includeHttps=1&includeRelay=1"_s));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 401) {
            setStatus(Error, tr("Your Plex sign-in has expired. Sign in again."));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(Error, tr("Couldn't reach plex.tv: %1").arg(reply->errorString()));
            return;
        }
        m_resources.clear();
        for (const QJsonValue &v : QJsonDocument::fromJson(reply->readAll()).array()) {
            const QJsonObject o = v.toObject();
            if (!o.value(u"provides").toString().split(u',').contains(u"server"_s))
                continue;
            Resource r;
            r.id = o.value(u"clientIdentifier").toString();
            r.name = o.value(u"name").toString();
            r.owned = o.value(u"owned").toBool();
            r.accessToken = o.value(u"accessToken").toString(m_accountToken);
            for (const QJsonValue &cv : o.value(u"connections").toArray()) {
                const QJsonObject c = cv.toObject();
                const bool local = c.value(u"local").toBool();
                const bool relay = c.value(u"relay").toBool();
                const QString uri = c.value(u"uri").toString();
                if (relay) {
                    r.connections.append({4, uri});
                } else if (local) {
                    r.connections.append({0, uri});
                    // plex.direct names can be blocked by DNS rebinding protection on the LAN.
                    const QString address = c.value(u"address").toString();
                    if (!address.isEmpty() && !address.contains(u':'))
                        r.connections.append({1, u"http://%1:%2"_s.arg(address).arg(c.value(u"port").toInt(32400))});
                } else {
                    r.connections.append({2, uri});
                }
            }
            if (!r.connections.isEmpty())
                m_resources.append(r);
        }
        std::stable_sort(m_resources.begin(), m_resources.end(),
                         [](const Resource &a, const Resource &b) { return a.owned > b.owned; });
        emit serversChanged();

        if (m_resources.isEmpty()) {
            setStatus(Error, tr("No Plex Media Server is shared with this account."));
            return;
        }
        auto it = std::find_if(m_resources.cbegin(), m_resources.cend(),
                               [this](const Resource &r) { return r.id == m_serverId; });
        probe(it != m_resources.cend() ? *it : m_resources.first());
    });
}

void Plex::selectServer(const QString &id)
{
    for (const Resource &r : std::as_const(m_resources)) {
        if (r.id == id) {
            if (id != m_serverId)
                m_sectionId.clear();
            probe(r);
            return;
        }
    }
}

// Tries every connection of a server at once and keeps the most direct one that answers.
void Plex::probe(const Resource &res)
{
    setStatus(Connecting);
    const int generation = ++m_probeGeneration;
    struct State {
        int pending = 0;
        int bestPriority = INT_MAX;
        QString bestUri;
        bool decided = false;
    };
    auto state = std::make_shared<State>();
    state->pending = int(res.connections.size());

    const auto decide = [this, res, state, generation] {
        if (state->decided || generation != m_probeGeneration)
            return;
        state->decided = true;
        if (state->bestUri.isEmpty()) {
            setStatus(Error, tr("Couldn't reach “%1”. Is the server running?").arg(res.name));
            return;
        }
        if (debug())
            qDebug().noquote() << "Kanzi Plex: using" << state->bestUri << "for" << res.name;
        setServer(res.id, res.name, state->bestUri, res.accessToken);
        loadSections();
    };

    for (const auto &connection : res.connections) {
        const int priority = connection.first;
        const QString uri = connection.second;
        QNetworkRequest req(QUrl(uri + u"/identity"_s));
        req.setRawHeader("Accept", "application/json");
        req.setRawHeader("X-Plex-Token", res.accessToken.toUtf8());
        req.setTransferTimeout(kProbeTimeoutMs);
        QNetworkReply *reply = m_nam.get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, priority, uri, state, decide] {
            reply->deleteLater();
            const bool ok = reply->error() == QNetworkReply::NoError;
            if (debug())
                qDebug().noquote() << "Kanzi Plex: probe" << uri << (ok ? "ok" : qPrintable(reply->errorString()));
            if (ok && priority < state->bestPriority) {
                state->bestPriority = priority;
                state->bestUri = uri;
            }
            --state->pending;
            if ((ok && priority == 0) || state->pending == 0)
                decide();
        });
    }
}

void Plex::connectManually(const QString &url, const QString &token)
{
    QString u = url.trimmed();
    if (u.isEmpty())
        return;
    if (!u.contains(u"://"_s))
        u.prepend(u"http://"_s);
    if (QUrl(u).port() == -1 && !u.startsWith(u"https"_s) && u.count(u':') < 2)
        u += u":32400"_s;
    m_sectionId.clear();
    setServer({}, QUrl(u).host(), u, token.trimmed().isEmpty() ? m_accountToken : token.trimmed());
    emit accountChanged();
    setStatus(Connecting);

    // Pick up the server's real name.
    serverGet(u"/"_s, {}, [this](const QJsonObject &c, const QString &err) {
        if (err.isEmpty() && !c.value(u"friendlyName").toString().isEmpty())
            setServer(c.value(u"machineIdentifier").toString(), c.value(u"friendlyName").toString(),
                      m_serverUrl, m_serverToken);
    });
    loadSections();
}

void Plex::loadSections()
{
    serverGet(u"/library/sections"_s, {}, [this](const QJsonObject &c, const QString &err) {
        if (!err.isEmpty()) {
            // The address may have changed (new IP, different network): rediscover it.
            if (!m_accountToken.isEmpty() && !m_serverId.isEmpty() && m_status == Connecting
                && m_resources.isEmpty()) {
                fetchResources();
                return;
            }
            setStatus(Error, err);
            return;
        }
        m_sections.clear();
        for (const QJsonValue &v : c.value(u"Directory").toArray()) {
            const QJsonObject d = v.toObject();
            if (d.value(u"type").toString() == u"artist")
                m_sections.append(QVariantMap{{u"id"_s, str(d, u"key")}, {u"title"_s, d.value(u"title").toString()}});
        }
        emit sectionsChanged();
        if (m_sections.isEmpty()) {
            setStatus(Error, tr("“%1” has no music libraries.").arg(m_serverName));
            return;
        }
        const bool known = std::any_of(m_sections.cbegin(), m_sections.cend(),
                                       [this](const QVariant &s) { return s.toMap().value(u"id"_s) == m_sectionId; });
        if (!known)
            setSectionId(m_sections.first().toMap().value(u"id"_s).toString());
        setStatus(Ready);
    });
}

void Plex::signOut()
{
    cancelSignIn();
    if (m_player->nowPlaying().value(u"source"_s) == u"plex"_s)
        m_player->pause();
    m_accountToken.clear();
    m_accountName.clear();
    m_resources.clear();
    m_sections.clear();
    m_sectionId.clear();
    ++m_probeGeneration;
    setServer({}, {}, {}, {});
    persist();
    emit accountChanged();
    emit serversChanged();
    emit sectionsChanged();
    emit sectionChanged();
    setStatus(SignedOut);
}

// ---------------------------------------------------------------------------
// Server requests
// ---------------------------------------------------------------------------

QUrl Plex::serverUrlFor(const QString &path, const QVariantMap &query) const
{
    QUrl url(m_serverUrl + path);
    QUrlQuery q(url);
    for (auto it = query.cbegin(); it != query.cend(); ++it)
        q.addQueryItem(it.key(), it.value().toString());
    url.setQuery(q);
    return url;
}

void Plex::serverGet(const QString &path, const QVariantMap &query, const JsonCallback &done)
{
    if (m_serverUrl.isEmpty()) {
        done({}, tr("Not connected to a Plex server"));
        return;
    }
    QNetworkRequest req(serverUrlFor(path, query));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("X-Plex-Token", m_serverToken.toUtf8());
    req.setRawHeader("X-Plex-Client-Identifier", m_clientId.toUtf8());
    req.setRawHeader("X-Plex-Product", "Kanzi");
    req.setRawHeader("X-Plex-Version", KANZI_VERSION);
    req.setRawHeader("X-Plex-Platform", "Linux");
    req.setTransferTimeout(kRequestTimeoutMs);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        if (debug())
            qDebug().noquote() << "Kanzi Plex:" << code << reply->request().url().toString(QUrl::RemoveQuery)
                               << data.size() << "bytes";
        if (reply->error() != QNetworkReply::NoError || code >= 400) {
            QString msg = reply->errorString();
            if (code == 401)
                msg = tr("Plex: not authorized. Sign in again.");
            else if (code == 404)
                msg = tr("Not found");
            qWarning().noquote() << "Kanzi Plex:" << code << reply->request().url().toString(QUrl::RemoveQuery) << msg;
            done({}, msg);
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(data).object();
        // Raw (non-JSON) bodies, e.g. lyrics, are passed through as { text }.
        done(root.isEmpty() ? QJsonObject{{u"text"_s, QString::fromUtf8(data)}}
                            : root.value(u"MediaContainer").toObject(),
             {});
    });
}

void Plex::call(const QJSValue &callback, const QVariant &result, const QString &error)
{
    if (!callback.isCallable() || !m_engine)
        return;
    const QJSValue err = error.isEmpty() ? QJSValue(QJSValue::NullValue)
                                         : m_engine->toScriptValue(QVariantMap{{u"message"_s, error}});
    const QJSValue res = error.isEmpty() ? m_engine->toScriptValue(result) : QJSValue(QJSValue::NullValue);
    const QJSValue ret = QJSValue(callback).call({res, err});
    if (ret.isError())
        qWarning().noquote() << "Kanzi: error in Plex callback -" << ret.toString()
                             << ret.property(u"stack"_s).toString();
}

// ---------------------------------------------------------------------------
// Plex metadata → Apple-shaped resources
// ---------------------------------------------------------------------------

QString Plex::artworkTemplate(const QString &thumb) const
{
    if (thumb.isEmpty() || m_serverUrl.isEmpty())
        return {};
    // {w} / {h} are filled in by AppleMusic.artworkUrl, like Apple's own templates.
    return m_serverUrl + u"/photo/:/transcode?width={w}&height={h}&minSize=1&upscale=1&url="_s
           + QString::fromLatin1(QUrl::toPercentEncoding(thumb))
           + u"&X-Plex-Token="_s + QString::fromLatin1(QUrl::toPercentEncoding(m_serverToken));
}

QVariantMap Plex::toResource(const QJsonObject &m) const
{
    static const QHash<QString, QString> types = {
        {u"track"_s, u"songs"_s}, {u"album"_s, u"albums"_s}, {u"artist"_s, u"artists"_s}, {u"playlist"_s, u"playlists"_s},
    };
    const QString plexType = m.value(u"type").toString();
    const QString type = types.value(plexType);
    if (type.isEmpty() || (plexType == u"playlist" && m.value(u"playlistType").toString() != u"audio"))
        return {};

    const QString id = str(m, u"ratingKey");
    QVariantMap a{{u"name"_s, m.value(u"title").toString()}};
    QVariantMap res{{u"id"_s, id}, {u"type"_s, type}, {u"source"_s, u"plex"_s}};

    QString thumb;
    const auto relate = [&res](const QString &key, const QString &relId, const QString &relType, const QString &name) {
        if (relId.isEmpty())
            return;
        QVariantMap rel = res.value(u"relationships"_s).toMap();
        rel.insert(key, QVariantMap{{u"data"_s, QVariantList{QVariantMap{
            {u"id"_s, relId}, {u"type"_s, relType}, {u"source"_s, u"plex"_s},
            {u"attributes"_s, QVariantMap{{u"name"_s, name}}}}}}});
        res.insert(u"relationships"_s, rel);
    };
    QStringList genres;
    for (const QJsonValue &g : m.value(u"Genre").toArray())
        genres.append(g.toObject().value(u"tag").toString());
    if (!genres.isEmpty())
        a.insert(u"genreNames"_s, genres);
    const QString summary = m.value(u"summary").toString();

    if (plexType == u"track") {
        thumb = m.value(u"parentThumb").toString(m.value(u"thumb").toString(m.value(u"grandparentThumb").toString()));
        const QString albumArtist = m.value(u"grandparentTitle").toString();
        a.insert(u"artistName"_s, m.value(u"originalTitle").toString(albumArtist));
        a.insert(u"albumName"_s, m.value(u"parentTitle").toString());
        a.insert(u"durationInMillis"_s, m.value(u"duration").toDouble());
        a.insert(u"trackNumber"_s, m.value(u"index").toInt());
        a.insert(u"discNumber"_s, m.value(u"parentIndex").toInt(1));
        relate(u"artists"_s, str(m, u"grandparentRatingKey"), u"artists"_s, albumArtist);
        relate(u"albums"_s, str(m, u"parentRatingKey"), u"albums"_s, m.value(u"parentTitle").toString());

        const QJsonObject media = firstOf(m.value(u"Media").toArray());
        const QJsonObject part = firstOf(media.value(u"Part").toArray());
        const QString partKey = part.value(u"key").toString();
        if (!partKey.isEmpty()) {
            res.insert(u"media"_s, QVariantMap{{u"part"_s, partKey}, {u"codec"_s, media.value(u"audioCodec").toString()}});
            a.insert(u"playParams"_s, QVariantMap{{u"id"_s, id}, {u"kind"_s, u"song"_s}});
        }
    } else if (plexType == u"album") {
        thumb = m.value(u"thumb").toString();
        a.insert(u"artistName"_s, m.value(u"parentTitle").toString());
        const QString released = m.value(u"originallyAvailableAt").toString();
        const int year = m.value(u"year").toInt();
        if (!released.isEmpty() || year)
            a.insert(u"releaseDate"_s, released.isEmpty() ? QString::number(year) : released);
        if (!summary.isEmpty())
            a.insert(u"description"_s, QVariantMap{{u"standard"_s, summary}});
        if (!m.value(u"studio").toString().isEmpty())
            a.insert(u"copyright"_s, m.value(u"studio").toString());
        a.insert(u"playParams"_s, QVariantMap{{u"id"_s, id}, {u"kind"_s, u"album"_s}});
        relate(u"artists"_s, str(m, u"parentRatingKey"), u"artists"_s, m.value(u"parentTitle").toString());
    } else if (plexType == u"artist") {
        thumb = m.value(u"thumb").toString();
        if (!summary.isEmpty())
            a.insert(u"artistBio"_s, summary);
    } else { // playlist
        thumb = m.value(u"thumb").toString(m.value(u"composite").toString());
        if (!summary.isEmpty())
            a.insert(u"description"_s, QVariantMap{{u"standard"_s, summary}});
        a.insert(u"playParams"_s, QVariantMap{{u"id"_s, id}, {u"kind"_s, u"playlist"_s}});
    }

    const QString art = artworkTemplate(thumb);
    if (!art.isEmpty())
        a.insert(u"artwork"_s, QVariantMap{{u"url"_s, art}});
    res.insert(u"attributes"_s, a);
    return res;
}

QVariantList Plex::toResources(const QJsonArray &items) const
{
    QVariantList out;
    for (const QJsonValue &v : items) {
        QVariantMap r = toResource(v.toObject());
        if (!r.isEmpty())
            out.append(r);
    }
    return out;
}

QVariantList Plex::hubsToShelves(const QJsonArray &hubs) const
{
    QVariantList out;
    for (const QJsonValue &v : hubs) {
        const QJsonObject hub = v.toObject();
        const QVariantList items = toResources(hub.value(u"Metadata").toArray());
        if (!items.isEmpty())
            out.append(QVariantMap{{u"title"_s, hub.value(u"title").toString()}, {u"items"_s, items}});
    }
    return out;
}

// ---------------------------------------------------------------------------
// Data for the pages
// ---------------------------------------------------------------------------

void Plex::home(const QJSValue &callback)
{
    serverGet(u"/hubs/sections/%1"_s.arg(m_sectionId), {{u"count"_s, 16}},
              [this, callback](const QJsonObject &c, const QString &err) {
                  call(callback, hubsToShelves(c.value(u"Hub").toArray()), err);
              });
}

void Plex::search(const QString &term, const QJSValue &callback)
{
    serverGet(u"/hubs/search"_s, {{u"query"_s, term}, {u"sectionId"_s, m_sectionId}, {u"limit"_s, 25}},
              [this, callback](const QJsonObject &c, const QString &err) {
                  QVariantMap out{{u"songs"_s, QVariantList()}, {u"albums"_s, QVariantList()},
                                  {u"artists"_s, QVariantList()}, {u"playlists"_s, QVariantList()}};
                  static const QHash<QString, QString> keys = {
                      {u"track"_s, u"songs"_s}, {u"album"_s, u"albums"_s},
                      {u"artist"_s, u"artists"_s}, {u"playlist"_s, u"playlists"_s},
                  };
                  for (const QJsonValue &v : c.value(u"Hub").toArray()) {
                      const QJsonObject hub = v.toObject();
                      const QString key = keys.value(hub.value(u"type").toString());
                      if (!key.isEmpty())
                          out.insert(key, out.value(key).toList() + toResources(hub.value(u"Metadata").toArray()));
                  }
                  call(callback, out, err);
              });
}

void Plex::browse(const QString &kind, int start, const QJSValue &callback)
{
    QString path = u"/library/sections/%1/all"_s.arg(m_sectionId);
    QVariantMap query{{u"X-Plex-Container-Start"_s, start}, {u"X-Plex-Container-Size"_s, kPageSize}};
    if (kind == u"albums")
        query.insert(QVariantMap{{u"type"_s, 9}, {u"sort"_s, u"titleSort"_s}});
    else if (kind == u"artists")
        query.insert(QVariantMap{{u"type"_s, 8}, {u"sort"_s, u"titleSort"_s}});
    else if (kind == u"songs")
        query.insert(QVariantMap{{u"type"_s, 10}, {u"sort"_s, u"titleSort"_s}});
    else if (kind == u"recent")
        query.insert(QVariantMap{{u"type"_s, 9}, {u"sort"_s, u"addedAt:desc"_s}});
    else if (kind == u"playlists") {
        path = u"/playlists"_s;
        query.insert(QVariantMap{{u"playlistType"_s, u"audio"_s}, {u"sectionID"_s, m_sectionId}});
    }
    serverGet(path, query, [this, callback, start](const QJsonObject &c, const QString &err) {
        const QJsonArray items = metadataOf(c);
        const int total = c.value(u"totalSize").toInt(-1);
        const int end = start + int(items.size());
        const bool more = total >= 0 ? end < total : items.size() >= kPageSize;
        call(callback, QVariantMap{{u"data"_s, toResources(items)}, {u"next"_s, more ? end : -1}}, err);
    });
}

void Plex::collection(const QString &kind, const QString &id, const QJSValue &callback)
{
    const bool playlist = kind == u"playlist";
    const QString itemPath = playlist ? u"/playlists/%1"_s.arg(id) : u"/library/metadata/%1"_s.arg(id);
    const QString tracksPath = playlist ? u"/playlists/%1/items"_s.arg(id) : u"/library/metadata/%1/children"_s.arg(id);
    serverGet(itemPath, {}, [this, callback, tracksPath](const QJsonObject &c, const QString &err) {
        if (!err.isEmpty())
            return call(callback, {}, err);
        const QVariantMap resource = toResource(firstOf(metadataOf(c)));
        serverGet(tracksPath, {}, [this, callback, resource](const QJsonObject &c, const QString &err) {
            call(callback, QVariantMap{{u"resource"_s, resource}, {u"tracks"_s, toResources(metadataOf(c))}}, err);
        });
    });
}

void Plex::artist(const QString &id, const QJSValue &callback)
{
    struct State {
        int pending = 4;
        QString error;
        QVariantMap artist;
        QVariantList albums, topSongs, related;
    };
    auto state = std::make_shared<State>();
    const auto finish = [this, state, callback] {
        if (--state->pending > 0)
            return;
        QVariantList shelves;
        if (!state->albums.isEmpty())
            shelves.append(QVariantMap{{u"title"_s, tr("Albums")}, {u"items"_s, state->albums}});
        shelves += state->related;
        if (state->artist.isEmpty())
            return call(callback, {}, state->error);
        call(callback, QVariantMap{{u"artist"_s, state->artist}, {u"topSongs"_s, state->topSongs},
                                   {u"shelves"_s, shelves}});
    };

    serverGet(u"/library/metadata/%1"_s.arg(id), {}, [this, state, finish](const QJsonObject &c, const QString &err) {
        state->error = err;
        state->artist = toResource(firstOf(metadataOf(c)));
        finish();
    });
    serverGet(u"/library/metadata/%1/children"_s.arg(id), {}, [this, state, finish](const QJsonObject &c, const QString &) {
        QJsonArray albums = metadataOf(c);
        QList<QJsonObject> sorted;
        for (const QJsonValue &v : albums)
            sorted.append(v.toObject());
        std::stable_sort(sorted.begin(), sorted.end(), [](const QJsonObject &a, const QJsonObject &b) {
            return a.value(u"year").toInt() > b.value(u"year").toInt();
        });
        for (const QJsonObject &o : sorted) {
            QVariantMap r = toResource(o);
            if (!r.isEmpty())
                state->albums.append(r);
        }
        finish();
    });
    // Popularity comes from Plex's Last.fm data; without it this is just some of their songs.
    serverGet(u"/library/sections/%1/all"_s.arg(m_sectionId),
              {{u"type"_s, 10}, {u"artist.id"_s, id}, {u"sort"_s, u"ratingCount:desc"_s},
               {u"X-Plex-Container-Start"_s, 0}, {u"X-Plex-Container-Size"_s, 10}},
              [this, state, finish](const QJsonObject &c, const QString &) {
                  state->topSongs = toResources(metadataOf(c));
                  finish();
              });
    // Related hubs (similar artists, appearances…); their own albums are already shown.
    serverGet(u"/hubs/metadata/%1"_s.arg(id), {{u"count"_s, 16}},
              [this, state, finish, id](const QJsonObject &c, const QString &) {
                  for (const QVariant &v : hubsToShelves(c.value(u"Hub").toArray())) {
                      QVariantMap shelf = v.toMap();
                      QVariantList items;
                      for (const QVariant &item : shelf.value(u"items"_s).toList()) {
                          const QVariantMap m = item.toMap();
                          const QVariantList artists = m.value(u"relationships"_s).toMap()
                                                           .value(u"artists"_s).toMap().value(u"data"_s).toList();
                          const bool own = m.value(u"type"_s) == u"albums"_s && !artists.isEmpty()
                                           && artists.first().toMap().value(u"id"_s) == id;
                          if (!own && m.value(u"id"_s) != id)
                              items.append(item);
                      }
                      if (!items.isEmpty()) {
                          shelf.insert(u"items"_s, items);
                          state->related.append(shelf);
                      }
                  }
                  finish();
              });
}

void Plex::lyrics(const QString &trackId, const QJSValue &callback)
{
    serverGet(u"/library/metadata/%1"_s.arg(trackId), {}, [this, callback](const QJsonObject &c, const QString &err) {
        if (!err.isEmpty())
            return call(callback, QVariantList(), err);
        QString key;
        bool synced = false;
        const QJsonObject track = firstOf(metadataOf(c));
        for (const QJsonValue &mv : track.value(u"Media").toArray()) {
            for (const QJsonValue &pv : mv.toObject().value(u"Part").toArray()) {
                for (const QJsonValue &sv : pv.toObject().value(u"Stream").toArray()) {
                    const QJsonObject s = sv.toObject();
                    if (s.value(u"streamType").toInt() != 4 || s.value(u"key").toString().isEmpty())
                        continue;
                    const bool lrc = s.value(u"codec").toString() == u"lrc" || s.value(u"format").toString() == u"lrc";
                    if (key.isEmpty() || (lrc && !synced)) {
                        key = s.value(u"key").toString();
                        synced = lrc;
                    }
                }
            }
        }
        if (key.isEmpty())
            return call(callback, QVariantList());
        serverGet(key, {}, [this, callback, synced](const QJsonObject &c, const QString &err) {
            call(callback, err.isEmpty() ? parseLyrics(c.value(u"text").toString(), synced) : QVariantList(), err);
        });
    });
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

QVariantMap Plex::queueItem(const QVariantMap &track) const
{
    const QVariantMap media = track.value(u"media"_s).toMap();
    const QString part = media.value(u"part"_s).toString();
    if (part.isEmpty() || m_serverUrl.isEmpty())
        return {};
    const QVariantMap a = track.value(u"attributes"_s).toMap();
    const QString id = track.value(u"id"_s).toString();

    QUrl direct(m_serverUrl + part);
    direct.setQuery(QUrlQuery{{u"X-Plex-Token"_s, m_serverToken}});

    const QUrl transcode = serverUrlFor(u"/music/:/transcode/universal/start.mp3"_s, {
        {u"path"_s, u"/library/metadata/"_s + id},
        {u"mediaIndex"_s, 0},
        {u"partIndex"_s, 0},
        {u"protocol"_s, u"http"_s},
        {u"directPlay"_s, 0},
        {u"directStream"_s, 0},
        {u"musicBitrate"_s, 320},
        {u"session"_s, QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {u"X-Plex-Client-Identifier"_s, m_clientId},
        {u"X-Plex-Product"_s, u"Kanzi"_s},
        {u"X-Plex-Platform"_s, u"Chrome"_s},
        {u"X-Plex-Token"_s, m_serverToken},
    });
    const bool canDirect = directPlayable(media.value(u"codec"_s).toString());

    return {
        {u"id"_s, id},
        {u"source"_s, u"plex"_s},
        {u"type"_s, u"songs"_s},
        {u"catalogId"_s, QString()},
        {u"title"_s, a.value(u"name"_s)},
        {u"artist"_s, a.value(u"artistName"_s)},
        {u"album"_s, a.value(u"albumName"_s)},
        {u"artwork"_s, a.value(u"artwork"_s).toMap().value(u"url"_s)},
        {u"artworkColor"_s, QString()},
        {u"duration"_s, a.value(u"durationInMillis"_s).toDouble() / 1000.0},
        {u"explicit"_s, false},
        {u"hasLyrics"_s, false},
        {u"url"_s, QString()},
        {u"relationships"_s, track.value(u"relationships"_s)},
        {u"src"_s, (canDirect ? direct : transcode).toString(QUrl::FullyEncoded)},
        {u"fallbackSrc"_s, canDirect ? transcode.toString(QUrl::FullyEncoded) : QString()},
    };
}

void Plex::playTracks(const QVariantList &tracks, int startIndex, bool shuffle)
{
    QVariantList items;
    int start = 0;
    for (qsizetype i = 0; i < tracks.size(); ++i) {
        const QVariantMap item = queueItem(tracks[i].toMap());
        if (item.isEmpty())
            continue;
        if (i <= startIndex)
            start = int(items.size());
        items.append(item);
    }
    if (items.isEmpty())
        return;
    m_player->playTracks(items, start, shuffle);
}

void Plex::resolveTracks(const QVariantMap &item, const std::function<void(QVariantList)> &done)
{
    const QString type = item.value(u"type"_s).toString();
    const QString id = item.value(u"id"_s).toString();
    if (type == u"songs") {
        done({item});
        return;
    }
    QString path;
    if (type == u"albums")
        path = u"/library/metadata/%1/children"_s.arg(id);
    else if (type == u"playlists")
        path = u"/playlists/%1/items"_s.arg(id);
    else if (type == u"artists")
        path = u"/library/metadata/%1/allLeaves"_s.arg(id);
    else
        return;
    serverGet(path, {}, [this, done](const QJsonObject &c, const QString &err) {
        if (err.isEmpty())
            done(toResources(metadataOf(c)));
    });
}

void Plex::play(const QVariant &item, bool shuffle)
{
    resolveTracks(item.toMap(), [this, shuffle](const QVariantList &tracks) { playTracks(tracks, 0, shuffle); });
}

void Plex::enqueue(const QVariant &item, bool next)
{
    resolveTracks(item.toMap(), [this, next](const QVariantList &tracks) {
        QVariantList items;
        for (const QVariant &t : tracks) {
            const QVariantMap q = queueItem(t.toMap());
            if (!q.isEmpty())
                items.append(q);
        }
        if (!items.isEmpty())
            m_player->queueTracks(items, next);
    });
}

void Plex::reportTimeline(const QString &state)
{
    if (m_reportedTrack.isEmpty() || m_serverUrl.isEmpty())
        return;
    const qint64 time = qint64(m_lastPosition * 1000);
    QNetworkRequest req(serverUrlFor(u"/:/timeline"_s, {
        {u"ratingKey"_s, m_reportedTrack},
        {u"key"_s, u"/library/metadata/"_s + m_reportedTrack},
        {u"state"_s, state},
        {u"time"_s, time},
        {u"duration"_s, qint64(m_reportedDuration * 1000)},
    }));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("X-Plex-Token", m_serverToken.toUtf8());
    req.setRawHeader("X-Plex-Client-Identifier", m_clientId.toUtf8());
    req.setRawHeader("X-Plex-Product", "Kanzi");
    req.setRawHeader("X-Plex-Version", KANZI_VERSION);
    req.setRawHeader("X-Plex-Platform", "Linux");
    req.setRawHeader("X-Plex-Device-Name", ("Kanzi on " + QSysInfo::machineHostName()).toUtf8());
    req.setTransferTimeout(kRequestTimeoutMs);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}
