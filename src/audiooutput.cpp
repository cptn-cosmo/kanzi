#include "audiooutput.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QSettings>

#include <pulse/pulseaudio.h>

namespace {

const QString kSettingsKey = QStringLiteral("audio/outputDevice");

// Our pid plus every descendant: QtWebEngine may emit audio from a helper process.
QSet<qint64> ownProcessTree()
{
    QHash<qint64, QList<qint64>> children;
    const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &e : entries) {
        bool ok = false;
        const qint64 pid = e.toLongLong(&ok);
        if (!ok)
            continue;
        QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
        if (!stat.open(QIODevice::ReadOnly))
            continue;
        // Format: pid (comm) state ppid ... — comm may contain spaces, so parse after the last ')'.
        const QByteArray line = stat.readAll();
        const int close = line.lastIndexOf(')');
        if (close < 0)
            continue;
        const QList<QByteArray> fields = line.mid(close + 2).split(' ');
        if (fields.size() > 1)
            children[fields[1].toLongLong()].append(pid);
    }

    QSet<qint64> tree;
    QList<qint64> todo{QCoreApplication::applicationPid()};
    while (!todo.isEmpty()) {
        const qint64 pid = todo.takeLast();
        if (tree.contains(pid))
            continue;
        tree.insert(pid);
        todo += children.value(pid);
    }
    return tree;
}

void release(pa_operation *op)
{
    if (op)
        pa_operation_unref(op);
}

bool ownedByUs(const pa_sink_input_info *info)
{
    const char *pid = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_PROCESS_ID);
    return pid && ownProcessTree().contains(QByteArray(pid).toLongLong());
}

} // namespace

AudioOutput::AudioOutput(QObject *parent)
    : QObject(parent)
{
    if (s_instance)
        qFatal("Kanzi: %s constructed twice", "AudioOutput");
    s_instance = this;

    m_device = QSettings().value(kSettingsKey).toString();
    m_target = m_device.toUtf8();

    m_loop = pa_threaded_mainloop_new();
    if (!m_loop)
        return;
    pa_proplist *props = pa_proplist_new();
    pa_proplist_sets(props, PA_PROP_APPLICATION_NAME, "Kanzi");
    pa_proplist_sets(props, PA_PROP_APPLICATION_ID, "dev.kanzi.Kanzi");
    pa_proplist_sets(props, PA_PROP_APPLICATION_ICON_NAME, "dev.kanzi.Kanzi");
    m_ctx = pa_context_new_with_proplist(pa_threaded_mainloop_get_api(m_loop), "Kanzi device control", props);
    pa_proplist_free(props);

    pa_context_set_state_callback(m_ctx, &AudioOutput::onContextState, this);
    pa_context_set_subscribe_callback(m_ctx, reinterpret_cast<pa_context_subscribe_cb_t>(&AudioOutput::onSubscribe), this);
    if (pa_context_connect(m_ctx, nullptr, PA_CONTEXT_NOFAIL, nullptr) < 0 || pa_threaded_mainloop_start(m_loop) < 0) {
        qWarning("Kanzi: couldn't connect to the sound server; output selection disabled");
        pa_context_unref(m_ctx);
        m_ctx = nullptr;
    }
}

AudioOutput::~AudioOutput()
{
    if (m_loop) {
        pa_threaded_mainloop_stop(m_loop);
        if (m_ctx) {
            pa_context_disconnect(m_ctx);
            pa_context_unref(m_ctx);
        }
        pa_threaded_mainloop_free(m_loop);
    }
    s_instance = nullptr;
}

AudioOutput *AudioOutput::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void AudioOutput::setDevice(const QString &sinkName)
{
    if (sinkName == m_device)
        return;
    m_device = sinkName;
    QSettings().setValue(kSettingsKey, sinkName);
    emit deviceChanged();

    if (!m_ctx)
        return;
    pa_threaded_mainloop_lock(m_loop);
    m_target = sinkName.toUtf8();
    routeAllInputs();
    pa_threaded_mainloop_unlock(m_loop);
}

// ---- pulse thread ----

void AudioOutput::onContextState(pa_context *c, void *userdata)
{
    auto *self = static_cast<AudioOutput *>(userdata);
    const pa_context_state_t state = pa_context_get_state(c);
    const bool ready = state == PA_CONTEXT_READY;

    if (ready) {
        release(pa_context_subscribe(
            c, pa_subscription_mask_t(PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_SERVER),
            nullptr, nullptr));
        release(pa_context_get_server_info(c, &AudioOutput::onServerInfo, self));
        self->requestSinks();
    }
    if (ready || state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
        QMetaObject::invokeMethod(self, [self, ready] {
            if (self->m_available != ready) {
                self->m_available = ready;
                emit self->availableChanged();
            }
        }, Qt::QueuedConnection);
    }
}

void AudioOutput::onSubscribe(pa_context *c, int type, uint32_t index, void *userdata)
{
    auto *self = static_cast<AudioOutput *>(userdata);
    const int facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    const int kind = type & PA_SUBSCRIPTION_EVENT_TYPE_MASK;

    if (facility == PA_SUBSCRIPTION_EVENT_SINK) {
        self->requestSinks();
    } else if (facility == PA_SUBSCRIPTION_EVENT_SERVER) {
        release(pa_context_get_server_info(c, &AudioOutput::onServerInfo, self));
    } else if (facility == PA_SUBSCRIPTION_EVENT_SINK_INPUT && kind == PA_SUBSCRIPTION_EVENT_NEW) {
        // Only NEW: reacting to CHANGE would loop, since moving a stream changes it.
        if (!self->m_target.isEmpty())
            release(pa_context_get_sink_input_info(c, index, &AudioOutput::onSinkInputInfo, self));
    }
}

void AudioOutput::onServerInfo(pa_context *, const pa_server_info *info, void *userdata)
{
    auto *self = static_cast<AudioOutput *>(userdata);
    if (info && info->default_sink_name)
        self->m_defaultSink = info->default_sink_name;
}

void AudioOutput::requestSinks()
{
    m_pendingSinks.clear();
    release(pa_context_get_sink_info_list(m_ctx, &AudioOutput::onSinkInfo, this));
}

void AudioOutput::onSinkInfo(pa_context *, const pa_sink_info *info, int eol, void *userdata)
{
    auto *self = static_cast<AudioOutput *>(userdata);
    if (eol) {
        QMetaObject::invokeMethod(self, [self, sinks = self->m_pendingSinks] {
            if (self->m_devices != sinks) {
                self->m_devices = sinks;
                emit self->devicesChanged();
            }
        }, Qt::QueuedConnection);
        return;
    }
    if (!info)
        return;
    self->m_pendingSinks.append(QVariantMap{
        {QStringLiteral("id"), QString::fromUtf8(info->name)},
        {QStringLiteral("name"), QString::fromUtf8(info->description ? info->description : info->name)},
    });
}

void AudioOutput::routeAllInputs()
{
    release(pa_context_get_sink_input_info_list(m_ctx, &AudioOutput::onSinkInputInfo, this));
}

void AudioOutput::onSinkInputInfo(pa_context *c, const pa_sink_input_info *info, int eol, void *userdata)
{
    auto *self = static_cast<AudioOutput *>(userdata);
    if (eol || !info || !ownedByUs(info))
        return;
    const QByteArray sink = self->m_target.isEmpty() ? self->m_defaultSink : self->m_target;
    if (sink.isEmpty())
        return;
    release(pa_context_move_sink_input_by_name(c, info->index, sink.constData(), nullptr, nullptr));
}
