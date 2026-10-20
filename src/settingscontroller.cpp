// SPDX-License-Identifier: LGPL-3.0-or-later
// Settings, overrides, virtual interface (TUN), tools and overview controllers.
// Ported from the Qt5/DTK5 widget Panel to the V25 QML facade: no QWidget is
// involved, every user-visible string is reported through the `message` signal.
#include "plugindata.h"
#include "tun.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <stdexcept>

namespace Party {
namespace {
// Byte counts, identical formatting to ProxyPlugin::formatSize().
QString sizeText(double bytes)
{
    const QStringList units{"B", "KiB", "MiB", "GiB", "TiB"};
    int index = 0;
    while (bytes >= 1024 && index < units.size() - 1) { bytes /= 1024; ++index; }
    return QString::number(bytes, 'f', index ? 1 : 0) + ' ' + units[index];
}

QStringList textEntries(const QString &text)
{
    return text.split(QRegularExpression("[\\n\\r,;，；]"), Qt::SkipEmptyParts);
}

bool setPort(QVariantMap &ports, const QString &key, int port)
{
    if (ports.value(key).toInt() == port) return false;
    ports[key] = port;
    return true;
}

// Runs `action`, reporting validation errors through the caller's message channel.
template <typename Controller, typename Action>
void guarded(Controller *controller, Action action)
{
    try { action(); }
    catch (const std::exception &e) { controller->message(QString::fromUtf8(e.what()), true); }
}

// One-shot controller client: always bound to the stored endpoint and secret,
// destroyed by its own completion. The plugin's own Api is reconfigured by
// ProxyPlugin::refresh(), so the controllers never hold a second one.
void coreRequest(QObject *owner, const QByteArray &method, const QString &path, const QJsonObject &body, Api::Done done)
{
    auto *api = new Api(owner);
    const auto settings = Store().settings();
    api->configure(settings.value("endpoint").toString(), settings.value("secret").toString());
    api->request(method, path, body, [api, done = std::move(done)](const QJsonValue &value, const QString &error) {
        api->deleteLater();
        done(value, error);
    });
}
} // namespace

// -------------------------------------------------------------- settings ----
SettingsController::SettingsController(Service *service, QObject *parent)
    : QObject(parent), m_service(service)
{
    for (const auto &key : proxyPortKeys()) m_ports[key] = key == "mixed-port" ? 16666 : 0;
    m_bypass = defaultProxyBypass().join('\n');
}

void SettingsController::setEndpoint(const QString &endpoint)
{
    if (m_endpoint == endpoint) return;
    m_endpoint = endpoint;
    emit changed();
}

void SettingsController::setSecret(const QString &secret)
{
    if (m_secret == secret) return;
    m_secret = secret;
    emit changed();
}

void SettingsController::setCorePath(const QString &path)
{
    if (m_corePath == path) return;
    m_corePath = path;
    emit changed();
}

void SettingsController::setUnifiedDelay(bool on)
{
    if (m_unifiedDelay == on) return;
    m_unifiedDelay = on;
    emit changed();
}

void SettingsController::setAutoSystemProxy(bool on)
{
    if (m_autoSystemProxy == on) return;
    m_autoSystemProxy = on;
    emit changed();
}

void SettingsController::setAutoStart(bool on)
{
    if (m_autoStart == on) return;
    m_autoStart = on;
    emit changed();
}

void SettingsController::setUpdateMinutes(int minutes)
{
    const int bounded = qBound(0, minutes, 10080);
    if (m_updateMinutes == bounded) return;
    m_updateMinutes = bounded;
    emit changed();
}

void SettingsController::setMixedPort(int port) { if (setPort(m_ports, "mixed-port", port)) emit changed(); }
void SettingsController::setHttpPort(int port) { if (setPort(m_ports, "port", port)) emit changed(); }
void SettingsController::setSocksPort(int port) { if (setPort(m_ports, "socks-port", port)) emit changed(); }
void SettingsController::setRedirPort(int port) { if (setPort(m_ports, "redir-port", port)) emit changed(); }
void SettingsController::setTproxyPort(int port) { if (setPort(m_ports, "tproxy-port", port)) emit changed(); }

void SettingsController::load()
{
    guarded(this, [this] {
        Store store;
        const auto settings = store.settings();
        const auto controlledPath = dataDir() + "/controlled.yaml";
        m_endpoint = settings.value("endpoint").toString();
        m_secret = settings.value("secret").toString();
        m_corePath = settings.value("core").toString();
        m_autoSystemProxy = settings.value("autoSystemProxy").toBool(true);
        m_systemNodeDns = settings.value("systemNodeDns").toBool(true);
        m_autoStart = settings.value("autoStart").toBool(false);
        m_updateMinutes = settings.value("updateMinutes").toInt(360);
        const auto controlled = QFile::exists(controlledPath) ? parseConfig(readFile(controlledPath))
                                                              : YAML::Node(YAML::NodeType::Map);
        m_unifiedDelay = !controlled["unified-delay"] || controlled["unified-delay"].as<bool>();
        // Ports come from the running configuration, then from the controlled
        // overrides; a fresh installation keeps the documented defaults.
        YAML::Node config(YAML::NodeType::Map);
        const auto work = dataDir() + "/work/config.yaml";
        const auto active = settings.value("active").toString();
        if (QFile::exists(work)) config = parseConfig(readFile(work));
        else if (validId(active)) config = parseConfig(store.profile(active));
        if (QFile::exists(controlledPath)) config = mergeYaml(config, parseConfig(readFile(controlledPath)));
        for (const auto &key : proxyPortKeys()) {
            const auto value = config[key.toStdString()];
            m_ports[key] = value ? value.as<int>() : (key == "mixed-port" ? 16666 : 0);
        }
        QStringList bypass = defaultProxyBypass();
        if (settings.value("proxyBypass").isArray()) {
            bypass.clear();
            for (const auto &entry : settings.value("proxyBypass").toArray()) bypass.append(entry.toString());
        }
        setBypass(bypass.join('\n'));
        emit changed();
    });
}

void SettingsController::apply()
{
    guarded(this, [this] {
        // The old panel only validated the endpoint before writing it back.
        Api api;
        QString error;
        if (!api.configure(m_endpoint, m_secret, &error)) { emit message(error, true); return; }
        Store store;
        auto settings = store.settings();
        settings["endpoint"] = m_endpoint;
        settings["secret"] = m_secret;
        settings["core"] = m_corePath;
        settings["autoSystemProxy"] = m_autoSystemProxy;
        settings["systemNodeDns"] = m_systemNodeDns;
        settings["autoStart"] = m_autoStart;
        settings["updateMinutes"] = m_updateMinutes;
        store.setSettings(settings);
        const auto controlledPath = dataDir() + "/controlled.yaml";
        auto controlled = QFile::exists(controlledPath) ? parseConfig(readFile(controlledPath))
                                                        : YAML::Node(YAML::NodeType::Map);
        controlled["unified-delay"] = m_unifiedDelay;
        writeFile(controlledPath, emitYaml(controlled));
        // The background service owns the autostart unit now.
        m_service->setEnabled(m_autoStart);
        emit message(tr("设置已保存"), false);
        emit changed();
    });
}

void SettingsController::applyPorts()
{
    guarded(this, [this] {
        QJsonObject ports;
        for (auto it = m_ports.begin(); it != m_ports.end(); ++it) ports[it.key()] = it.value().toInt();
        validateProxyPorts(ports);
        // Service::request() folds the daemon answer into the exported state and
        // reports failures itself, so only the success message belongs here.
        m_service->request({{"action", "ports"}, {"options", ports}}, [this](const QJsonValue &value, const QString &error) {
            if (error.isEmpty()) {
                const auto text = value.toObject().value("message").toString();
                if (!text.isEmpty()) emit message(text, false);
            }
            load();
        });
    });
}

void SettingsController::applyBypass()
{
    guarded(this, [this] {
        const auto entries = normalizeProxyBypass(textEntries(m_bypass));
        m_bypass = entries.join('\n');
        m_bypassError.clear();
        emit bypassEdited();
        m_service->request({{"action", "bypass"}, {"entries", QJsonArray::fromStringList(entries)}}, [this](const QJsonValue &value, const QString &error) {
            if (!error.isEmpty()) return;
            const auto text = value.toObject().value("message").toString();
            if (!text.isEmpty()) emit message(text, false);
            emit systemProxyChanged();
        });
    });
}

void SettingsController::fillDefaultBypass()
{
    m_bypass = defaultProxyBypass().join('\n');
    m_bypassError.clear();
    emit bypassEdited();
}

void SettingsController::setBypass(const QString &text)
{
    m_bypass = text;
    m_bypassError.clear();
    try { normalizeProxyBypass(textEntries(text)); }
    catch (const std::exception &e) { m_bypassError = QString::fromUtf8(e.what()); }
    emit bypassEdited();
}

void SettingsController::notifyBypass()
{
    // The QML editor writes `bypass` directly; re-validate the current text.
    setBypass(m_bypass);
}

void SettingsController::setMode(const QString &mode)
{
    if (!QStringList{"rule", "global", "direct"}.contains(mode)) return;
    coreRequest(this, "PATCH", "/configs", QJsonObject{{"mode", mode}}, [this, mode](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) { emit message(error, true); return; }
        guarded(this, [this, mode] {
            const auto path = dataDir() + "/controlled.yaml";
            auto base = QFile::exists(path) ? parseConfig(readFile(path)) : YAML::Node(YAML::NodeType::Map);
            const auto delta = parseConfig(QJsonDocument(QJsonObject{{"mode", mode}}).toJson(QJsonDocument::Compact));
            writeFile(path, emitYaml(mergeYaml(base, delta)));
            emit message(tr("设置已应用并保存"), false);
        });
    });
}

void SettingsController::editSection(const QString &section)
{
    guarded(this, [this, section] {
        Store store;
        const auto active = store.settings().value("active").toString();
        auto node = parseConfig(store.profile(active));
        const auto current = node[section.toStdString()];
        YAML::Node wrapper;
        wrapper[section.toStdString()] = current;
        const auto example = ProxyPlugin::sectionExample(section);
        const bool empty = !current || current.IsNull();
        m_document.setTitle(section.toUpper());
        m_document.setHelp(ProxyPlugin::sectionHelp(section));
        m_document.setExample(QString::fromUtf8(example));
        m_document.setEditable(true);
        m_document.setText(QString::fromUtf8(empty && !example.isEmpty() ? example : emitYaml(wrapper)));
    });
}

void SettingsController::saveSection(const QString &section, const QString &text)
{
    guarded(this, [this, section, text] {
        Store store;
        const auto active = store.settings().value("active").toString();
        auto node = parseConfig(store.profile(active));
        node[section.toStdString()] = parseConfig(text.toUtf8())[section.toStdString()];
        QString name, url;
        for (const auto &entry : store.profiles()) {
            const auto record = entry.toObject();
            if (record.value("id").toString() != active) continue;
            name = record.value("name").toString();
            url = record.value("url").toString();
            break;
        }
        store.saveProfile(name, url, emitYaml(node), active);
        m_service->restart(active);
    });
}

void SettingsController::showRuntimeConfig()
{
    coreRequest(this, "GET", "/configs", {}, [this](const QJsonValue &value, const QString &error) {
        if (!error.isEmpty()) { emit message(error, true); return; }
        m_document.setTitle(tr("运行配置"));
        m_document.setHelp({});
        m_document.setExample({});
        m_document.setEditable(false);
        m_document.setText(QString::fromUtf8(QJsonDocument(value.toObject()).toJson()));
    });
}

void SettingsController::showRuntimeProfile()
{
    guarded(this, [this] {
        Store store;
        const auto active = store.settings().value("active").toString();
        const auto runtime = store.runtime(active);
        m_document.setTitle(tr("生效配置"));
        m_document.setHelp({});
        m_document.setExample({});
        m_document.setEditable(false);
        m_document.setText(QString::fromUtf8(runtime));
    });
}

void SettingsController::backupTo(const QString &path)
{
    guarded(this, [path] { writeFile(path, backup(dataDir())); });
}

void SettingsController::restoreFrom(const QString &path)
{
    guarded(this, [this, path] {
        restore(dataDir(), readFile(path));
        load();
    });
}

void SettingsController::webdav(const QString &url, const QString &user, const QString &password, bool upload)
{
    const QUrl target(url);
    if (!target.isValid() || target.scheme() != "https" || !target.userInfo().isEmpty()) {
        emit message(tr("WebDAV 需要 HTTPS 文件 URL"), true);
        return;
    }
    guarded(this, [this, target, user, password, upload] {
        Store store;
        auto settings = store.settings();
        settings["webdavUrl"] = target.toString();
        settings["webdavUser"] = user;
        store.setSettings(settings);
        // The download must not travel through the proxy this plugin configures.
        auto *manager = new QNetworkAccessManager(this);
        manager->setProxy(QNetworkProxy::NoProxy);
        QNetworkRequest request(target);
        request.setRawHeader("Authorization", "Basic " + (user + ':' + password).toUtf8().toBase64());
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        auto *reply = upload ? manager->put(request, backup(dataDir())) : manager->get(request);
        QTimer::singleShot(20000, reply, &QNetworkReply::abort);
        connect(reply, &QNetworkReply::readyRead, reply, [reply] {
            if (reply->bytesAvailable() > 32 * 1024 * 1024) reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, manager, reply, upload] {
            manager->deleteLater();
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
                emit message(reply->errorString(), true);
                return;
            }
            const auto body = reply->readAll();
            guarded(this, [this, body, upload] {
                if (!upload) restore(dataDir(), body);
                load();
                emit message(upload ? tr("备份上传完成") : tr("备份恢复完成"), false);
            });
        });
    });
}

void SettingsController::chooseCore(const QString &path)
{
    setCorePath(path);
}

// ------------------------------------------------------------- overrides ----
OverrideController::OverrideController(Service *service, QObject *parent)
    : QObject(parent), m_service(service)
{
}

void OverrideController::load()
{
    guarded(this, [this] {
        const auto settings = Store().settings();
        m_yamlEnabled = settings.value("overrideEnabled").toBool(true);
        m_scriptEnabled = settings.value("scriptEnabled").toBool(false);
        const auto yamlPath = dataDir() + "/override.yaml";
        const auto scriptPath = dataDir() + "/override.js";
        auto yaml = QFile::exists(yamlPath) ? QString::fromUtf8(readFile(yamlPath)) : QString();
        if (yaml.trimmed().isEmpty() || yaml.trimmed() == "{}") yaml = QString::fromUtf8(ProxyPlugin::yamlOverrideExample());
        m_yaml = yaml;
        m_script = QFile::exists(scriptPath) ? QString::fromUtf8(readFile(scriptPath)) : QString();
        emit changed();
    });
}

void OverrideController::save()
{
    guarded(this, [this] {
        const auto yaml = m_yaml.trimmed().isEmpty() ? QByteArray("{}\n") : m_yaml.toUtf8();
        parseConfig(yaml);
        writeFile(dataDir() + "/override.yaml", yaml);
        writeFile(dataDir() + "/override.js", m_script.toUtf8());
        Store store;
        auto settings = store.settings();
        settings["overrideEnabled"] = m_yamlEnabled;
        settings["scriptEnabled"] = m_scriptEnabled;
        store.setSettings(settings);
        emit message(tr("覆写已保存；重启内核后生效"), false);
    });
}

void OverrideController::fillExample()
{
    m_yaml = QString::fromUtf8(ProxyPlugin::yamlOverrideExample());
    emit changed();
}

// ------------------------------------------------------------------- tun ----
TunController::TunController(Service *service, QObject *parent)
    : QObject(parent), m_service(service)
{
    const auto defaults = yamlToJson(defaultTunConfig()).toObject();
    m_device = defaults.value("device").toString("Mihomo");
    m_stack = defaults.value("stack").toString("mixed");
    m_mtu = defaults.value("mtu").toInt(1500);
    m_dnsHijack = "any:53";
    m_autoRoute = defaults.value("auto-route").toBool(true);
    m_autoRedirect = defaults.value("auto-redirect").toBool(false);
    m_autoDetectInterface = defaults.value("auto-detect-interface").toBool(true);
    m_strictRoute = defaults.value("strict-route").toBool(false);
    connect(m_service, &Service::stateChanged, this, &TunController::refreshStatus);
    connect(m_service, &Service::tunChanged, this, &TunController::refreshStatus);
    refreshStatus();
}

void TunController::load()
{
    guarded(this, [this] {
        auto tun = defaultTunConfig();
        const auto path = dataDir() + "/controlled.yaml";
        if (QFile::exists(path)) {
            const auto controlled = parseConfig(readFile(path));
            if (controlled["tun"]) tun = mergeYaml(tun, controlled["tun"]);
        }
        const auto options = yamlToJson(tun).toObject();
        QStringList dns, exclude;
        for (const auto &entry : options.value("dns-hijack").toArray()) dns.append(entry.toString());
        for (const auto &entry : options.value("route-exclude-address").toArray()) exclude.append(entry.toString());
        m_device = options.value("device").toString("Mihomo");
        m_stack = options.value("stack").toString("mixed");
        m_mtu = options.value("mtu").toInt(1500);
        m_dnsHijack = dns.join(", ");
        m_routeExclude = exclude.join('\n');
        m_autoRoute = options.value("auto-route").toBool(true);
        m_autoRedirect = options.value("auto-redirect").toBool(false);
        m_autoDetectInterface = options.value("auto-detect-interface").toBool(true);
        m_strictRoute = options.value("strict-route").toBool(false);
        emit edited();
        refreshStatus();
    });
}

void TunController::apply()
{
    QVariantMap options;
    options["device"] = m_device;
    options["stack"] = m_stack;
    options["mtu"] = m_mtu;
    options["dns-hijack"] = textEntries(m_dnsHijack);
    options["route-exclude-address"] = textEntries(m_routeExclude);
    options["auto-route"] = m_autoRoute;
    options["auto-redirect"] = m_autoRedirect;
    options["auto-detect-interface"] = m_autoDetectInterface;
    options["strict-route"] = m_strictRoute;
    // `enable` is deliberately omitted: the daemon keeps its current value.
    applyOptions(options);
}

void TunController::setEnabled(bool enabled)
{
    applyOptions(QVariantMap{{"enable", enabled}});
}

void TunController::applyOptions(const QVariantMap &options)
{
    if (m_busy) return;
    guarded(this, [this, options] {
        const auto validated = validateTunOptions(QJsonObject::fromVariantMap(options));
        m_busy = true;
        emit changed();
        m_service->request({{"action", "tun"}, {"options", validated}}, [this](const QJsonValue &value, const QString &error) {
            m_busy = false;
            if (error.isEmpty()) {
                const auto text = value.toObject().value("message").toString();
                if (!text.isEmpty()) emit message(text, false);
            }
            load();
            emit changed();
        });
    });
}

void TunController::authorize()
{
    // Service::authorizeCore() owns the bundled-core check, the desktop
    // authorization prompt and its own user messages.
    if (m_busy) return;
    m_service->authorizeCore();
}

void TunController::refreshStatus()
{
    const auto text = m_service->tunEnabled() ? tr("虚拟网卡 %1 已启用").arg(m_service->tunDevice())
                                              : tr("虚拟网卡未启用");
    if (m_status == text) return;
    m_status = text;
    emit changed();
}

// ----------------------------------------------------------------- tools ----
ToolsController::ToolsController(Api *api, QObject *parent)
    : QObject(parent), m_api(api)
{
}

void ToolsController::queryDns(const QString &name, const QString &type)
{
    guarded(this, [this, name, type] {
        QUrlQuery query;
        query.addQueryItem("name", name);
        query.addQueryItem("type", type);
        m_api->get("/dns/query?" + query.toString(QUrl::FullyEncoded), [this](const QJsonValue &value, const QString &error) {
            if (!error.isEmpty()) { emit message(error, true); return; }
            const auto result = QString::fromUtf8(QJsonDocument(value.toObject()).toJson());
            if (m_dnsResult == result) return;
            m_dnsResult = result;
            emit changed();
        });
    });
}

void ToolsController::flushCache(const QString &kind)
{
    if (kind != "dns" && kind != "fakeip") return;
    m_api->request("POST", QString("/cache/%1/flush").arg(kind), {}, [this](const QJsonValue &, const QString &error) {
        emit message(error.isEmpty() ? tr("缓存已清除") : error, !error.isEmpty());
    });
}

// ------------------------------------------------------------- overview ----
OverviewController::OverviewController(Api *api, Service *service, QObject *parent)
    : QObject(parent), m_api(api), m_service(service)
{
    m_stateText = tr("内核离线");
    m_trafficText = tr("上传 —    下载 —");
    m_statisticsText = tr("连接 —    总上传 —    总下载 —    内存 —");
    connect(m_api, &Api::streamData, this, [this](const QString &path, const QJsonObject &data) {
        if (path != "/traffic" || !m_active) return;
        const auto text = tr("上传 %1/s    下载 %2/s")
                              .arg(sizeText(data.value("up").toDouble()), sizeText(data.value("down").toDouble()));
        if (m_trafficText == text) return;
        m_trafficText = text;
        emit changed();
    });
    connect(m_service, &Service::stateChanged, this, &OverviewController::updateState);
}

void OverviewController::refresh()
{
    m_api->get("/configs", [this](const QJsonValue &value, const QString &error) {
        m_connected = error.isEmpty();
        if (m_connected) {
            const auto config = value.toObject();
            const auto kernel = tr("内核已连接    混合端口 %1    HTTP %2    SOCKS %3    局域网 %4")
                                    .arg(config.value("mixed-port").toInt())
                                    .arg(config.value("port").toInt())
                                    .arg(config.value("socks-port").toInt())
                                    .arg(config.value("allow-lan").toBool() ? tr("允许") : tr("禁止"));
            const auto mode = config.value("mode").toString();
            const bool dirty = m_kernelText != kernel || (!mode.isEmpty() && m_mode != mode);
            m_kernelText = kernel;
            if (!mode.isEmpty()) m_mode = mode;
            if (dirty) emit changed();
        }
        updateState();
    });
    if (!m_active) return;
    m_api->get("/connections", [this](const QJsonValue &value, const QString &error) {
        if (!error.isEmpty() || !m_active) return;
        const auto data = value.toObject();
        const auto text = tr("连接 %1    总上传 %2    总下载 %3    内存 %4")
                              .arg(data.value("connections").toArray().size())
                              .arg(sizeText(data.value("uploadTotal").toDouble()),
                                   sizeText(data.value("downloadTotal").toDouble()),
                                   sizeText(data.value("memory").toDouble()));
        if (m_statisticsText == text) return;
        m_statisticsText = text;
        emit changed();
    });
}

void OverviewController::activate()
{
    if (m_active) return;
    m_active = true;
    m_api->stream("/traffic", true);
    refresh();
}

void OverviewController::deactivate()
{
    if (!m_active) return;
    m_active = false;
    m_api->stream("/traffic", false);
}

void OverviewController::updateState()
{
    QString state;
    if (m_connected) state = tr("内核已连接");
    else if (!m_service->daemonReachable()) state = tr("后台未启动。请导入订阅并打开“代理服务”开关。");
    else if (m_service->busy()) state = tr("正在验证配置并启动内核，请稍候…");
    else if (!m_service->error().isEmpty()) state = tr("内核启动失败：%1").arg(m_service->error());
    else if (!m_service->running()) state = tr("后台已运行，内核未启动。导入订阅后可打开“代理服务”开关。");
    else state = tr("内核正在运行，等待控制器连接。");
    const bool disconnected = !m_connected && !m_kernelText.isEmpty();
    if (m_stateText == state && !disconnected) return;
    m_stateText = state;
    if (disconnected) m_kernelText.clear();
    emit changed();
}
} // namespace Party
