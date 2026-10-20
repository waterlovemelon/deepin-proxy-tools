// SPDX-License-Identifier: LGPL-3.0-or-later
// dccData service facade. The dde-mihomo-daemon runs as a systemd --user unit,
// so this class owns the unit's lifecycle through systemctl and forwards every
// configuration command to the daemon over its local socket.
#include "plugindata.h"
#include "tun.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QProcess>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QTimer>
#include <sys/stat.h>
#include <unistd.h>

namespace Party {
namespace {

const QString kUnitName = QStringLiteral("dde-mihomo-daemon.service");
const QString kBundledCore = QStringLiteral("/usr/lib/dcc-proxy/mihomo");

// The daemon binds its socket just after systemd reports the unit active, so the
// first command is retried until the socket answers.
constexpr int kReadyAttempts = 20;
constexpr int kReadyIntervalMs = 250;
constexpr int kSystemctlTimeoutMs = 30000;

// DCC_PROXY_DATA_DIR is the documented development/test isolation switch used by
// tools/ and tests/*.py: it relocates the data directory (see dataDir()) and
// makes this facade spawn the daemon itself instead of going through systemd.
bool isolatedDataDir() { return !qEnvironmentVariable("DCC_PROXY_DATA_DIR").isEmpty(); }

// Installed by the package under /usr/lib/systemd/user, or shipped by hand in the
// user's own unit directory.
QString unitFile()
{
    const QStringList candidates{
        QStringLiteral("/usr/lib/systemd/user/dde-mihomo-daemon.service"),
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/systemd/user/dde-mihomo-daemon.service")};
    for (const auto &path : candidates)
        if (QFileInfo(path).isFile()) return path;
    return QString();
}

// Value of one `systemctl --user show -p <name>` line (empty when unset).
QString propertyValue(const QString &output, const QString &name)
{
    for (const auto &line : output.split(QLatin1Char('\n')))
        if (line.startsWith(name + QLatin1Char('='))) return line.mid(name.size() + 1);
    return QString();
}

// `systemctl --user <arguments>`. The merged output reaches `done` so callers can
// parse `show` answers and quote failures verbatim.
void systemctlAsync(QObject *context, const QStringList &arguments, std::function<void(bool, const QString &)> done)
{
    auto *process = new QProcess(context);
    process->setProcessChannelMode(QProcess::MergedChannels);
    const auto finished = QSharedPointer<bool>::create(false);
    auto complete = [process, finished, done](bool ok) {
        if (*finished) return;
        *finished = true;
        const auto output = QString::fromUtf8(process->readAll()).trimmed();
        process->deleteLater();
        done(ok, output);
    };
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), process,
        [complete](int code, QProcess::ExitStatus status) { complete(code == 0 && status == QProcess::NormalExit); });
    QObject::connect(process, &QProcess::errorOccurred, process,
        [complete](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) complete(false); });
    QTimer::singleShot(kSystemctlTimeoutMs, process, [process, complete] {
        if (process->state() == QProcess::NotRunning) return;
        process->kill();
        complete(false);
    });
    process->start(QStringLiteral("systemctl"), QStringList{QStringLiteral("--user")} + arguments);
}

// Sends `command` as soon as the daemon socket answers.
void sendWhenReady(Service *service, const QJsonObject &command, int attempts, Api::Done done)
{
    serviceRequest(QJsonObject{{"action", "status"}}, service, [=](const QJsonValue &state, const QString &error) {
        if (error.isEmpty() || state.toObject().contains(QStringLiteral("running"))) {
            service->request(command, done);
            return;
        }
        if (attempts <= 1) { done(QJsonValue(), Service::tr("后台服务未在预期时间内就绪")); return; }
        QTimer::singleShot(kReadyIntervalMs, service, [=] { sendWhenReady(service, command, attempts - 1, done); });
    });
}

// Hands `command` to the daemon and folds its answer into `result(ok, text)`.
void forwardToDaemon(Service *service, const QJsonObject &command, const QString &fallback,
                     const std::function<void(bool, const QString &)> &result)
{
    sendWhenReady(service, command, kReadyAttempts, [fallback, result](const QJsonValue &value, const QString &error) {
        if (!error.isEmpty()) { result(false, error); return; }
        result(true, value.toObject()[QStringLiteral("message")].toString(fallback));
    });
}

// `systemctl --user <verb> dde-mihomo-daemon.service`, then the daemon command.
void startUnit(Service *service, const QString &verb, const QJsonObject &command,
               const QString &fallback, const std::function<void(bool, const QString &)> &result)
{
    systemctlAsync(service, QStringList{verb, kUnitName}, [=](bool ok, const QString &output) {
        if (!ok) { result(false, output.isEmpty() ? Service::tr("无法通过 systemd 控制后台服务") : output); return; }
        forwardToDaemon(service, command, fallback, result);
    });
}

// Shared start()/restart() body: make sure systemd knows the current unit file,
// run the systemd verb, then let the daemon run `command`.
void launchDaemon(Service *service, const QString &verb, const QJsonObject &command,
                  const QString &fallback, const std::function<void(bool, const QString &)> &result)
{
    if (isolatedDataDir()) {
        // Isolation mode owns the process itself, mirroring the previous
        // implementation: only launch the binary when the daemon is not up yet.
        serviceRequest(QJsonObject{{"action", "status"}}, service, [=](const QJsonValue &state, const QString &error) {
            if (error.isEmpty() || state.toObject().contains(QStringLiteral("running"))) {
                forwardToDaemon(service, command, fallback, result);
                return;
            }
            if (!QProcess::startDetached(serviceExecutable(), QStringList{QStringLiteral("--daemon")}, dataDir())) {
                result(false, Service::tr("无法启动 dde-mihomo-daemon"));
                return;
            }
            forwardToDaemon(service, command, fallback, result);
        });
        return;
    }
    if (unitFile().isEmpty()) {
        result(false, Service::tr("未找到 dde-mihomo-daemon.service 用户服务，请重新安装 dcc-proxy"));
        return;
    }
    // The user manager only starts units it has read: a freshly installed or
    // updated unit file needs `daemon-reload` first. UnitFileState stays empty
    // while the manager does not know the unit at all, and NeedDaemonReload
    // tells whether any unit file changed on disk since the last reload.
    systemctlAsync(service, QStringList{QStringLiteral("show"), QStringLiteral("-p"), QStringLiteral("UnitFileState"),
                                        QStringLiteral("-p"), QStringLiteral("NeedDaemonReload"), kUnitName},
        [=](bool ok, const QString &output) {
            const bool stale = propertyValue(output, QStringLiteral("UnitFileState")).isEmpty() ||
                               propertyValue(output, QStringLiteral("NeedDaemonReload")) == QStringLiteral("yes");
            if (!ok || !stale) {
                startUnit(service, verb, command, fallback, result);
                return;
            }
            systemctlAsync(service, QStringList{QStringLiteral("daemon-reload")},
                [=](bool, const QString &) { startUnit(service, verb, command, fallback, result); });
        });
}

} // namespace

Service::Service(QObject *parent) : QObject(parent)
{
    // The factory builds dccData on a worker thread and moves it to the main
    // thread, so the first probe must be queued instead of run here.
    QTimer::singleShot(0, this, [this] { refresh(); });
}

void Service::refresh()
{
    if (isolatedDataDir()) {
        serviceRequest(QJsonObject{{"action", "status"}}, this, [this](const QJsonValue &value, const QString &) {
            const auto state = value.toObject();
            if (state.contains(QStringLiteral("running"))) { applyStatus(state); return; }
            // The isolated daemon is not up, or is still starting.
            applyStatus(QJsonObject{{"reachable", false}, {"running", false}});
        });
        return;
    }
    const auto unit = unitFile();
    applyStatus(QJsonObject{{"unitAvailable", !unit.isEmpty()}});
    if (unit.isEmpty()) {
        // Without the unit file the daemon cannot be started through systemd.
        applyStatus(QJsonObject{{"unitEnabled", false}, {"reachable", false}, {"running", false}});
        return;
    }
    runSystemctl({QStringLiteral("show"), QStringLiteral("-p"), QStringLiteral("ActiveState"), QStringLiteral("-p"),
                  QStringLiteral("SubState"), QStringLiteral("-p"), QStringLiteral("MainPID"), QStringLiteral("-p"),
                  QStringLiteral("UnitFileState"), kUnitName},
        [this](bool ok, const QString &output) {
            if (!ok) {
                // systemctl --user cannot reach the user manager: no session bus.
                const auto text = tr("无法连接用户会话总线（systemctl --user 不可用），请在桌面会话中运行控制中心");
                const bool first = m_error != text;
                applyStatus(QJsonObject{{"running", false}, {"reachable", false}, {"error", text}});
                if (first) emit message(text, true);
                return;
            }
            applyStatus(QJsonObject{{"unitEnabled", propertyValue(output, QStringLiteral("UnitFileState")) == QStringLiteral("enabled")}});
            // "active" without a live main process (or a dead/failed sub state)
            // means the daemon is gone even if systemd has not settled yet.
            const auto active = propertyValue(output, QStringLiteral("ActiveState"));
            const auto sub = propertyValue(output, QStringLiteral("SubState"));
            const bool starting = active == QStringLiteral("activating") || active == QStringLiteral("reloading");
            const bool up = starting || (active == QStringLiteral("active") &&
                                         propertyValue(output, QStringLiteral("MainPID")).toInt() > 0 &&
                                         sub != QStringLiteral("dead") && sub != QStringLiteral("failed"));
            if (!up) {
                applyStatus(QJsonObject{{"reachable", false}, {"running", false}});
                return;
            }
            // The unit is up: the daemon answer is the authoritative state.
            serviceRequest(QJsonObject{{"action", "status"}}, this, [this](const QJsonValue &value, const QString &) {
                const auto state = value.toObject();
                if (state.contains(QStringLiteral("running"))) { applyStatus(state); return; }
                applyStatus(QJsonObject{{"reachable", false}, {"running", false}});
            });
        });
}

void Service::start(const QString &profile)
{
    if (m_busy) { emit message(tr("正在执行上一个代理操作，请稍候"), true); return; }
    m_busy = true; emit stateChanged();
    launchDaemon(this, QStringLiteral("start"), QJsonObject{{"action", "start"}, {"profile", profile}}, tr("内核已启动"),
        [this](bool ok, const QString &text) {
            m_busy = false; emit stateChanged();
            if (ok) { emit message(text, false); return; }
            // A failure already reported by request() is not repeated.
            const bool first = m_error != text;
            applyStatus(QJsonObject{{"running", false}, {"error", text}});
            if (first) emit message(text, true);
        });
}

void Service::restart(const QString &profile)
{
    if (m_busy) { emit message(tr("正在执行上一个代理操作，请稍候"), true); return; }
    m_busy = true; emit stateChanged();
    launchDaemon(this, QStringLiteral("restart"), QJsonObject{{"action", "restart"}, {"profile", profile}}, tr("内核已重新启动"),
        [this](bool ok, const QString &text) {
            m_busy = false; emit stateChanged();
            if (ok) { emit message(text, false); return; }
            const bool first = m_error != text;
            applyStatus(QJsonObject{{"running", false}, {"error", text}});
            if (first) emit message(text, true);
        });
}

void Service::stop()
{
    if (m_busy) { emit message(tr("正在执行上一个代理操作，请稍候"), true); return; }
    m_busy = true; emit stateChanged();
    if (isolatedDataDir()) {
        // Isolation mode owns the process itself, so stopping means stopping the
        // kernel; the daemon stays available exactly like before.
        request(QJsonObject{{"action", "stop"}}, [this](const QJsonValue &value, const QString &error) {
            m_busy = false; emit stateChanged();
            if (!error.isEmpty()) return; // request() already reported it
            emit message(value.toObject()[QStringLiteral("message")].toString(tr("代理服务已停止")), false);
        });
        return;
    }
    if (unitFile().isEmpty()) {
        m_busy = false; emit stateChanged();
        const auto text = tr("未找到 dde-mihomo-daemon.service 用户服务，请重新安装 dcc-proxy");
        applyStatus(QJsonObject{{"error", text}});
        emit message(text, true);
        return;
    }
    // systemd stops the daemon; its SIGTERM path stops the kernel and restores the
    // system proxy before exiting.
    runSystemctl({QStringLiteral("stop"), kUnitName}, [this](bool ok, const QString &output) {
        m_busy = false; emit stateChanged();
        if (!ok) {
            const auto text = output.isEmpty() ? tr("无法通过 systemd 停止后台服务") : output;
            applyStatus(QJsonObject{{"error", text}});
            emit message(text, true);
            return;
        }
        applyStatus(QJsonObject{{"running", false}, {"reachable", false}});
        emit message(tr("代理服务已停止"), false);
    });
}

void Service::setEnabled(bool enabled)
{
    if (isolatedDataDir()) {
        // There is no unit to enable while DCC_PROXY_DATA_DIR isolates the daemon.
        applyStatus(QJsonObject{{"unitEnabled", enabled}});
        return;
    }
    if (unitFile().isEmpty()) {
        const auto text = tr("未找到 dde-mihomo-daemon.service 用户服务，请重新安装 dcc-proxy");
        applyStatus(QJsonObject{{"error", text}});
        emit message(text, true);
        return;
    }
    runSystemctl({enabled ? QStringLiteral("enable") : QStringLiteral("disable"), kUnitName},
        [this, enabled](bool ok, const QString &output) {
            if (!ok) {
                const auto text = output.isEmpty() ? tr("无法修改后台服务的自启动状态") : output;
                applyStatus(QJsonObject{{"error", text}});
                emit message(text, true);
                return;
            }
            applyStatus(QJsonObject{{"unitEnabled", enabled}, {"error", QString()}});
        });
}

void Service::setTun(const QVariantMap &options)
{
    QJsonObject validated;
    try { validated = validateTunOptions(QJsonObject::fromVariantMap(options)); }
    catch (const std::exception &e) { emit message(QString::fromUtf8(e.what()), true); return; }
    request(QJsonObject{{"action", "tun"}, {"options", validated}},
        [this](const QJsonValue &value, const QString &error) {
            const auto state = value.toObject();
            if (state[QStringLiteral("needsAuthorization")].toBool()) {
                // The daemon refused the switch; show what the user has to do.
                m_error = tr("启用虚拟网卡需要管理员授权内核，请完成系统认证。");
                emit stateChanged();
                return;
            }
            if (!error.isEmpty()) return; // request() already reported it
            emit message(state[QStringLiteral("message")].toString(tr("虚拟网卡设置已保存")), false);
            // A saved-but-not-running answer carries no status, so re-read it to
            // keep every switch fresh after apply.
            if (!state.contains(QStringLiteral("running"))) refresh();
        });
}

void Service::repairTun()
{
    request(QJsonObject{{"action", "repair-tun"}}, [this](const QJsonValue &value, const QString &error) {
        if (!error.isEmpty()) return; // request() already reported it
        emit message(value.toObject()[QStringLiteral("message")].toString(tr("虚拟网卡、路由与 DNS 配置已重新加载")), false);
    });
}

void Service::setPorts(const QVariantMap &ports)
{
    QJsonObject validated;
    try { validated = validateProxyPorts(QJsonObject::fromVariantMap(ports)); }
    catch (const std::exception &e) { emit message(QString::fromUtf8(e.what()), true); return; }
    request(QJsonObject{{"action", "ports"}, {"options", validated}},
        [this](const QJsonValue &value, const QString &error) {
            if (!error.isEmpty()) return; // request() already reported it
            emit message(value.toObject()[QStringLiteral("message")].toString(tr("代理端口已保存")), false);
            refresh();
        });
}

void Service::setBypass(const QStringList &entries)
{
    QStringList normalized;
    try { normalized = normalizeProxyBypass(entries); }
    catch (const std::exception &e) { emit message(QString::fromUtf8(e.what()), true); return; }
    request(QJsonObject{{"action", "bypass"}, {"entries", QJsonArray::fromStringList(normalized)}},
        [this](const QJsonValue &value, const QString &error) {
            if (!error.isEmpty()) return; // request() already reported it
            emit message(value.toObject()[QStringLiteral("message")].toString(tr("代理绕过已保存")), false);
        });
}

void Service::setSystemProxy(const QString &mode, int port)
{
    request(QJsonObject{{"action", "proxy"}, {"mode", mode}, {"port", port}},
        [this, mode](const QJsonValue &value, const QString &error) {
            if (!error.isEmpty()) return; // request() already reported it
            const auto fallback = mode == QStringLiteral("none") ? tr("已恢复原系统代理设置") : tr("系统代理已启用");
            emit message(value.toObject()[QStringLiteral("message")].toString(fallback), false);
        });
}

void Service::authorizeCore()
{
    // The package ships the core as root:root mode 4755 (see debian/postinst).
    // Re-applying that recipe directly needs root; otherwise the desktop
    // authorization prompt performs the same chown/chmod.
    const auto prompted = [this](QString error) {
        if (!error.isEmpty()) { emit message(error, true); return; }
        emit message(tr("内核授权成功，可启用虚拟网卡"), false);
        refresh();
    };
    // The postinst never touches a symlinked core; the same applies here so the
    // setuid bit cannot be handed to an arbitrary target.
    const QFileInfo core(kBundledCore);
    const bool bundled = core.isFile() && !core.isSymLink();
    if (bundled && hasTunPermission(kBundledCore)) {
        emit message(tr("内核文件已授权；首次启用时会自动重启未获得权限的内核进程"), false);
        return;
    }
    if (bundled) {
        const auto path = QFile::encodeName(kBundledCore);
        if (::chown(path.constData(), 0, 0) == 0 && ::chmod(path.constData(), 04755) == 0 && hasTunPermission(kBundledCore)) {
            prompted(QString());
            return;
        }
    }
    requestTunPermission(kBundledCore, this, prompted);
}

void Service::request(const QJsonObject &command, std::function<void(const QJsonValue &, const QString &)> done)
{
    // Raw channel for the controllers: it folds the daemon answer into the
    // exported state and reports failures, so callers own success messaging only.
    const bool operation = command[QStringLiteral("action")].toString() != QStringLiteral("status");
    const bool owned = operation && !m_busy;
    if (owned) { m_busy = true; emit stateChanged(); }
    serviceRequest(command, this, [this, done, owned, operation](const QJsonValue &value, const QString &error) {
        const auto state = value.toObject();
        // Transport failure: the daemon never answered.
        if (state.isEmpty()) {
            if (!error.isEmpty()) applyStatus(QJsonObject{{"error", error}});
        } else applyStatus(state);
        if (owned) { m_busy = false; emit stateChanged(); }
        if (operation && !error.isEmpty()) emit message(error, true);
        if (done) done(value, error);
    });
}

void Service::applyStatus(const QJsonObject &state)
{
    // Single place where daemon answers and local lifecycle results become
    // exported properties; user messaging stays with the callers.
    bool changed = false;
    const auto fold = [&changed](auto &field, const auto &value) {
        if (field == value) return;
        field = value; changed = true;
    };
    if (state.contains(QStringLiteral("reachable"))) fold(m_reachable, state[QStringLiteral("reachable")].toBool());
    else if (state.contains(QStringLiteral("running"))) fold(m_reachable, true); // the daemon answered
    if (state.contains(QStringLiteral("running"))) fold(m_running, state[QStringLiteral("running")].toBool());
    if (state.contains(QStringLiteral("root"))) fold(m_root, state[QStringLiteral("root")].toBool());
    if (state.contains(QStringLiteral("profile"))) fold(m_profile, state[QStringLiteral("profile")].toString());
    if (state.contains(QStringLiteral("unitAvailable"))) fold(m_unitAvailable, state[QStringLiteral("unitAvailable")].toBool());
    if (state.contains(QStringLiteral("unitEnabled"))) fold(m_unitEnabled, state[QStringLiteral("unitEnabled")].toBool());
    bool tunChangedNow = false;
    if (state.contains(QStringLiteral("tun"))) {
        const auto tun = state[QStringLiteral("tun")].toObject();
        const bool enabled = m_tunEnabled, authorized = m_tunAuthorized, processAuthorized = m_tunProcessAuthorized;
        const auto device = m_tunDevice;
        if (tun.contains(QStringLiteral("enable"))) fold(m_tunEnabled, tun[QStringLiteral("enable")].toBool());
        if (tun.contains(QStringLiteral("device"))) fold(m_tunDevice, tun[QStringLiteral("device")].toString());
        if (tun.contains(QStringLiteral("permission"))) fold(m_tunAuthorized, tun[QStringLiteral("permission")].toBool());
        if (tun.contains(QStringLiteral("processPermission"))) fold(m_tunProcessAuthorized, tun[QStringLiteral("processPermission")].toBool());
        tunChangedNow = m_tunEnabled != enabled || m_tunDevice != device || m_tunAuthorized != authorized ||
                        m_tunProcessAuthorized != processAuthorized;
    }
    if (state.contains(QStringLiteral("error"))) fold(m_error, state[QStringLiteral("error")].toString());
    if (changed) emit stateChanged();
    if (tunChangedNow) emit tunChanged();
}

void Service::runSystemctl(const QStringList &arguments, std::function<void(bool, const QString &)> done)
{
    systemctlAsync(this, arguments, std::move(done));
}
} // namespace Party
