#pragma once

#include <QMutex>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

struct pa_threaded_mainloop;
struct pa_context;
struct pa_sink_info;
struct pa_sink_input_info;
struct pa_server_info;

// Lets the user pick which audio device Kanzi plays through.
//
// Audio is produced by Chromium inside the hidden player view, so instead of choosing a
// device in-page we talk to the sound server (PulseAudio or PipeWire's pulse server):
// list its sinks, and move every playback stream owned by this process tree to the
// chosen sink — including streams created later, e.g. when a new track starts.
class AudioOutput : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    // [{ id, name }] where id is the sink name; does not include the "system default" entry.
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    // Selected sink name, or "" to follow the system default.
    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY deviceChanged)

public:
    // Deliberately not default-constructible: QML prefers a default constructor over
    // create(), and would otherwise silently build a second, disconnected instance.
    explicit AudioOutput(QObject *parent);
    ~AudioOutput() override;

    static AudioOutput *create(QQmlEngine *, QJSEngine *);

    bool available() const { return m_available; }
    QVariantList devices() const { return m_devices; }
    QString device() const { return m_device; }
    void setDevice(const QString &sinkName);

signals:
    void availableChanged();
    void devicesChanged();
    void deviceChanged();

private:
    // All of these run on the pulse mainloop thread.
    static void onContextState(pa_context *c, void *self);
    static void onSubscribe(pa_context *c, int type, uint32_t index, void *self);
    static void onSinkInfo(pa_context *c, const pa_sink_info *info, int eol, void *self);
    static void onSinkInputInfo(pa_context *c, const pa_sink_input_info *info, int eol, void *self);
    static void onServerInfo(pa_context *c, const pa_server_info *info, void *self);
    void requestSinks();
    void routeAllInputs();

    static inline AudioOutput *s_instance = nullptr;

    pa_threaded_mainloop *m_loop = nullptr;
    pa_context *m_ctx = nullptr;

    // Qt (GUI) thread state.
    bool m_available = false;
    QVariantList m_devices;
    QString m_device;

    // Pulse thread state; touched only with the mainloop lock held.
    QVariantList m_pendingSinks;
    QByteArray m_target;        // chosen sink, empty = default
    QByteArray m_defaultSink;   // server's current default sink
};
