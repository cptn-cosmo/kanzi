#pragma once

#include <QObject>
#include <QQmlEngine>

class Player;
class QAction;
class QMenu;
class QSystemTrayIcon;

// System tray icon (StatusNotifierItem on Wayland desktops) plus the
// close-to-tray / start-in-tray preferences.
class Tray : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    // Effective: closing the main window hides it instead of quitting.
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY settingsChanged)
    Q_PROPERTY(bool startInTray READ startInTray WRITE setStartInTray NOTIFY settingsChanged)
    Q_PROPERTY(bool active READ active NOTIFY settingsChanged)
    // Ask the compositor to keep the mini player above other windows.
    Q_PROPERTY(bool miniPlayerOnTop READ miniPlayerOnTop WRITE setMiniPlayerOnTop NOTIFY settingsChanged)
    // KWin is running, so keep-above can be applied through a KWin script (works on Wayland).
    Q_PROPERTY(bool kwin READ kwin CONSTANT)

public:
    // Not default-constructible on purpose; see AppleMusic.
    Tray(Player *player, QObject *parent);
    ~Tray() override;

    static Tray *create(QQmlEngine *, QJSEngine *);

    bool available() const;
    bool enabled() const { return m_enabled; }
    bool active() const { return available() && m_enabled; }
    bool closeToTray() const { return active() && m_closeToTray; }
    bool startInTray() const { return m_startInTray; }
    void setEnabled(bool on);
    void setCloseToTray(bool on);
    void setStartInTray(bool on);
    bool miniPlayerOnTop() const { return m_miniPlayerOnTop; }
    void setMiniPlayerOnTop(bool on);
    bool kwin() const { return m_kwin; }

    // Sets KWin's "keep above" on this process's window titled `caption`.
    // Wayland gives clients no way to do this themselves; KWin scripting can.
    Q_INVOKABLE void setKeepAbove(const QString &caption, bool on);

signals:
    void settingsChanged();
    void toggleWindowRequested();
    void showWindowRequested();
    void miniPlayerRequested();

private:
    void updateIcon();
    void updateNowPlaying();

    static inline Tray *s_instance = nullptr;

    Player *m_player;
    QSystemTrayIcon *m_icon = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_nowPlaying = nullptr;
    QAction *m_playPause = nullptr;
    QAction *m_next = nullptr;
    QAction *m_previous = nullptr;
    bool m_enabled = true;
    bool m_closeToTray = true;
    bool m_startInTray = false;
    bool m_miniPlayerOnTop = false;
    bool m_kwin = false;
    int m_scriptSerial = 0;
};
