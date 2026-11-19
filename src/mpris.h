#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>

class QNetworkReply;

class Player;

// MPRIS2 integration so desktop media controls, lock screens and media keys work.
class Mpris : public QObject
{
    Q_OBJECT
public:
    explicit Mpris(Player *player, QObject *parent = nullptr);

    Player *player() const { return m_player; }
    void notifyChanged(const QString &interface, const QVariantMap &changed);

    // Local copy of the current track's cover (file://), empty until it is cached. Remote art
    // URLs are never published: shells may not fetch them, and Plex ones carry the account token.
    QUrl artUrl() const { return m_artLocal; }

private:
    void updateArt();
    void pruneArtCache();

    Player *m_player;
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_artReply;
    QString m_artRemote;
    QUrl m_artLocal;
};

class MprisRoot : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit MprisRoot(Mpris *parent);

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("Kanzi"); }
    QString desktopEntry() const { return QStringLiteral("dev.kanzi.Kanzi"); }
    QStringList supportedUriSchemes() const { return {}; }
    QStringList supportedMimeTypes() const { return {}; }

public slots:
    void Raise();
    void Quit();

private:
    Mpris *m_mpris;
};

class MprisPlayer : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ rate)
    Q_PROPERTY(double MaximumRate READ rate)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPlay)
    Q_PROPERTY(bool CanSeek READ canPlay)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    explicit MprisPlayer(Mpris *parent);

    QString playbackStatus() const;
    QString loopStatus() const;
    void setLoopStatus(const QString &s);
    double rate() const { return 1.0; }
    void setRate(double) {}
    bool shuffle() const;
    void setShuffle(bool on);
    QVariantMap metadata() const;
    double volume() const;
    void setVolume(double v);
    qlonglong position() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPlay() const;
    bool canControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &trackId, qlonglong position);
    void OpenUri(const QString &) {}

signals:
    void Seeked(qlonglong position);

private:
    QDBusObjectPath trackId() const;

    Mpris *m_mpris;
    Player *m_player;
};
