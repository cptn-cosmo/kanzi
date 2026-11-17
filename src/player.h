#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

class AppleMusic;
class Plex;

// Playback state mirrored from the MusicKit JS instance that runs inside a hidden
// WebEngineView (see PlayerHost.qml / player.html). That page is the only place
// DRM'd audio can be decoded; everything else in the app is native.
//
// QML talks to `Player`; the page talks to the same object over a WebChannel,
// receiving work through the `command` signal and reporting back via the js* slots.
class Player : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(bool buffering READ buffering NOTIFY stateChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY positionChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY modesChanged)
    Q_PROPERTY(RepeatMode repeatMode READ repeatMode WRITE setRepeatMode NOTIFY modesChanged)
    Q_PROPERTY(QVariantMap nowPlaying READ nowPlaying NOTIFY nowPlayingChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY nowPlayingChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int queuePosition READ queuePosition NOTIFY queueChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY queueChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY queueChanged)
    // Fade length between songs in seconds (0 = off).
    Q_PROPERTY(int crossfade READ crossfade WRITE setCrossfade NOTIFY transitionsChanged)
    // End songs early when they fade into silence, with adaptive fades.
    Q_PROPERTY(bool automix READ automix WRITE setAutomix NOTIFY transitionsChanged)
    // Which service plays and browses music: Apple Music (MusicKit page) or Plex (plex.html).
    Q_PROPERTY(Source source READ source WRITE setSource NOTIFY sourceChanged)

public:
    // Mirrors MusicKit.PlaybackStates.
    enum State {
        None = 0, Loading = 1, Playing = 2, Paused = 3, Stopped = 4, Ended = 5,
        Seeking = 6, Waiting = 8, Stalled = 9, Completed = 10
    };
    Q_ENUM(State)
    enum RepeatMode { RepeatNone = 0, RepeatOne = 1, RepeatAll = 2 };
    Q_ENUM(RepeatMode)
    enum Source { AppleMusicSource = 0, PlexSource = 1 };
    Q_ENUM(Source)

    explicit Player(AppleMusic *api, QObject *parent = nullptr);
    ~Player() override;

    static Player *create(QQmlEngine *, QJSEngine *);
    void connectPlex(Plex *plex);
    static Player *instance() { return s_instance; }

    bool ready() const { return m_ready; }
    QString errorString() const { return m_error; }
    State state() const { return m_state; }
    bool playing() const { return m_state == Playing; }
    bool buffering() const { return m_state == Loading || m_state == Waiting || m_state == Stalled || m_state == Seeking; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    double volume() const { return m_volume; }
    bool shuffle() const { return m_shuffle; }
    RepeatMode repeatMode() const { return m_repeat; }
    QVariantMap nowPlaying() const { return m_nowPlaying; }
    bool hasTrack() const { return !m_nowPlaying.isEmpty(); }
    QVariantList queue() const { return m_queue; }
    int queuePosition() const { return m_queuePosition; }
    bool canGoNext() const { return m_queuePosition + 1 < m_queue.size() || m_repeat == RepeatAll; }
    bool canGoPrevious() const { return hasTrack(); }

    void setVolume(double v);
    int crossfade() const { return m_crossfade; }
    bool automix() const { return m_automix; }
    void setCrossfade(int seconds);
    void setAutomix(bool on);
    void setShuffle(bool on);
    void setRepeatMode(RepeatMode mode);
    Source source() const { return m_source; }
    void setSource(Source source);

    // The HTML document to load into the hidden player view (MusicKit or Plex host),
    // the origin it's loaded under, and whether there's enough to load it yet.
    Q_INVOKABLE QString playerHtml() const;
    Q_INVOKABLE QString playerBaseUrl() const;
    Q_INVOKABLE bool canLoad() const;

    // type: "album" | "playlist" | "song" | "songs" | "station"; `id` is a single id or a list of ids.
    Q_INVOKABLE void playCollection(const QString &type, const QVariant &id, int startIndex = 0, bool shuffle = false);
    Q_INVOKABLE void playNext(const QString &type, const QString &id);
    Q_INVOKABLE void playLater(const QString &type, const QString &id);
    Q_INVOKABLE void jumpTo(int queueIndex);
    // Plex: play / queue ready-made items ({ id, title, …, src }); see Plex::queueItem.
    void playTracks(const QVariantList &items, int startIndex, bool shuffle);
    void queueTracks(const QVariantList &items, bool next);

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void cycleRepeat();

    Q_INVOKABLE void authorize();
    Q_INVOKABLE void requestRaise() { emit raiseRequested(); }

    // ---- called from the page over the WebChannel ----
public slots:
    void jsReady();
    void jsError(const QString &message);
    void jsLog(const QString &message);
    void jsState(int state);
    void jsTime(double position, double duration);
    void jsVolume(double volume);
    void jsModes(bool shuffle, int repeat);
    void jsNowPlaying(const QVariantMap &item);
    void jsQueue(const QVariantList &items, int position);
    void jsUserToken(const QString &token);

signals:
    // Delivered to the page; never connect to this from QML.
    void command(const QString &name, const QVariantMap &args);

    void readyChanged();
    void errorStringChanged();
    void stateChanged();
    void positionChanged();
    void volumeChanged();
    void modesChanged();
    void nowPlayingChanged();
    void queueChanged();
    void transitionsChanged();
    void sourceChanged();
    void seeked(double position);
    void raiseRequested();
    void reloadRequested();

private:
    void send(const QString &name, const QVariantMap &args = {});
    void sendTransitions();
    QVariantMap transitionArgs() const;
    void resetState();

    static inline Player *s_instance = nullptr;

    AppleMusic *m_api;
    bool m_ready = false;
    QString m_error;
    State m_state = None;
    double m_position = 0;
    double m_duration = 0;
    double m_volume = 1.0;
    bool m_shuffle = false;
    RepeatMode m_repeat = RepeatNone;
    QVariantMap m_nowPlaying;
    QVariantList m_queue;
    int m_queuePosition = -1;
    int m_crossfade = 0;
    bool m_automix = false;
    Source m_source = AppleMusicSource;
};
