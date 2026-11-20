#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

class Mpris;
class Player;

// Desktop notifications (org.freedesktop.Notifications) when the playing track changes.
class Notifications : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    // Also notify while a Kanzi window has focus (the track is on screen anyway).
    Q_PROPERTY(bool whenFocused READ whenFocused WRITE setWhenFocused NOTIFY settingsChanged)

public:
    // Not default-constructible on purpose; see AppleMusic.
    Notifications(Player *player, Mpris *mpris, QObject *parent);
    ~Notifications() override;

    static Notifications *create(QQmlEngine *, QJSEngine *);

    bool available() const { return m_available; }
    bool enabled() const { return m_enabled; }
    bool whenFocused() const { return m_whenFocused; }
    void setEnabled(bool on);
    void setWhenFocused(bool on);

signals:
    void availableChanged();
    void settingsChanged();

private slots:
    void onActionInvoked(uint id, const QString &action);
    void onClosed(uint id, uint reason);

private:
    void queryServer();
    void onTrackMaybeChanged();
    void show();

    static inline Notifications *s_instance = nullptr;

    Player *m_player;
    Mpris *m_mpris;
    QTimer m_artWait;
    QString m_pendingId;  // track waiting to be announced
    QString m_notifiedId; // last track announced (or skipped)
    uint m_notificationId = 0;
    QStringList m_caps;
    bool m_available = false;
    bool m_enabled = false;
    bool m_whenFocused = false;
};
