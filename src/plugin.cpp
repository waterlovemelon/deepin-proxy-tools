// SPDX-License-Identifier: LGPL-3.0-or-later
// V25 control center plugin entry: dccData object returned by dccV25::DccFactory.
#include "plugindata.h"
#include "dccfactory.h"
#include <QDir>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QTimer>
#include <QUrl>

namespace Party {
static QString sectionHelpText(const QString &section)
{
    static const QMap<QString, QString> help{
        {"dns", QObject::tr("DNS：设置域名解析服务器。示例使用系统 DNS，通常无需单独开放 DNS 监听端口。nameserver 是解析器列表；启用 TUN 时会联动启用 DNS。")},
        {"sniffer", QObject::tr("嗅探：从 HTTP/TLS 流量中识别域名，用于域名规则匹配。示例默认关闭，将 enable 改为 true 后启用；HTTP 80/8080、TLS 443 是待识别的目标端口。")},
        {"hosts", QObject::tr("Hosts：为域名指定固定 IP。示例将 example.test 指向文档专用地址 192.0.2.10，请替换为你实际需要的域名和 IP。")},
        {"listeners", QObject::tr("监听器：新增独立代理入口。示例仅在本机 127.0.0.1:17892 开放 HTTP 代理，使用 DIRECT 策略；端口必须空闲，可按需改为已有策略组名。常用混合/HTTP/SOCKS 端口也可直接在设置页修改。")},
        {"tunnels", QObject::tr("隧道：把本地端口转发到指定目标。示例把本机 TCP 18081 转发到 127.0.0.1:18080，使用 DIRECT；目标端口需有实际服务。格式为 协议,本地监听,目标地址,策略。")}};
    return help.value(section) + QObject::tr("\n只修改当前订阅的该部分；保存后验证并重新加载，取消不生效。YAML 注意缩进；覆写和专用设置可能覆盖这里的值。");
}

QString ProxyPlugin::sectionHelp(const QString &section) { return sectionHelpText(section); }

QByteArray ProxyPlugin::sectionExample(const QString &section)
{
    static const QMap<QString, QByteArray> examples{
        {"dns", "dns:\n  enable: true\n  enhanced-mode: redir-host\n  nameserver:\n    - system\n"},
        {"sniffer", "sniffer:\n  enable: false\n  sniff:\n    HTTP:\n      ports: [80, 8080]\n      override-destination: true\n    TLS:\n      ports: [443]\n"},
        {"hosts", "hosts:\n  example.test: 192.0.2.10\n"},
        {"listeners", "listeners:\n  - name: local-http-example\n    type: http\n    listen: 127.0.0.1\n    port: 17892\n    proxy: DIRECT\n"},
        {"tunnels", "tunnels:\n  - tcp,127.0.0.1:18081,127.0.0.1:18080,DIRECT\n"}};
    return examples.value(section);
}

QByteArray ProxyPlugin::yamlOverrideExample()
{
    return "# 映射递归合并；列表整体替换；null 删除字段。\n"
           "# 此示例添加 hosts 映射并关闭局域网访问，不替换订阅节点和规则。\n"
           "allow-lan: false\nhosts:\n  example.test: 192.0.2.10\n"
           "# DNS 列表示例（取消注释后会替换原 nameserver 列表）：\n"
           "# dns:\n#   nameserver:\n#     - 10.20.0.10\n#     - system\n";
}

ProxyPlugin::ProxyPlugin(QObject *parent)
    : QObject(parent)
    , m_service(this)
    , m_api(this)
    , m_download(new QNetworkAccessManager(this))
    , m_overview(&m_api, &m_service, this)
    , m_proxies(&m_api, &m_service, this)
    , m_profiles(&m_service, this)
    , m_connections(&m_api, this)
    , m_rules(&m_api, &m_service, this)
    , m_providers(&m_api, this)
    , m_logs(&m_api, this)
    , m_override(&m_service, this)
    , m_settings(&m_service, this)
    , m_tun(&m_service, this)
    , m_tools(&m_api, this)
    , m_refreshTimer(new QTimer(this))
{
    // Downloads must never travel through the proxy this plugin is configuring.
    m_download->setProxy(QNetworkProxy::NoProxy);

    // Controllers report through one status area; the QML pages bind to it.
    const auto relay = [this](const QString &text, bool error) { emit message(text, error); };
    connect(&m_service, &Service::message, this, relay);
    connect(&m_overview, &OverviewController::message, this, relay);
    connect(&m_proxies, &ProxyController::message, this, relay);
    connect(&m_profiles, &ProfileModel::message, this, relay);
    connect(&m_connections, &ConnectionModel::message, this, relay);
    connect(&m_rules, &RuleModel::message, this, relay);
    connect(&m_providers, &ProviderModel::message, this, relay);
    connect(&m_logs, &LogModel::message, this, relay);
    connect(&m_override, &OverrideController::message, this, relay);
    connect(&m_settings, &SettingsController::message, this, relay);
    connect(&m_tun, &TunController::message, this, relay);
    connect(&m_tools, &ToolsController::message, this, relay);

    // A controller that starts answering (or stops answering) invalidates every
    // cached latency result: they belong to the previous core instance.
    connect(&m_api, &Api::configurationChanged, this, [this] { m_proxies.clearTests(); });
    connect(&m_service, &Service::stateChanged, this, [this] {
        if (!m_service.running()) { m_proxies.setApiConnected(false); m_logs.deactivate(); }
    });

    connect(m_refreshTimer, &QTimer::timeout, this, &ProxyPlugin::refresh);
    m_refreshTimer->setInterval(3000);
    m_refreshTimer->start();

    m_settings.load();
    m_override.load();
    m_tun.load();
    QTimer::singleShot(0, this, &ProxyPlugin::refresh);
}

void ProxyPlugin::setConnected(bool connected)
{
    if (m_connected == connected) return;
    m_connected = connected;
    m_proxies.setApiConnected(connected);
    emit connectedChanged();
}

// Page names that belong to this module; anything else means the user is in a
// different control center module and the plugin must stay idle.
static bool isOwnPage(const QString &page)
{
    static const QStringList pages{"dccproxy", "overview", "proxies", "profiles", "connections",
                                   "rules", "providers", "logs", "override", "tools", "settings", "tun"};
    return pages.contains(page);
}

void ProxyPlugin::setActivePage(const QString &page)
{
    if (m_activePage == page) return;
    m_activePage = page;
    m_proxies.setActive(page == "dccproxy" || page == "proxies" || page == "rules");
    if (page == "overview") m_overview.activate(); else m_overview.deactivate();
    // The log stream is the single most expensive poller: only while its page
    // (or the module landing page) is on screen.
    if (page == "logs" || page == "dccproxy") m_logs.activate(); else m_logs.deactivate();
    emit activePageChanged();
    if (isOwnPage(m_activePage)) refresh();
}

void ProxyPlugin::refresh()
{
    // Nothing on screen belongs to this plugin: do not poll systemd or the
    // controller (the timer keeps running so the state is fresh on return).
    if (!isOwnPage(m_activePage)) return;
    // Api::configure() is idempotent: it returns early while endpoint/secret are
    // unchanged and only restarts the controller streams on a real change.
    const QString endpoint = m_settings.endpoint();
    if (!endpoint.isEmpty()) m_api.configure(endpoint, m_settings.secret());
    m_service.refresh();
    m_overview.refresh();
    m_proxies.refresh();
    m_api.get("/configs", [this](const QJsonValue &value, const QString &error) {
        Q_UNUSED(value)
        setConnected(error.isEmpty());
    });
}

void ProxyPlugin::report(const QString &text, bool error) { emit message(text, error); }

void ProxyPlugin::openDocument(int which)
{
    if (which == 0) m_settings.showRuntimeProfile();
    else m_settings.showRuntimeConfig();
}

QString ProxyPlugin::version() const
{
#ifdef PARTY_VERSION
    return QStringLiteral(PARTY_VERSION);
#else
    return QStringLiteral("0.2.0");
#endif
}

QString ProxyPlugin::formatSize(double bytes) const
{
    const QStringList units{"B", "KiB", "MiB", "GiB", "TiB"};
    int index = 0;
    while (bytes >= 1024 && index < units.size() - 1) { bytes /= 1024; ++index; }
    return QString::number(bytes, 'f', index ? 1 : 0) + ' ' + units[index];
}

bool ProxyPlugin::writeTextFile(const QString &path, const QString &text)
{
    try {
        writeFile(path, text.toUtf8());
        return true;
    } catch (const std::exception &e) {
        emit message(QString::fromUtf8(e.what()), true);
        return false;
    }
}

// QtQuick FileDialog hands out URLs; converting them in C++ keeps non-ASCII
// paths intact instead of stripping "file://" in JavaScript.
QString ProxyPlugin::localPath(const QUrl &url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}
} // namespace Party

using ProxyPlugin = Party::ProxyPlugin;
DCC_FACTORY_CLASS(ProxyPlugin)
#include "plugin.moc"
