#include "applemusic.h"

#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QUrlQuery>
#include <QXmlStreamReader>
#include <QStandardPaths>
#include <QWebEngineClientHints>
#include <QWebEngineScript>

namespace {

const QString kApiBase = QStringLiteral("https://api.music.apple.com");
const QString kWebOrigin = QStringLiteral("https://music.apple.com");
// The Apple Music web player's developer token (issued 2026-11-05, expires 2027-01-14).
// Once it expires Kanzi pulls the current one from the web bundle instead.
const QString kBuiltInDeveloperToken = QStringLiteral(
    "eyJ0eXAiOiJKV1QiLCJhbGciOiJFUzI1NiIsImtpZCI6IldlYlBsYXlLaWQifQ.eyJpc3MiOiJBTVBXZWJQbGF5IiwiaWF0IjoxNzkzOTAwNzA4LCJleHAiOjE3OTk5NDg3MDgsInJvb3RfaHR0cHNfb3JpZ2luIjpbImFwcGxlLmNvbSJdfQ.7L-PYJHjO8PpFdhKS-OSkf_snyFvg5cq-JDERUs_N_UcUX9Vk0kf1WLatgDeEvhB_Arg1b5Tl-99G1v6hUwH0A");
const QByteArray kUserAgent =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36";

// Returns the `exp` claim of a JWT, or 0 if it can't be read.
qint64 jwtExpiry(const QString &token)
{
    const QStringList parts = token.split(u'.');
    if (parts.size() != 3)
        return 0;
    const QByteArray payload = QByteArray::fromBase64(
        parts[1].toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QJsonDocument::fromJson(payload).object().value(u"exp").toInteger();
}

bool tokenUsable(const QString &token)
{
    if (token.isEmpty())
        return false;
    const qint64 exp = jwtExpiry(token);
    // Tokens without a readable expiry are trusted; otherwise require a day of headroom.
    return exp == 0 || exp > QDateTime::currentSecsSinceEpoch() + 24 * 3600;
}

// TTML clock values look like "12.345", "1:02.345" or "00:01:02.345".
qint64 parseClock(QStringView v)
{
    if (v.isEmpty())
        return -1;
    if (v.endsWith(u"ms"))
        return qint64(v.chopped(2).toDouble());
    if (v.endsWith(u's'))
        return qint64(v.chopped(1).toDouble() * 1000);
    double seconds = 0;
    for (QStringView part : v.split(u':'))
        seconds = seconds * 60 + part.toDouble();
    return qint64(seconds * 1000);
}

} // namespace

AppleMusic::AppleMusic(QObject *parent)
    : QObject(parent)
{
    if (s_instance)
        qFatal("Kanzi: %s constructed twice", "AppleMusic");
    s_instance = this;

    m_profile = new QQuickWebEngineProfile(QStringLiteral("kanzi"), this);
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_profile->setPersistentStoragePath(dataDir + QStringLiteral("/webengine"));
    m_profile->setCachePath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/webengine"));
    m_profile->setPersistentCookiesPolicy(QQuickWebEngineProfile::ForcePersistentCookies);
    // Present as the Chrome release QtWebEngine is built on; Apple's sign-in pages
    // branch on both the UA string and the client-hint brands.
    m_profile->setHttpUserAgent(QString::fromLatin1(kUserAgent));
    m_profile->clientHints()->setFullVersionList({
        {QStringLiteral("Chromium"), QStringLiteral("140.0.7339.0")},
        {QStringLiteral("Google Chrome"), QStringLiteral("140.0.7339.0")},
        {QStringLiteral("Not=A?Brand"), QStringLiteral("24.0.0.0")},
    });

    // Apple's sign-in widget probes WebAuthn before showing the password flow:
    // PublicKeyCredential.getClientCapabilities() and friends. In QtWebEngine those
    // promises never settle (the browser-side WebAuthn delegate isn't wired up), so the
    // widget never initialises and every later step throws "reading 'trigger'".
    // Answer the probes immediately with "no platform passkeys"; plain security keys
    // still work through navigator.credentials.get and the overlay in LoginWindow.qml.
    QWebEngineScript noConditionalWebAuthn;
    noConditionalWebAuthn.setName(QStringLiteral("kanzi-webauthn-probes"));
    noConditionalWebAuthn.setInjectionPoint(QWebEngineScript::DocumentCreation);
    noConditionalWebAuthn.setWorldId(QWebEngineScript::MainWorld);
    noConditionalWebAuthn.setRunsOnSubFrames(true);
    noConditionalWebAuthn.setSourceCode(QStringLiteral(R"js(
        (() => {
            const P = window.PublicKeyCredential;
            if (!P || !/(^|\.)apple\.com$/.test(location.hostname))
                return;
            const define = (name, fn) => {
                try { Object.defineProperty(P, name, { value: fn, configurable: true, writable: true }); } catch (e) {}
            };
            define("getClientCapabilities", () => Promise.resolve({
                conditionalGet: false, conditionalCreate: false, hybridTransport: false,
                passkeyPlatformAuthenticator: false, userVerifyingPlatformAuthenticator: false,
            }));
            define("isConditionalMediationAvailable", () => Promise.resolve(false));
            define("isUserVerifyingPlatformAuthenticatorAvailable", () => Promise.resolve(false));
            console.warn("[kanzi] WebAuthn probes patched for", location.hostname);
        })();
    )js"));
    // QQuickWebEngineScriptCollection is private API; its insert() is invokable though.
    auto *scripts = m_profile->property("userScripts").value<QObject *>();
    if (!scripts || !QMetaObject::invokeMethod(scripts, "insert", Q_ARG(QWebEngineScript, noConditionalWebAuthn)))
        qWarning("Kanzi: couldn't install sign-in compatibility script");

    QSettings settings;
    m_userToken = settings.value(QStringLiteral("auth/userToken")).toString();
    m_storefront = settings.value(QStringLiteral("auth/storefront"), m_storefront).toString();
    m_language = settings.value(QStringLiteral("auth/language"), m_language).toString();

    loadDeveloperToken();
    if (authorized())
        fetchStorefront();
}

AppleMusic::~AppleMusic()
{
    s_instance = nullptr;
}

AppleMusic *AppleMusic::create(QQmlEngine *qml, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    s_instance->m_engine = qml;
    return s_instance;
}

void AppleMusic::setStatus(Status s, const QString &error)
{
    if (m_status == s && m_error == error)
        return;
    m_status = s;
    m_error = error;
    emit statusChanged();
}

void AppleMusic::retry()
{
    // Something went wrong with the current token, so fetch a fresh one.
    m_skipBuiltInToken = true;
    QSettings().remove(QStringLiteral("auth/developerToken"));
    loadDeveloperToken();
}

void AppleMusic::loadDeveloperToken()
{
    setStatus(Loading);

    // 1. Development override, e.g. a token minted from your own MusicKit key.
    const QString env = qEnvironmentVariable("KANZI_DEVELOPER_TOKEN");
    if (!env.isEmpty())
        return acceptDeveloperToken(env, false);

    QSettings settings;
    settings.remove(QStringLiteral("developerTokenOverride")); // no longer a setting

    // 2. The built-in token, while it's valid.
    if (!m_skipBuiltInToken && tokenUsable(kBuiltInDeveloperToken))
        return acceptDeveloperToken(kBuiltInDeveloperToken, false);

    // 3. A previously scraped token that hasn't expired yet.
    const QString cached = settings.value(QStringLiteral("auth/developerToken")).toString();
    if (tokenUsable(cached))
        return acceptDeveloperToken(cached, false);

    // 4. Pull the token the Apple Music web player currently ships with.
    scrapeDeveloperToken();
}

void AppleMusic::scrapeDeveloperToken()
{
    QNetworkRequest req(QUrl(kWebOrigin + QStringLiteral("/us/browse")));
    req.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return setStatus(Error, tr("Couldn't reach music.apple.com: %1").arg(reply->errorString()));

        const QString html = QString::fromUtf8(reply->readAll());
        static const QRegularExpression scriptRe(
            QStringLiteral(R"re(<script[^>]+src="(/assets/index[^"]*\.js)")re"));
        QString scriptPath;
        for (auto it = scriptRe.globalMatch(html); it.hasNext();) {
            const QString p = it.next().captured(1);
            if (!p.contains(u"legacy")) {
                scriptPath = p;
                break;
            }
        }
        if (scriptPath.isEmpty())
            return setStatus(Error, tr("Couldn't locate the Apple Music web bundle."));

        QNetworkRequest jsReq(QUrl(kWebOrigin + scriptPath));
        jsReq.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
        QNetworkReply *js = m_nam.get(jsReq);
        connect(js, &QNetworkReply::finished, this, [this, js] {
            js->deleteLater();
            if (js->error() != QNetworkReply::NoError)
                return setStatus(Error, tr("Couldn't download the web bundle: %1").arg(js->errorString()));
            // Any JWT (header and payload both start with base64 "{\"" = "eyJ"); keep the first
            // one whose payload reads like a MusicKit token that hasn't expired.
            static const QRegularExpression jwtRe(
                QStringLiteral(R"(eyJ[A-Za-z0-9_\-]+\.eyJ[A-Za-z0-9_\-]+\.[A-Za-z0-9_\-]+)"));
            const QString bundle = QString::fromUtf8(js->readAll());
            for (auto it = jwtRe.globalMatch(bundle); it.hasNext();) {
                const QString token = it.next().captured(0);
                if (jwtExpiry(token) > QDateTime::currentSecsSinceEpoch())
                    return acceptDeveloperToken(token, true);
            }
            setStatus(Error, tr("Couldn't find a developer token in the web bundle."));
        });
    });
}

void AppleMusic::acceptDeveloperToken(const QString &token, bool persist)
{
    if (persist)
        QSettings().setValue(QStringLiteral("auth/developerToken"), token);
    if (m_developerToken != token) {
        m_developerToken = token;
        emit developerTokenChanged();
    }
    setStatus(Ready);
}

void AppleMusic::setUserToken(const QString &token)
{
    if (token == m_userToken)
        return;
    m_userToken = token;
    QSettings settings;
    if (token.isEmpty())
        settings.remove(QStringLiteral("auth/userToken"));
    else
        settings.setValue(QStringLiteral("auth/userToken"), token);
    emit userTokenChanged();
    if (!token.isEmpty())
        fetchStorefront();
}

void AppleMusic::signOut()
{
    setUserToken({});
    emit signOutRequested();
}

void AppleMusic::fetchStorefront()
{
    QNetworkReply *reply = m_nam.get(buildRequest(buildUrl(QStringLiteral("/v1/me/storefront"), {})));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 401 || code == 403) {
            // Stale user token; drop it so the UI asks to sign in again.
            setUserToken({});
            return;
        }
        const QJsonObject sf = QJsonDocument::fromJson(reply->readAll())
                                   .object().value(u"data").toArray().first().toObject();
        const QString id = sf.value(u"id").toString();
        if (id.isEmpty())
            return;
        m_storefront = id;
        m_language = sf.value(u"attributes").toObject().value(u"defaultLanguageTag").toString(m_language);
        QSettings settings;
        settings.setValue(QStringLiteral("auth/storefront"), m_storefront);
        settings.setValue(QStringLiteral("auth/language"), m_language);
        emit storefrontChanged();
    });
}

QUrl AppleMusic::buildUrl(const QString &path, const QVariantMap &query) const
{
    QString p = path;
    p.replace(QStringLiteral("{sf}"), m_storefront);
    QUrl url = p.startsWith(u"http") ? QUrl(p) : QUrl(kApiBase + p);
    QUrlQuery q(url);
    for (auto it = query.cbegin(); it != query.cend(); ++it) {
        q.removeAllQueryItems(it.key());
        q.addQueryItem(it.key(), it.value().toString());
    }
    if (!q.hasQueryItem(QStringLiteral("l")) && !m_language.isEmpty())
        q.addQueryItem(QStringLiteral("l"), m_language);
    url.setQuery(q);
    return url;
}

QNetworkRequest AppleMusic::buildRequest(const QUrl &url) const
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
    req.setRawHeader("Authorization", "Bearer " + m_developerToken.toUtf8());
    if (!m_userToken.isEmpty())
        req.setRawHeader("Music-User-Token", m_userToken.toUtf8());
    // The web player's token is bound to its origin.
    req.setRawHeader("Origin", kWebOrigin.toUtf8());
    req.setRawHeader("Referer", (kWebOrigin + u'/').toUtf8());
    req.setRawHeader("Accept", "application/json");
    return req;
}

void AppleMusic::get(const QString &path, const QVariantMap &query, const QJSValue &callback)
{
    request(QStringLiteral("GET"), path, query, {}, callback);
}

void AppleMusic::request(const QString &method, const QString &path, const QVariantMap &query,
                         const QVariant &body, const QJSValue &callback)
{
    QNetworkRequest req = buildRequest(buildUrl(path, query));
    QByteArray payload;
    if (body.isValid() && !body.isNull()) {
        payload = QJsonDocument::fromVariant(body).toJson(QJsonDocument::Compact);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }
    QNetworkReply *reply = m_nam.sendCustomRequest(req, method.toLatin1(), payload);

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback] {
        reply->deleteLater();
        if (!callback.isCallable() || !m_engine)
            return;

        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();
        QJSValue result = QJSValue::NullValue;
        QJSValue error = QJSValue::NullValue;

        if (!data.isEmpty()) {
            QJsonParseError perr;
            const QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
            if (perr.error == QJsonParseError::NoError)
                result = m_engine->toScriptValue(doc.toVariant());
        }
        if (reply->error() != QNetworkReply::NoError || code >= 400) {
            if (code != 404 || qEnvironmentVariableIsSet("KANZI_DEBUG"))
                qWarning().noquote() << "Kanzi API:" << code << reply->request().url().toString(QUrl::RemoveQuery)
                                     << reply->errorString() << QString::fromUtf8(data.left(300));
            QString msg = reply->errorString();
            const QJsonArray errors = QJsonDocument::fromJson(data).object().value(u"errors").toArray();
            if (!errors.isEmpty()) {
                const QJsonObject e = errors.first().toObject();
                const QString detail = e.value(u"detail").toString(e.value(u"title").toString());
                if (!detail.isEmpty())
                    msg = tr("Apple Music: %1").arg(detail);
            }
            if (code == 401)
                msg = tr("Not authorized");
            else if (code == 403)
                msg = tr("Forbidden: sign in or check your subscription");
            else if (code == 404)
                msg = tr("Not found");
            error = m_engine->toScriptValue(QVariantMap{{QStringLiteral("status"), code},
                                                         {QStringLiteral("message"), msg}});
        }
        if (result.isNull() && error.isNull())
            result = m_engine->newObject();
        if (qEnvironmentVariableIsSet("KANZI_DEBUG"))
            qDebug().noquote() << "Kanzi API:" << code << reply->request().url().toString() << data.size() << "bytes";
        const QJSValue ret = callback.call({result, error});
        if (ret.isError())
            qWarning().noquote() << "Kanzi: error in API callback for" << reply->request().url().path() << "-"
                                 << ret.toString() << ret.property(QStringLiteral("stack")).toString();
    });
}

void AppleMusic::copyToClipboard(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

QString AppleMusic::artworkUrl(const QVariant &artwork, int size) const
{
    QString tpl;
    if (artwork.metaType().id() == QMetaType::QString)
        tpl = artwork.toString();
    else
        tpl = artwork.toMap().value(QStringLiteral("url")).toString();
    if (tpl.isEmpty())
        return {};
    const QString s = QString::number(qBound(32, size, 3000));
    tpl.replace(QStringLiteral("{w}"), s).replace(QStringLiteral("{h}"), s);
    tpl.replace(QStringLiteral("{c}"), QStringLiteral("cc")).replace(QStringLiteral("{f}"), QStringLiteral("jpg"));
    return tpl;
}

QVariantList AppleMusic::parseTtml(const QString &ttml) const
{
    QVariantList lines;
    QXmlStreamReader xml(ttml);
    int depth = -1; // depth inside the current <p>, -1 when outside
    qint64 begin = 0, end = 0;
    QString text;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (depth < 0 && xml.name() == u"p") {
                depth = 0;
                begin = parseClock(xml.attributes().value(u"begin"));
                end = parseClock(xml.attributes().value(u"end"));
                text.clear();
            } else if (depth >= 0) {
                ++depth;
                // Skip background vocals / translations nested in spans with roles.
                if (xml.attributes().value(u"ttm:role") == u"x-bg") {
                    xml.skipCurrentElement();
                    --depth;
                }
            }
        } else if (xml.isEndElement() && depth >= 0) {
            if (depth == 0) {
                lines.append(QVariantMap{{QStringLiteral("begin"), begin},
                                         {QStringLiteral("end"), end},
                                         {QStringLiteral("text"), text.simplified()}});
                depth = -1;
            } else {
                --depth;
            }
        } else if (xml.isCharacters() && depth >= 0) {
            text += xml.text();
        }
    }
    return lines;
}
