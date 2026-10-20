// SPDX-License-Identifier: LGPL-3.0-or-later
// Runtime data models for the V25 control center plugin: connections, rules,
// providers and the log viewer (dccData.connections / rules / providers / logs).
#include "plugindata.h"
#include "config.h"
#include "subscriptionnodes.h"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>
#include <stdexcept>

namespace Party {
namespace {
const int kMaxLogLines = 2000;
const int kLogTailBytes = 64 * 1024;
const int kStreamResyncMs = 3000;
const char kResyncTimer[] = "dccProxyLogResync";

// Panel::sizeText.
QString sizeText(double bytes)
{
    const QStringList units{"B", "KiB", "MiB", "GiB", "TiB"};
    int index = 0;
    while (bytes >= 1024 && index < units.size() - 1) { bytes /= 1024; ++index; }
    return QString::number(bytes, 'f', index ? 1 : 0) + ' ' + units[index];
}

// The old pages filtered on the serialized JSON of every row.
bool matches(const QJsonObject &item, const QString &search)
{
    return search.isEmpty() || QString::fromUtf8(QJsonDocument(item).toJson()).contains(search, Qt::CaseInsensitive);
}

QString targetOf(const QJsonObject &connection)
{
    const auto metadata = connection["metadata"].toObject();
    const auto host = metadata["host"].toString();
    return (host.isEmpty() ? metadata["destinationIP"].toString() : host) + ':' + metadata["destinationPort"].toString();
}

QString chainOf(const QJsonObject &connection)
{
    QStringList chain;
    for (const auto &node : connection["chains"].toArray()) chain << node.toString();
    std::reverse(chain.begin(), chain.end());
    return chain.join(" → ");
}

QJsonObject providerEntry(const QString &kind, const QString &name, const QJsonObject &provider)
{
    QJsonObject entry{{"name", name}, {"kind", kind},
                      {"vehicleType", provider["vehicleType"].toString()},
                      {"updatedAt", provider["updatedAt"].toString()},
                      {"entries", provider.contains("proxies") ? provider["proxies"].toArray().size() : provider["ruleCount"].toInt()}};
    return entry;
}

QJsonArray providerRows(const QJsonArray &items, const QString &kind, const QJsonObject &providers)
{
    auto rows = items;
    for (auto it = providers.begin(); it != providers.end(); ++it) rows.append(providerEntry(kind, it.key(), it.value().toObject()));
    return rows;
}
} // namespace

// ----------------------------------------------------------- connections ----
ConnectionModel::ConnectionModel(Api *api, QObject *parent) : QAbstractTableModel(parent), m_api(api) { }

int ConnectionModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_items.size(); }

int ConnectionModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : 7; }

QVariant ConnectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) return {};
    const auto connection = m_items.at(index.row()).toObject();
    const auto metadata = connection["metadata"].toObject();
    switch (role) {
    case IdRole: return connection["id"].toString();
    case TargetRole: return targetOf(connection);
    case ProcessRole: return metadata["process"].toString();
    case KindRole: return metadata["network"].toString() + '/' + metadata["type"].toString();
    case RuleRole: return connection["rule"].toString() + ' ' + connection["rulePayload"].toString();
    case ChainRole: return chainOf(connection);
    case UploadRole: return connection["upload"].toDouble();
    case DownloadRole: return connection["download"].toDouble();
    case UploadTextRole: return sizeText(connection["upload"].toDouble());
    case DownloadTextRole: return sizeText(connection["download"].toDouble());
    case Qt::DisplayRole:
        switch (index.column()) {
        case 0: return targetOf(connection);
        case 1: return metadata["process"].toString();
        case 2: return metadata["network"].toString() + '/' + metadata["type"].toString();
        case 3: return connection["rule"].toString() + ' ' + connection["rulePayload"].toString();
        case 4: return chainOf(connection);
        case 5: return sizeText(connection["upload"].toDouble());
        case 6: return sizeText(connection["download"].toDouble());
        default: return {};
        }
    default: return {};
    }
}

QHash<int, QByteArray> ConnectionModel::roleNames() const
{
    return {{IdRole, "id"}, {TargetRole, "target"}, {ProcessRole, "process"}, {KindRole, "kind"},
            {RuleRole, "rule"}, {ChainRole, "chain"}, {UploadRole, "upload"}, {DownloadRole, "download"},
            {UploadTextRole, "uploadText"}, {DownloadTextRole, "downloadText"}};
}

void ConnectionModel::setSearch(const QString &search)
{
    if (m_search == search) return;
    m_search = search;
    emit changed();
    refresh();
}

void ConnectionModel::refresh()
{
    m_api->get("/connections", [this](const QJsonValue &value, const QString &error) {
        if (!error.isEmpty()) return; // keep the previous rows, as the old page did
        QJsonArray rows;
        for (const auto &item : value.toObject()["connections"].toArray()) {
            const auto connection = item.toObject();
            if (matches(connection, m_search)) rows.append(connection);
        }
        beginResetModel();
        m_items = rows;
        endResetModel();
        emit changed();
    });
}

QString ConnectionModel::idAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).toObject()["id"].toString() : QString();
}

void ConnectionModel::closeAt(int row)
{
    const auto id = idAt(row);
    if (id.isEmpty()) return;
    m_api->request("DELETE", "/connections/" + Api::segment(id), {}, [this](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) emit message(error, true);
        refresh();
    });
}

void ConnectionModel::closeAll()
{
    m_api->request("DELETE", "/connections", {}, [this](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) emit message(error, true);
        refresh();
    });
}

// ----------------------------------------------------------------- rules ----
RuleModel::RuleModel(Api *api, Service *service, QObject *parent)
    : QAbstractTableModel(parent), m_api(api), m_service(service) { }

int RuleModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_items.size(); }

int RuleModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : 4; }

QVariant RuleModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) return {};
    const auto rule = m_items.at(index.row()).toObject();
    const bool disabled = rule["extra"].toObject()["disabled"].toBool();
    switch (role) {
    case TypeRole: return rule["type"].toString();
    case PayloadRole: return rule["payload"].toString();
    case ProxyRole: return rule["proxy"].toString();
    case DisabledRole: return disabled;
    case IndexRole: return rule["index"].toInt();
    case Qt::DisplayRole:
        switch (index.column()) {
        case 0: return rule["type"].toString();
        case 1: return rule["payload"].toString();
        case 2: return rule["proxy"].toString();
        case 3: return disabled ? tr("禁用") : tr("启用");
        default: return {};
        }
    default: return {};
    }
}

QHash<int, QByteArray> RuleModel::roleNames() const
{
    return {{TypeRole, "type"}, {PayloadRole, "payload"}, {ProxyRole, "proxy"},
            {DisabledRole, "disabled"}, {IndexRole, "ruleIndex"}};
}

void RuleModel::setSearch(const QString &search)
{
    if (m_search == search) return;
    m_search = search;
    emit changed();
    refresh();
}

void RuleModel::refresh()
{
    m_api->get("/rules", [this](const QJsonValue &value, const QString &error) {
        if (!error.isEmpty()) return;
        const auto data = value.toObject()["rules"].toArray();
        QJsonArray rows;
        for (int i = 0; i < data.size(); ++i) {
            auto rule = data.at(i).toObject();
            if (!rule.contains("index")) rule["index"] = i;
            if (matches(rule, m_search)) rows.append(rule);
        }
        beginResetModel();
        m_items = rows;
        endResetModel();
        emit changed();
    });
}

void RuleModel::setDisabled(int row, bool disabled)
{
    if (row < 0 || row >= m_items.size()) return;
    const auto index = m_items.at(row).toObject()["index"].toInt();
    m_api->request("PATCH", "/rules/disable", {{QString::number(index), disabled}}, [this](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) emit message(error, true);
        refresh();
    });
}

QString RuleModel::websiteRulePreview(const QString &input, const QString &policy) const
{
    try {
        return websiteRule(input, policy);
    } catch (const std::exception &e) {
        return QString::fromUtf8(e.what()); // QML shows it and keeps 添加 disabled
    }
}

// Panel::addWebsiteRule: prepend one website rule to a subscription, verify that
// the rule survives the overrides, and restart the core when it is the active one.
void RuleModel::addWebsiteRule(const QString &input, const QString &policy, const QString &profileId)
{
    try {
        Store store;
        const auto settings = store.settings();
        auto id = profileId;
        if (id.isEmpty()) id = settings["active"].toString();
        const auto profiles = store.profiles();
        if (id.isEmpty() && !profiles.isEmpty()) id = profiles.first().toObject()["id"].toString();
        const auto rule = websiteRule(input, policy);
        const auto previous = store.profile(id);
        const auto updated = prependWebsiteRule(previous, rule);
        QJsonObject record;
        for (const auto &value : profiles) if (value.toObject()["id"].toString() == id) record = value.toObject();
        if (record.isEmpty()) throw std::runtime_error("订阅不存在，请先导入配置");
        store.saveProfile(record["name"].toString(), record["url"].toString(), updated, id);
        try {
            const auto runtime = parseConfig(store.runtime(id));
            bool present = false;
            if (runtime["rules"]) for (const auto &entry : runtime["rules"])
                if (QString::fromStdString(entry.as<std::string>()) == rule) { present = true; break; }
            if (!present) throw std::runtime_error("当前 YAML 或 JavaScript 覆写替换了规则列表，请先调整覆写中的 rules");
            if (policy != "DIRECT" && policy != "REJECT" && policy != "GLOBAL") {
                bool found = false;
                for (const auto *key : {"proxies", "proxy-groups"}) if (runtime[key]) for (const auto &entry : runtime[key])
                    if (entry["name"] && QString::fromStdString(entry["name"].as<std::string>()) == policy) found = true;
                if (!found) throw std::runtime_error("当前策略组不属于该订阅，请重新选择分组");
            }
        } catch (...) {
            store.saveProfile(record["name"].toString(), record["url"].toString(), previous, id);
            throw;
        }
        if (m_service && m_service->running() && id == settings["active"].toString()) m_service->restart(id);
        else emit message(tr("网址规则已保存到 %1，启动该配置后生效：%2").arg(record["name"].toString(), rule), false);
    } catch (const std::exception &e) {
        emit message(QString::fromUtf8(e.what()), true);
    }
}

// ------------------------------------------------------------- providers ----
ProviderModel::ProviderModel(Api *api, QObject *parent) : QAbstractTableModel(parent), m_api(api) { }

int ProviderModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_items.size(); }

int ProviderModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : 5; }

QVariant ProviderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) return {};
    const auto provider = m_items.at(index.row()).toObject();
    const bool proxies = provider["kind"].toString() == "proxies";
    switch (role) {
    case KindRole: return proxies ? tr("节点") : tr("规则");
    case NameRole: return provider["name"].toString();
    case VehicleRole: return provider["vehicleType"].toString();
    case UpdatedRole: return provider["updatedAt"].toString();
    case EntriesRole: return provider["entries"].toInt();
    case HealthRole: return proxies; // only proxy providers answer /healthcheck
    case Qt::DisplayRole:
        switch (index.column()) {
        case 0: return proxies ? tr("节点") : tr("规则");
        case 1: return provider["name"].toString();
        case 2: return provider["vehicleType"].toString();
        case 3: return provider["updatedAt"].toString();
        case 4: return QString::number(provider["entries"].toInt());
        default: return {};
        }
    default: return {};
    }
}

QHash<int, QByteArray> ProviderModel::roleNames() const
{
    return {{KindRole, "kind"}, {NameRole, "name"}, {VehicleRole, "vehicle"},
            {UpdatedRole, "updated"}, {EntriesRole, "entries"}, {HealthRole, "health"}};
}

void ProviderModel::refresh()
{
    m_api->get("/providers/proxies", [this](const QJsonValue &value, const QString &error) {
        const auto proxies = error.isEmpty() ? providerRows({}, "proxies", value.toObject()["providers"].toObject()) : QJsonArray();
        m_api->get("/providers/rules", [this, proxies](const QJsonValue &value, const QString &error) {
            const auto rows = error.isEmpty() ? providerRows(proxies, "rules", value.toObject()["providers"].toObject()) : proxies;
            beginResetModel();
            m_items = rows;
            endResetModel();
            emit changed();
        });
    });
}

void ProviderModel::updateAt(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    const auto provider = m_items.at(row).toObject();
    const auto path = "/providers/" + provider["kind"].toString() + '/' + Api::segment(provider["name"].toString());
    m_api->request("PUT", path, {}, [this](const QJsonValue &, const QString &error) {
        emit message(error.isEmpty() ? tr("操作完成") : error, !error.isEmpty());
        refresh();
    });
}

void ProviderModel::healthCheckAt(int row)
{
    if (row < 0 || row >= m_items.size()) return;
    const auto provider = m_items.at(row).toObject();
    if (provider["kind"].toString() != "proxies") return;
    const auto path = "/providers/proxies/" + Api::segment(provider["name"].toString()) + "/healthcheck";
    m_api->get(path, [this](const QJsonValue &, const QString &error) {
        emit message(error.isEmpty() ? tr("操作完成") : error, !error.isEmpty());
        refresh();
    });
}

void ProviderModel::updateGeo()
{
    m_api->request("POST", "/upgrade/geo", {}, [this](const QJsonValue &, const QString &error) {
        emit message(error.isEmpty() ? tr("GEO 数据已更新") : error, !error.isEmpty());
    });
}

// ------------------------------------------------------------------ logs ----
LogModel::LogModel(Api *api, QObject *parent) : QAbstractListModel(parent), m_api(api)
{
    m_status = tr("显示后台最近记录；实时日志会在新连接产生时追加。");
    // A dropped stream is only restarted by the next syncStreams(), like the old
    // page that re-synced on every panel refresh tick.
    auto *resync = new QTimer(this);
    resync->setObjectName(kResyncTimer);
    resync->setInterval(kStreamResyncMs);
    connect(resync, &QTimer::timeout, this, [this] { syncStreams(); });
    connect(m_api, &Api::streamData, this, [this](const QString &path, const QJsonObject &data) {
        if (!path.startsWith("/logs")) return;
        const QString connected = tr("实时日志已连接，后续连接记录会自动追加。");
        if (m_status != connected) { m_status = connected; emit changed(); }
        if (m_paused) return;
        const int overflow = m_lines.size() - kMaxLogLines + 1;
        if (overflow > 0) {
            beginRemoveRows({}, 0, overflow - 1);
            for (int i = 0; i < overflow; ++i) m_lines.removeFirst();
            endRemoveRows();
        }
        beginInsertRows({}, m_lines.size(), m_lines.size());
        m_lines.append(QDateTime::currentDateTime().toString("HH:mm:ss ") + data["type"].toString() + " " + data["payload"].toString());
        endInsertRows();
        emit changed();
    });
    connect(m_api, &Api::streamError, this, [this](const QString &path, const QString &error) {
        if (!path.startsWith("/logs") || !m_active || m_paused) return;
        const auto text = tr("实时日志连接中断：%1；将自动重连。后台历史仍可查看。").arg(error);
        if (m_status == text) return;
        m_status = text;
        emit changed();
    });
}

int LogModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_lines.size(); }

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size()) return {};
    switch (role) {
    case TextRole: return m_lines.at(index.row());
    case Qt::DisplayRole: return m_lines.at(index.row());
    default: return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const { return {{TextRole, "text"}}; }

void LogModel::setPaused(bool paused)
{
    if (m_paused == paused) return;
    m_paused = paused;
    emit changed();
    syncStreams();
}

void LogModel::setLevel(const QString &level)
{
    if (m_level == level) return;
    m_level = level;
    emit changed();
    // Only the level's own stream changes: stopping every stream would also kill
    // the traffic stream the overview owns. The old page did the same work by
    // re-syncing after m_api.stopStreams().
    syncStreams();
    // Panel::patchConfig, but never while the controller is unreachable.
    m_api->get("/configs", [this, level](const QJsonValue &, const QString &error) {
        if (!error.isEmpty()) return;
        m_api->request("PATCH", "/configs", {{"log-level", level}}, [this](const QJsonValue &, const QString &error) {
            if (!error.isEmpty()) { emit message(error, true); return; }
            try {
                const auto path = dataDir() + "/controlled.yaml";
                auto base = QFile::exists(path) ? parseConfig(readFile(path)) : YAML::Node(YAML::NodeType::Map);
                const auto delta = parseConfig(QJsonDocument(QJsonObject{{"log-level", m_level}}).toJson(QJsonDocument::Compact));
                writeFile(path, emitYaml(mergeYaml(base, delta)));
                emit message(tr("设置已应用并保存"), false);
            } catch (const std::exception &e) {
                emit message(QString::fromUtf8(e.what()), true);
            }
        });
    });
}

void LogModel::activate()
{
    m_active = true;
    if (!m_historyLoaded) {
        const auto tail = daemonLogTail();
        if (!tail.isEmpty()) {
            m_historyLoaded = true;
            auto lines = tail.split('\n');
            if (lines.size() > kMaxLogLines) lines = lines.mid(lines.size() - kMaxLogLines);
            beginInsertRows({}, m_lines.size(), m_lines.size() + lines.size() - 1);
            m_lines += lines;
            endInsertRows();
            emit changed();
        }
    }
    syncStreams();
    if (auto *resync = findChild<QTimer *>(kResyncTimer)) resync->start();
}

void LogModel::deactivate()
{
    m_active = false;
    syncStreams();
    if (auto *resync = findChild<QTimer *>(kResyncTimer)) resync->stop();
}

void LogModel::clear()
{
    if (m_lines.isEmpty()) return;
    beginResetModel();
    m_lines.clear();
    endResetModel();
    emit changed();
}

bool LogModel::exportTo(const QString &path)
{
    try {
        const auto text = m_lines.isEmpty() ? QString() : m_lines.join('\n') + '\n';
        writeFile(path, text.toUtf8());
        return true;
    } catch (const std::exception &e) {
        emit message(QString::fromUtf8(e.what()), true);
        return false;
    }
}

QString LogModel::daemonLogPath() const { return dataDir() + "/daemon.log"; }

QString LogModel::daemonLogTail() const
{
    QFile file(daemonLogPath());
    if (!file.open(QIODevice::ReadOnly)) return {};
    const bool truncated = file.size() > kLogTailBytes;
    if (truncated) file.seek(file.size() - kLogTailBytes);
    auto bytes = file.readAll();
    if (truncated) bytes = bytes.mid(bytes.indexOf('\n') + 1);
    auto text = QString::fromUtf8(bytes);
    text.remove(QRegularExpression("\x1b\\[[0-9;]*m"));
    return text.trimmed();
}

void LogModel::syncStreams()
{
    const QStringList levels{"info", "warning", "error", "debug", "silent"};
    for (const auto &level : levels) {
        const auto path = QString("/logs?level=%1").arg(level);
        m_api->stream(path, m_active && !m_paused && m_level == level);
    }
}
} // namespace Party
