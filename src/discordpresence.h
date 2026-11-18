#pragma once

#include <QByteArray>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>

class Player;
class QLocalSocket;

// Discord Rich Presence ("Listening to …") over Discord's local IPC socket.
// Speaks the RPC protocol directly: frames are [opcode:u32le][length:u32le][json].
class DiscordPresence : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    // Application ID from https://discord.com/developers/applications; its name is
    // what Discord shows after "Listening to".
    Q_PROPERTY(bool showWhenPaused READ showWhenPaused WRITE setShowWhenPaused NOTIFY settingsChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    DiscordPresence(Player *player, QObject *parent);
    ~DiscordPresence() override;

    static DiscordPresence *create(QQmlEngine *, QJSEngine *);

    bool enabled() const { return m_enabled; }
    bool showWhenPaused() const { return m_showWhenPaused; }
    bool connected() const { return m_ready; }
    QString status() const { return m_status; }
    void setEnabled(bool on);
    void setShowWhenPaused(bool on);

signals:
    void settingsChanged();
    void statusChanged();

private:
    enum Opcode : quint32 { Handshake = 0, Frame = 1, Close = 2, Ping = 3, Pong = 4 };

    void reconnect();
    void tryConnect();
    void disconnectSocket(const QString &status);
    void send(Opcode op, const QJsonObject &payload);
    void onReadyRead();
    void handleFrame(quint32 op, const QJsonObject &msg);
    void scheduleUpdate();
    void pushActivity();
    void setStatus(const QString &status, bool ready);

    static inline DiscordPresence *s_instance = nullptr;

    Player *m_player;
    QLocalSocket *m_socket = nullptr;
    QByteArray m_buffer;
    QTimer m_retry;
    QTimer m_debounce;
    QStringList m_candidates;
    bool m_ready = false;
    bool m_enabled = true;
    bool m_showWhenPaused = false;
    QString m_clientId;
    QString m_status;
};
