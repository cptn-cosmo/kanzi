#pragma once

#include <QJSValue>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QtWebEngineQuick/QQuickWebEngineProfile>

// Thin client for api.music.apple.com plus developer-token bootstrapping.
// Exposed to QML as the `AppleMusic` singleton.
class AppleMusic : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Persistent browser profile shared by the hidden player and the sign-in window.
    // (Kept off Player: that object is mirrored to the page over the WebChannel.)
    Q_PROPERTY(QQuickWebEngineProfile *webProfile READ webProfile CONSTANT)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY statusChanged)
    Q_PROPERTY(QString developerToken READ developerToken NOTIFY developerTokenChanged)
    Q_PROPERTY(QString userToken READ userToken NOTIFY userTokenChanged)
    Q_PROPERTY(bool authorized READ authorized NOTIFY userTokenChanged)
    Q_PROPERTY(QString storefront READ storefront NOTIFY storefrontChanged)
    Q_PROPERTY(QString language READ language NOTIFY storefrontChanged)

public:
    enum Status { Loading, Ready, Error };
    Q_ENUM(Status)

    // Deliberately not default-constructible: QML prefers a default constructor over
    // create(), and would otherwise silently build a second, disconnected instance.
    explicit AppleMusic(QObject *parent);
    ~AppleMusic() override;

    static AppleMusic *create(QQmlEngine *qml, QJSEngine *js);
    static AppleMusic *instance() { return s_instance; }

    QQuickWebEngineProfile *webProfile() const { return m_profile; }
    Status status() const { return m_status; }
    QString errorString() const { return m_error; }
    QString developerToken() const { return m_developerToken; }
    QString userToken() const { return m_userToken; }
    bool authorized() const { return !m_userToken.isEmpty(); }
    QString storefront() const { return m_storefront; }
    QString language() const { return m_language; }

    // `path` may contain `{sf}` which is replaced with the user's storefront, and may
    // already carry a query string (e.g. a `next` link from a paginated response).
    // The callback receives (result, error) where result is the parsed JSON body.
    Q_INVOKABLE void get(const QString &path, const QVariantMap &query, const QJSValue &callback);
    Q_INVOKABLE void request(const QString &method, const QString &path, const QVariantMap &query,
                             const QVariant &body, const QJSValue &callback);

    // Fills in an Apple artwork URL template ({w}x{h}) for a square size in pixels.
    Q_INVOKABLE QString artworkUrl(const QVariant &artwork, int size) const;
    // Parses Apple's TTML lyrics into [{ begin, end, text }] with times in milliseconds.
    Q_INVOKABLE QVariantList parseTtml(const QString &ttml) const;

    Q_INVOKABLE void retry();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void copyToClipboard(const QString &text) const;

    void setUserToken(const QString &token);

signals:
    void statusChanged();
    void developerTokenChanged();
    void userTokenChanged();
    void storefrontChanged();
    void signOutRequested();

private:
    void setStatus(Status s, const QString &error = {});
    void loadDeveloperToken();
    void scrapeDeveloperToken();
    void acceptDeveloperToken(const QString &token, bool persist);
    void fetchStorefront();
    QNetworkRequest buildRequest(const QUrl &url) const;
    QUrl buildUrl(const QString &path, const QVariantMap &query) const;

    static inline AppleMusic *s_instance = nullptr;

    QNetworkAccessManager m_nam;
    QQuickWebEngineProfile *m_profile = nullptr;
    QPointer<QQmlEngine> m_engine;
    Status m_status = Loading;
    QString m_error;
    QString m_developerToken;
    bool m_skipBuiltInToken = false;
    QString m_userToken;
    QString m_storefront = QStringLiteral("us");
    QString m_language = QStringLiteral("en-US");
};
