#pragma once

#include <QJSValue>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QTimer>
#include <functional>

class Player;

// Client for a Plex Media Server music library, exposed to QML as the `Plex` singleton.
//
// Sign-in uses plex.tv's PIN flow (the browser opens app.plex.tv/auth), then the server's
// connections are probed to find one that answers. A server URL + token can also be
// entered by hand.
//
// Plex metadata is converted into the same resource shape the Apple Music pages use
// ({ id, type: "albums" | "artists" | "playlists" | "songs", attributes: { name, artistName,
// artwork: { url }, … } }) plus `source: "plex"`, so the shared components work unchanged.
class Plex : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY statusChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY statusChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY accountChanged)
    Q_PROPERTY(bool signingIn READ signingIn NOTIFY signingInChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY accountChanged)
    // [{ id, name, owned }] from the plex.tv account (empty for a manual connection).
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(QString serverId READ serverId NOTIFY serverChanged)
    Q_PROPERTY(QString serverName READ serverName NOTIFY serverChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverChanged)
    // Music libraries on the server: [{ id, title }].
    Q_PROPERTY(QVariantList sections READ sections NOTIFY sectionsChanged)
    Q_PROPERTY(QString sectionId READ sectionId WRITE setSectionId NOTIFY sectionChanged)

public:
    enum Status { SignedOut, Connecting, Ready, Error };
    Q_ENUM(Status)

    // Not default-constructible on purpose; see AppleMusic.
    explicit Plex(Player *player, QObject *parent);
    ~Plex() override;

    static Plex *create(QQmlEngine *qml, QJSEngine *js);
    static Plex *instance() { return s_instance; }

    Status status() const { return m_status; }
    QString errorString() const { return m_error; }
    bool ready() const { return m_status == Ready; }
    bool signedIn() const { return !m_accountToken.isEmpty() || !m_serverToken.isEmpty(); }
    bool signingIn() const { return m_pinId != 0; }
    QString accountName() const { return m_accountName; }
    QVariantList servers() const;
    QString serverId() const { return m_serverId; }
    QString serverName() const { return m_serverName; }
    QString serverUrl() const { return m_serverUrl; }
    QVariantList sections() const { return m_sections; }
    QString sectionId() const { return m_sectionId; }
    void setSectionId(const QString &id);

    // ---- account / connection ----
    Q_INVOKABLE void signIn();
    Q_INVOKABLE void cancelSignIn();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void selectServer(const QString &id);
    Q_INVOKABLE void connectManually(const QString &url, const QString &token);
    Q_INVOKABLE void reconnect();

    // ---- data; every callback receives (result, error) like AppleMusic.get ----
    // [{ title, items }]
    Q_INVOKABLE void home(const QJSValue &callback);
    // { songs, albums, artists, playlists }
    Q_INVOKABLE void search(const QString &term, const QJSValue &callback);
    // kind: albums | artists | songs | playlists | recent → { data, next } (next = -1 at the end)
    Q_INVOKABLE void browse(const QString &kind, int start, const QJSValue &callback);
    // kind: album | playlist → { resource, tracks }
    Q_INVOKABLE void collection(const QString &kind, const QString &id, const QJSValue &callback);
    // { artist, topSongs, shelves: [{ title, items }] }
    Q_INVOKABLE void artist(const QString &id, const QJSValue &callback);
    // [{ begin, end, text }] (ms; begin = -1 for unsynced lyrics)
    Q_INVOKABLE void lyrics(const QString &trackId, const QJSValue &callback);

    // ---- playback ----
    Q_INVOKABLE void play(const QVariant &item, bool shuffle = false);
    Q_INVOKABLE void enqueue(const QVariant &item, bool next);
    Q_INVOKABLE void playTracks(const QVariantList &tracks, int startIndex = 0, bool shuffle = false);

signals:
    void statusChanged();
    void accountChanged();
    void signingInChanged();
    void serversChanged();
    void serverChanged();
    void sectionsChanged();
    void sectionChanged();
    // The page has to be reloaded (new server origin).
    void serverUrlChanged();

private:
    using JsonCallback = std::function<void(const QJsonObject &container, const QString &error)>;

    struct Resource {
        QString id;
        QString name;
        bool owned = false;
        QString accessToken;
        QList<QPair<int, QString>> connections; // (priority, uri), lower = better
    };

    void setStatus(Status s, const QString &error = {});
    void setServer(const QString &id, const QString &name, const QString &url, const QString &token);
    void persist();
    void pollPin();
    void fetchAccount();
    void fetchResources();
    void probe(const Resource &res);
    void loadSections();

    QNetworkRequest plexTvRequest(const QString &path) const;
    QUrl serverUrlFor(const QString &path, const QVariantMap &query = {}) const;
    void serverGet(const QString &path, const QVariantMap &query, const JsonCallback &done);
    void call(const QJSValue &callback, const QVariant &result, const QString &error = {});

    QVariantMap toResource(const QJsonObject &m) const;
    QVariantList toResources(const QJsonArray &items) const;
    QVariantList hubsToShelves(const QJsonArray &hubs) const;
    QString artworkTemplate(const QString &thumb) const;
    QVariantMap queueItem(const QVariantMap &track) const;
    void resolveTracks(const QVariantMap &item, const std::function<void(QVariantList)> &done);

    // Plex "now playing" / history reporting.
    void reportTimeline(const QString &state);

    static inline Plex *s_instance = nullptr;

    Player *m_player;
    QNetworkAccessManager m_nam;
    QPointer<QQmlEngine> m_engine;
    Status m_status = SignedOut;
    QString m_error;
    QString m_clientId;
    QString m_accountToken;
    QString m_accountName;
    QList<Resource> m_resources;
    QString m_serverId;
    QString m_serverName;
    QString m_serverUrl;
    QString m_serverToken;
    QVariantList m_sections;
    QString m_sectionId;
    int m_probeGeneration = 0;

    qint64 m_pinId = 0;
    QString m_pinCode;
    int m_pinPolls = 0;
    QTimer m_pinTimer;

    QTimer m_timelineTimer;
    QString m_reportedTrack;
    double m_reportedDuration = 0;
    double m_lastPosition = 0;
};
