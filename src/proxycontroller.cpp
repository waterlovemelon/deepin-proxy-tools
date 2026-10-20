// SPDX-License-Identifier: LGPL-3.0-or-later
// Proxy group / node page backend. The QML accordion (qml/ProxyGroupsPage.qml)
// talks to ProxyController only; the removed widget page is the behavioural
// reference: Panel::proxiesPage(), Panel::groupsPage(), Panel::renderGroups(),
// Panel::reloadNodeSources(), Panel::showProfileNodes(), Panel::previewProfile(),
// Panel::refreshProxies(), Panel::updateProxyGroups(), Panel::renderNodes(),
// Panel::renderNodeList(), Panel::selectNode(), Panel::chooseNode() and
// Panel::testNodes()/Panel::runNodeTests().
//
// Two modes, exactly like the widget page:
//  * preview: an explicit subscription source is selected, or the controller is
//    not answering. The stored profile YAML is read through subscriptionNodes();
//    the core is never started and the system proxy is never touched.
//  * runtime: the "自动" source with a connected controller. /proxies is polled
//    on refresh(), /providers/proxies once per source change, and both feed the
//    same models.
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QQueue>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>
#include <QVector>
#include <QCryptographicHash>
#include <algorithm>
#include <exception>
#include <utility>

#include "plugindata.h"
#include "config.h"
#include "nodeflags.h"
#include "subscriptionnodes.h"

namespace Party {
namespace {
// /proxies carries volatile fields (history timestamps, provider extras) that
// change on every health check. Rebuilding every model for those would rebuild
// every delegate on screen, so fingerprint only what the pages display.
QByteArray displayFingerprint(const QJsonObject &proxies)
{
    auto keys = proxies.keys();
    keys.sort();
    QString out;
    for (const auto &name : keys) {
        const auto info = proxies.value(name).toObject();
        const auto history = info.value(QStringLiteral("history")).toArray();
        const int delay = history.isEmpty() ? -1 : history.last().toObject().value(QStringLiteral("delay")).toInt(-1);
        QStringList all;
        for (const auto &entry : info.value(QStringLiteral("all")).toArray()) all.append(entry.toString());
        out += name % QLatin1Char('|') % info.value(QStringLiteral("type")).toString() % QLatin1Char('|')
             % info.value(QStringLiteral("now")).toString() % QLatin1Char('|')
             % (info.value(QStringLiteral("alive")).toBool() ? QLatin1String("1") : QLatin1String("0")) % QLatin1Char('|')
             % QString::number(delay) % QLatin1Char('|') % all.join(QLatin1Char(','))
             % QLatin1Char('|') % info.value(QStringLiteral("displayName")).toString() % QLatin1Char('\n');
    }
    return out.toUtf8();
}

const QString kProxiesPath = QStringLiteral("/proxies");
const QString kProvidersPath = QStringLiteral("/providers/proxies");
const QString kDefaultTestUrl = QStringLiteral("https://www.gstatic.com/generate_204");
constexpr int kRequestGraceMs = 2000; // the request outlives the node's own timeout

// QML loads resource icons through the qrc scheme; nodeFlagPath() returns the
// plain ":/..." form that QIcon used.
QString flagUrl(const QString &name)
{
    const auto path = nodeFlagPath(name);
    return path.startsWith(QStringLiteral(":/")) ? QStringLiteral("qrc") + path : path;
}

// True while nodes are queued or their requests are in flight. m_testing cannot
// be used for this: it keeps the whole node set of the run (see tally()).
bool testsRunning(const QQueue<QString> &queue, int active) { return active > 0 || !queue.isEmpty(); }

struct NodeView {
    QString search;
    int sortIndex = 0;
    bool hideUnavailable = false;
    bool previewing = true;
};

// renderNodeList(): filter, merge the group's own history with the latest plugin
// measurement, then order. A node of the running test shows 测速中 until its
// measurement lands in results.
QVector<NodeModel::Entry> nodeEntries(const QJsonObject &proxies, const QString &group, const NodeView &view,
                                      const QSet<QString> &testing, const QMap<QString, QPair<QDateTime, int>> &results,
                                      const QMap<QString, QString> &errors)
{
    const auto groupInfo = proxies.value(group).toObject();
    const auto members = groupInfo.value(QStringLiteral("all")).toArray();
    QVector<NodeModel::Entry> entries;
    entries.reserve(members.size());
    for (const auto &value : members) {
        const auto name = value.toString();
        const auto info = proxies.value(name).toObject();
        if (!view.search.isEmpty() && !name.contains(view.search, Qt::CaseInsensitive)) continue;
        if (view.hideUnavailable && info.contains(QStringLiteral("alive")) && !info.value(QStringLiteral("alive")).toBool()) continue;
        NodeModel::Entry entry;
        entry.name = name;
        entry.displayName = nodeDisplayName(name);
        entry.flag = flagUrl(name);
        entry.type = info.value(QStringLiteral("type")).toString();
        entry.alive = info.value(QStringLiteral("alive")).toBool(true);
        const auto history = info.value(QStringLiteral("history")).toArray();
        int delay = history.isEmpty() ? -1 : history.last().toObject().value(QStringLiteral("delay")).toInt();
        const auto measured = results.constFind(name);
        if (measured != results.constEnd()) {
            const auto historyTime = history.isEmpty() ? QDateTime()
                : QDateTime::fromString(history.last().toObject().value(QStringLiteral("time")).toString(), Qt::ISODateWithMs);
            if (!historyTime.isValid() || historyTime <= measured->first) delay = measured->second;
        }
        entry.delay = delay;
        entry.testing = testing.contains(name) && measured == results.constEnd();
        entry.active = !view.previewing && groupInfo.value(QStringLiteral("now")).toString() == name;
        entry.error = errors.value(name);
        entries.append(entry);
    }
    if (view.sortIndex == 1) std::stable_sort(entries.begin(), entries.end(), [](const NodeModel::Entry &a, const NodeModel::Entry &b) {
        return (a.delay > 0 ? a.delay : 1000000) < (b.delay > 0 ? b.delay : 1000000);
    });
    if (view.sortIndex == 2) std::sort(entries.begin(), entries.end(), [](const NodeModel::Entry &a, const NodeModel::Entry &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return entries;
}

// Per-node success/failure accounting. A run queues every node into m_testing and
// drops its entry from results; a node therefore counts as measured as soon as it
// has a fresh entry there, which keeps the two maps the facade declares enough to
// report the run without extra members.
void tally(const QSet<QString> &tested, const QMap<QString, QPair<QDateTime, int>> &results, int *passed, int *failed)
{
    *passed = 0;
    *failed = 0;
    for (const auto &name : tested) {
        if (results.value(name).second > 0) ++*passed;
        else ++*failed;
    }
}

NodeModel *rowNodes(GroupModel &model, int row)
{
    return static_cast<NodeModel *>(model.data(model.index(row, 0), GroupModel::NodesRole).value<QObject *>());
}

} // namespace

// ------------------------------------------------------------------ nodes ----
NodeModel::NodeModel(QObject *parent) : QAbstractListModel(parent) {}

int NodeModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_entries.size()); }

QVariant NodeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) return {};
    const auto &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole: return entry.name;
    case DisplayNameRole: return entry.displayName;
    case FlagRole: return entry.flag;
    case TypeRole: return entry.type;
    case DelayRole: return entry.delay;
    case DelayTextRole:
        if (entry.testing) return tr("测速中");
        if (entry.delay < 0) return tr("未测速");
        if (entry.delay == 0) return tr("超时");
        return tr("%1 ms").arg(entry.delay);
    case TestingRole: return entry.testing;
    case ActiveRole: return entry.active;
    case ErrorRole: return entry.error;
    case AliveRole: return entry.alive;
    default: return {};
    }
}

QHash<int, QByteArray> NodeModel::roleNames() const
{
    return {{NameRole, "name"}, {DisplayNameRole, "displayName"}, {FlagRole, "flag"}, {TypeRole, "type"},
            {DelayRole, "delay"}, {DelayTextRole, "delayText"}, {TestingRole, "testing"},
            {ActiveRole, "active"}, {ErrorRole, "error"}, {AliveRole, "alive"}};
}

QString NodeModel::nameAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).name : QString();
}

namespace {
bool sameNode(const NodeModel::Entry &a, const NodeModel::Entry &b)
{
    return a.name == b.name && a.displayName == b.displayName && a.flag == b.flag
        && a.type == b.type && a.delay == b.delay && a.testing == b.testing
        && a.active == b.active && a.alive == b.alive && a.error == b.error;
}
} // namespace

void NodeModel::reset(QVector<Entry> entries)
{
    if (entries.size() == m_entries.size()) {
        bool same = true;
        for (int i = 0; same && i < entries.size(); ++i) same = sameNode(entries.at(i), m_entries.at(i));
        if (same) return;
    }
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    emit changed();
}

// ----------------------------------------------------------------- groups ----
GroupModel::GroupModel(QObject *parent) : QAbstractListModel(parent) {}

int GroupModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : int(m_entries.size()); }

QVariant GroupModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) return {};
    const auto &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole: return entry.name;
    case DisplayNameRole: return entry.displayName;
    case TypeRole: return entry.type;
    case NowRole: return entry.now;
    case RouteRole: return entry.route;
    case ExpandedRole: return entry.expanded;
    case CountRole: return entry.nodes ? entry.nodes->rowCount() : 0;
    case NodesRole: return QVariant::fromValue(static_cast<QObject *>(entry.nodes));
    case AutomaticRole: return entry.automatic;
    case TestableRole: return entry.testable;
    default: return {};
    }
}

QHash<int, QByteArray> GroupModel::roleNames() const
{
    return {{NameRole, "name"}, {DisplayNameRole, "displayName"}, {TypeRole, "type"}, {NowRole, "now"},
            {RouteRole, "route"}, {ExpandedRole, "expanded"}, {CountRole, "count"}, {NodesRole, "nodes"},
            {AutomaticRole, "automatic"}, {TestableRole, "testable"}};
}

// The model owns the per-row node models: they are reparented on adoption and
// dropped when a row disappears or is replaced.
void GroupModel::reset(QVector<Entry> entries)
{
    beginResetModel();
    for (const auto &entry : std::as_const(m_entries)) delete entry.nodes;
    m_entries = std::move(entries);
    for (auto &entry : m_entries) if (entry.nodes && entry.nodes->parent() != this) entry.nodes->setParent(this);
    endResetModel();
    emit changed();
}

void GroupModel::updateRow(int row, const Entry &entry)
{
    if (row < 0 || row >= m_entries.size()) return;
    const auto &old = m_entries.at(row);
    if (old.name == entry.name && old.displayName == entry.displayName && old.type == entry.type
        && old.now == entry.now && old.route == entry.route && old.expanded == entry.expanded
        && old.automatic == entry.automatic && old.testable == entry.testable
        && old.nodes == entry.nodes)
        return;
    auto *replaced = m_entries.at(row).nodes;
    m_entries[row] = entry;
    auto *adopted = m_entries.at(row).nodes;
    if (adopted && adopted->parent() != this) adopted->setParent(this);
    if (replaced && replaced != adopted) delete replaced;
    emit dataChanged(index(row, 0), index(row, 0));
    emit changed();
}

int GroupModel::rowOf(const QString &name) const
{
    for (int row = 0; row < m_entries.size(); ++row) if (m_entries.at(row).name == name) return row;
    return -1;
}

// ---------------------------------------------------------- controller ----
ProxyController::ProxyController(Api *api, Service *service, QObject *parent)
    : QObject(parent), m_api(api), m_service(service), m_groups(this), m_nodes(this)
{
    const auto settings = Store().settings();
    m_testUrl = settings.value(QStringLiteral("delayTestUrl")).toString();
    m_testTimeoutMs = qBound(1, settings.value(QStringLiteral("delayTestTimeout")).toInt(5000), 30000);
    m_testConcurrency = qBound(1, settings.value(QStringLiteral("delayTestConcurrency")).toInt(50), 256);
    if (m_service) {
        // Everything read from a controller belongs to the core instance that
        // answered it; a stopped daemon invalidates the provider map.
        connect(m_service, &Service::stateChanged, this, [this] {
            if (!m_service->running()) m_reads.clear();
        });
    }
    refresh();
}

// reloadNodeSources() + refreshProxies()/previewProfile(): the source combo is
// rebuilt from the profile store, then the selected source is (re)loaded.
void ProxyController::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    if (m_active) refresh();
}

void ProxyController::refresh()
{
    if (!m_active && !testsRunning(m_testQueue, m_activeTests)) return; // nothing shows this data
    const Store store;
    const auto previous = m_sourceIds.value(m_sourceIndex);
    QStringList labels, ids;
    labels.append(tr("自动（运行时显示实时节点）"));
    ids.append(QString());
    for (const auto &value : store.profiles()) {
        const auto profile = value.toObject();
        ids.append(profile.value(QStringLiteral("id")).toString());
        labels.append(tr("订阅预览：%1").arg(profile.value(QStringLiteral("name")).toString()));
    }
    const bool switched = ids.indexOf(previous) != m_sourceIndex;
    const bool relisted = labels != m_sources || ids != m_sourceIds;
    m_sources = labels;
    m_sourceIds = ids;
    m_sourceIndex = qMax(0, m_sourceIds.indexOf(previous));
    if (switched) {
        // Different subscription, different nodes: drop everything read from or
        // measured on the previous source.
        m_reads.clear();
        m_nodeProvider.clear();
        m_providerProxies.clear();
        clearTests();
    }
    if (relisted || switched) emit sourcesChanged();
    const auto sourceId = m_sourceIds.value(m_sourceIndex);
    if (!sourceId.isEmpty() || !m_connected) previewProfile(sourceId);
    else pollRuntime();
}

void ProxyController::setSourceIndex(int index)
{
    if (index < 0 || index >= m_sources.size() || index == m_sourceIndex) return;
    m_sourceIndex = index;
    m_reads.clear();
    m_nodeProvider.clear();
    m_providerProxies.clear();
    clearTests();
    emit sourcesChanged();
    const auto sourceId = m_sourceIds.value(index);
    if (!sourceId.isEmpty() || !m_connected) previewProfile(sourceId);
    else pollRuntime();
}

// previewProfile(): the profile YAML is read from the store only. No core start,
// no system proxy change; results are re-read only when the YAML changed.
void ProxyController::previewProfile(const QString &requested)
{
    try {
        const Store store;
        const auto profiles = store.profiles();
        QString id = requested;
        if (id.isEmpty()) id = store.settings().value(QStringLiteral("active")).toString();
        if (id.isEmpty() && !profiles.isEmpty()) id = profiles.first().toObject().value(QStringLiteral("id")).toString();
        QJsonObject profile;
        for (const auto &value : profiles) if (value.toObject().value(QStringLiteral("id")).toString() == id) { profile = value.toObject(); break; }
        if (profile.isEmpty() && !profiles.isEmpty() && requested.isEmpty()) {
            profile = profiles.first().toObject();
            id = profile.value(QStringLiteral("id")).toString();
        }
        if (profile.isEmpty()) {
            m_previewing = true;
            m_previewFingerprint.clear();
            m_proxyData = {};
            m_hint = tr("导入订阅后即可查看节点，无需启动内核或开启系统代理。");
            rebuild();
            return;
        }
        const auto bytes = store.profile(id);
        const auto fingerprint = QString::fromLatin1(id.toUtf8() + QCryptographicHash::hash(bytes, QCryptographicHash::Sha256));
        if (m_previewing && fingerprint == m_previewFingerprint) {
            rebuild();
            return;
        }
        const auto preview = subscriptionNodes(bytes);
        clearTests();
        m_nodeProvider.clear();
        m_providerProxies.clear();
        m_previewing = true;
        m_previewFingerprint = fingerprint;
        m_proxyData = preview.proxies;
        auto hint = tr("订阅预览：%1 · 启动并使用此配置后可切换节点和测速；无需开启系统代理。").arg(profile.value(QStringLiteral("name")).toString());
        if (preview.remoteProviders) hint += tr(" %1 个远程节点提供者需加载后显示。").arg(preview.remoteProviders);
        m_hint = hint;
        rebuild();
    } catch (const std::exception &error) {
        emit message(QString::fromUtf8(error.what()), true);
    }
}

// refreshProxies() runtime branch: /providers/proxies fills the node -> provider
// map once per source (kept marked in m_reads, cleared on every source change),
// /proxies is read once per refresh. The read set keeps both deduplicated while
// a request is in flight, so the 3 s refresh tick cannot stack requests.
void ProxyController::pollRuntime()
{
    if (m_reads.contains(kProxiesPath)) return;
    m_reads.insert(kProxiesPath);
    if (!m_reads.contains(kProvidersPath)) {
        m_reads.insert(kProvidersPath);
        m_api->get(kProvidersPath, [this](const QJsonValue &value, const QString &error) {
            const bool usable = error.isEmpty() && m_connected && m_sourceIds.value(m_sourceIndex).isEmpty();
            if (!usable) {
                m_reads.remove(kProvidersPath); // read again on a later refresh
                return;
            }
            m_nodeProvider.clear();
            m_providerProxies.clear();
            const auto providers = value.toObject().value(QStringLiteral("providers")).toObject();
            for (auto it = providers.constBegin(); it != providers.constEnd(); ++it)
                for (const auto &item : it.value().toObject().value(QStringLiteral("proxies")).toArray()) {
                    const auto proxy = item.toObject();
                    const auto name = proxy.value(QStringLiteral("name")).toString();
                    if (name.isEmpty()) continue;
                    m_nodeProvider.insert(name, it.key());
                    m_providerProxies.insert(name, proxy);
                    if (!m_proxyData.contains(name)) m_proxyData.insert(name, proxy);
                }
            const auto fingerprint = displayFingerprint(m_proxyData);
            if (fingerprint == m_runtimeFingerprint) return;
            m_runtimeFingerprint = fingerprint;
            rebuild();
        });
    }
    m_api->get(kProxiesPath, [this](const QJsonValue &value, const QString &error) {
        m_reads.remove(kProxiesPath);
        if (!m_sourceIds.value(m_sourceIndex).isEmpty()) return; // switched to a subscription preview
        if (!error.isEmpty()) {
            // The controller went away: fall back to the stored profile.
            m_connected = false;
            m_reads.clear();
            m_previewing = true;
            m_previewFingerprint.clear();
            m_runtimeFingerprint.clear();
            previewProfile(QString());
            return;
        }
        m_previewing = false;
        m_previewFingerprint.clear();
        m_hint = tr("实时节点 · 测速和切换不依赖系统代理开关");
        m_proxyData = value.toObject().value(QStringLiteral("proxies")).toObject();
        for (auto it = m_providerProxies.constBegin(); it != m_providerProxies.constEnd(); ++it)
            if (!m_proxyData.contains(it.key())) m_proxyData.insert(it.key(), it.value());
        const auto fingerprint = displayFingerprint(m_proxyData);
        if (fingerprint == m_runtimeFingerprint) return; // unchanged: keep the existing models
        m_runtimeFingerprint = fingerprint;
        rebuild();
    });
}

// updateProxyGroups() + renderNodes() + renderGroups(): push the current data
// into both models. Rows are kept when the group list did not change, so the
// per-row node models are not reallocated on every 3 s tick.
void ProxyController::rebuild()
{
    const auto settings = Store().settings();
    // The refresh tick runs every 3 s. Recomputing 22 groups and 62 nodes plus
    // resetting their models would rebuild every delegate on screen, so skip
    // the pass when neither the data nor the view state changed.
    {
        QStringList expandedNames, measured;
        const auto scope = m_sourceIds.value(m_sourceIndex).isEmpty()
            ? settings.value(QStringLiteral("active")).toString() : m_sourceIds.value(m_sourceIndex);
        for (const auto &value : settings.value(QStringLiteral("expandedProxyGroups")).toObject()
                                              .value(scope).toArray())
            expandedNames.append(value.toString());
        for (auto it = m_testResults.constBegin(); it != m_testResults.constEnd(); ++it)
            measured.append(it.key() % QLatin1Char(':') % QString::number(it.value().second)
                            % QLatin1Char(':') % m_testErrors.value(it.key()));
        QStringList testing = m_testing.values();
        testing.sort();
        const QByteArray signature = displayFingerprint(m_proxyData) % '|' % m_search.toUtf8() % '|'
            % QByteArray::number(m_sortIndex) % (m_hideUnavailable ? "1" : "0")
            % (m_previewing ? "1" : "0") % QByteArray::number(testable() ? 1 : 0)
            % m_currentGroup.toUtf8() % expandedNames.join(QLatin1Char(',')).toUtf8()
            % testing.join(QLatin1Char(',')).toUtf8() % measured.join(QLatin1Char(',')).toUtf8();
        if (signature == m_rebuildSignature) return;
        m_rebuildSignature = signature;
    }
    const auto sourceId = m_sourceIds.value(m_sourceIndex);
    const auto scope = sourceId.isEmpty() ? settings.value(QStringLiteral("active")).toString() : sourceId;
    const auto savedGroups = settings.value(QStringLiteral("expandedProxyGroups")).toObject();
    const bool saved = savedGroups.contains(scope);
    QSet<QString> expanded;
    for (const auto &value : savedGroups.value(scope).toArray()) expanded.insert(value.toString());

    NodeView view;
    view.search = m_search;
    view.sortIndex = m_sortIndex;
    view.hideUnavailable = m_hideUnavailable;
    view.previewing = m_previewing;
    const bool operable = testable() && !testsRunning(m_testQueue, m_activeTests);

    const auto names = proxyGroupNames(m_proxyData);
    bool sameRows = m_groups.rowCount() == names.size();
    for (int row = 0; sameRows && row < names.size(); ++row) sameRows = m_groups.rowOf(names.at(row)) == row;

    QVector<GroupModel::Entry> rows;
    rows.reserve(names.size());
    for (int row = 0; row < names.size(); ++row) {
        const auto name = names.at(row);
        const auto info = m_proxyData.value(name).toObject();
        GroupModel::Entry entry;
        entry.name = name;
        entry.displayName = info.value(QStringLiteral("displayName")).toString(nodeDisplayName(name));
        entry.type = info.value(QStringLiteral("type")).toString();
        entry.now = m_previewing ? QString() : nodeDisplayName(info.value(QStringLiteral("now")).toString());
        entry.route = proxyGroupRoute(m_proxyData, name);
        entry.expanded = saved ? expanded.contains(name) : row == 0;
        entry.automatic = entry.type == QStringLiteral("URLTest") || entry.type == QStringLiteral("Fallback");
        entry.testable = operable;
        entry.nodes = sameRows ? rowNodes(m_groups, row) : new NodeModel;
        entry.nodes->reset(nodeEntries(m_proxyData, name, view, m_testing, m_testResults, m_testErrors));
        rows.append(entry);
    }
    if (sameRows) for (int row = 0; row < rows.size(); ++row) m_groups.updateRow(row, rows.at(row));
    else m_groups.reset(rows);

    if (m_currentGroup.isEmpty() || !names.contains(m_currentGroup)) {
        const auto first = names.value(0);
        if (first != m_currentGroup) m_currentGroup = first;
    }
    m_nodes.reset(nodeEntries(m_proxyData, m_currentGroup, view, m_testing, m_testResults, m_testErrors));
    emit changed();
}

void ProxyController::setCurrentGroup(const QString &group)
{
    if (group == m_currentGroup) return;
    if (!group.isEmpty() && !m_proxyData.contains(group)) return;
    m_currentGroup = group;
    NodeView view;
    view.search = m_search;
    view.sortIndex = m_sortIndex;
    view.hideUnavailable = m_hideUnavailable;
    view.previewing = m_previewing;
    m_nodes.reset(nodeEntries(m_proxyData, m_currentGroup, view, m_testing, m_testResults, m_testErrors));
    emit changed();
}

void ProxyController::setSearch(const QString &search)
{
    if (search == m_search) return;
    m_search = search;
    rebuild();
    emit filtersChanged();
}

void ProxyController::setSortIndex(int index)
{
    index = qBound(0, index, 2);
    if (index == m_sortIndex) return;
    m_sortIndex = index;
    rebuild();
    emit filtersChanged();
}

void ProxyController::setHideUnavailable(bool hide)
{
    if (hide == m_hideUnavailable) return;
    m_hideUnavailable = hide;
    rebuild();
    emit filtersChanged();
}

void ProxyController::setTestUrl(const QString &url)
{
    if (url == m_testUrl) return;
    m_testUrl = url;
    emit testSettingsChanged();
}

void ProxyController::setTestTimeout(int ms)
{
    ms = qBound(1, ms, 30000);
    if (ms == m_testTimeoutMs) return;
    m_testTimeoutMs = ms;
    emit testSettingsChanged();
}

void ProxyController::setTestConcurrency(int concurrency)
{
    concurrency = qBound(1, concurrency, 256);
    if (concurrency == m_testConcurrency) return;
    m_testConcurrency = concurrency;
    emit testSettingsChanged();
}

void ProxyController::setApiConnected(bool connected)
{
    if (m_connected == connected) return;
    m_connected = connected;
    if (!connected) {
        // Everything cached came from a controller that no longer answers.
        m_reads.clear();
        m_nodeProvider.clear();
        m_providerProxies.clear();
        clearTests();
        m_previewing = true;
        m_previewFingerprint.clear();
        m_runtimeFingerprint.clear();
        previewProfile(m_sourceIds.value(m_sourceIndex));
    } else {
        refresh();
    }
    emit changed();
}

// Latency results and errors belong to one core instance: drop them all. The
// rows are re-read here because the caller may only know that the controller
// changed, not when the next refresh tick lands.
void ProxyController::clearTests()
{
    ++m_testEpoch;
    m_testing.clear();
    m_testQueue.clear();
    m_activeTests = 0;
    m_testResults.clear();
    m_testErrors.clear();
    m_testSummary.clear();
    rebuild();
}

bool ProxyController::testable() const { return m_connected && !m_previewing; }

QString ProxyController::currentSourceId() const { return m_sourceIds.value(m_sourceIndex); }

// websiteRulePolicy(): the rule the website helper may attach to. Subscription
// preview groups are not real policies in the running configuration.
QString ProxyController::websiteRulePolicy(const QString &mode) const
{
    if (mode == QStringLiteral("direct")) return QStringLiteral("DIRECT");
    if (!m_previewing && mode == QStringLiteral("global")) return QStringLiteral("GLOBAL");
    if (!m_currentGroup.isEmpty() && m_proxyData.value(m_currentGroup).toObject().value(QStringLiteral("type")).toString() != QStringLiteral("Subscription"))
        return m_currentGroup;
    for (const auto &name : proxyGroupNames(m_proxyData))
        if (name != QStringLiteral("GLOBAL") && m_proxyData.value(name).toObject().value(QStringLiteral("type")).toString() != QStringLiteral("Subscription"))
            return name;
    return QStringLiteral("DIRECT");
}

// The group accordion persists its open rows per data source, like the cards of
// the widget page did; opening a group also selects it for the node list.
void ProxyController::setExpanded(const QString &group, bool expanded)
{
    if (m_groups.rowOf(group) < 0) return;
    if (expanded && group != m_currentGroup) setCurrentGroup(group);
    Store store;
    auto settings = store.settings();
    auto groups = settings.value(QStringLiteral("expandedProxyGroups")).toObject();
    const auto sourceId = m_sourceIds.value(m_sourceIndex);
    const auto scope = sourceId.isEmpty() ? settings.value(QStringLiteral("active")).toString() : sourceId;
    QStringList saved;
    for (const auto &value : groups.value(scope).toArray()) saved.append(value.toString());
    if (expanded) {
        if (!saved.contains(group)) saved.append(group);
    } else {
        saved.removeAll(group);
    }
    groups[scope] = QJsonArray::fromStringList(saved);
    settings[QStringLiteral("expandedProxyGroups")] = groups;
    store.setSettings(settings);
    rebuild();
}

// chooseNode(): the widget page refused switching while previewing, ignored the
// types without a manual selection, and kept a single switch in flight.
void ProxyController::selectNode(const QString &group, const QString &node)
{
    setCurrentGroup(group);
    if (m_previewing) {
        emit message(tr("当前为订阅预览，启动并使用该配置后可切换节点；无需开启系统代理。"), false);
        return;
    }
    if (!m_connected || m_selecting) return;
    const auto info = m_proxyData.value(group).toObject();
    if (info.value(QStringLiteral("type")).toString() == QStringLiteral("Relay")
        || info.value(QStringLiteral("type")).toString() == QStringLiteral("LoadBalance")
        || !info.value(QStringLiteral("all")).isArray()
        || info.value(QStringLiteral("now")).toString() == node) return;
    m_selecting = true;
    m_api->request("PUT", QStringLiteral("/proxies/") + Api::segment(group), QJsonObject{{QStringLiteral("name"), node}},
        [this, group, node](const QJsonValue &, const QString &error) {
            m_selecting = false;
            if (!error.isEmpty()) {
                emit message(tr("节点切换失败：%1").arg(error), true);
                rebuild();
                return;
            }
            emit message(tr("已切换 %1 → %2").arg(group, node), false);
            refresh();
        });
}

void ProxyController::restoreAutomatic(const QString &group)
{
    if (m_previewing || !m_connected || m_selecting) return;
    if (group.isEmpty()) return;
    const auto type = m_proxyData.value(group).toObject().value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("Relay") || type == QStringLiteral("LoadBalance")) return;
    m_selecting = true;
    m_api->request("DELETE", QStringLiteral("/proxies/") + Api::segment(group), {},
        [this](const QJsonValue &, const QString &error) {
            m_selecting = false;
            if (!error.isEmpty()) emit message(error, true);
            else refresh();
        });
}

// testNodes(): the per-node test URL is the page setting, then the group's own
// url, then the running runtime configuration's group url, then the default.
// Settings are persisted exactly like the widget page did before a run starts.
void ProxyController::testGroup(const QString &group) { testNode(group, QString()); }

void ProxyController::testNode(const QString &group, const QString &node)
{
    if (m_previewing) {
        emit message(tr("当前为订阅预览，启动并使用该配置后可测速；无需开启系统代理。"), false);
        return;
    }
    if (!m_connected || testsRunning(m_testQueue, m_activeTests)) return;
    const auto policy = group.isEmpty() ? m_currentGroup : group;
    auto target = m_testUrl.trimmed();
    if (target.isEmpty()) target = m_proxyData.value(policy).toObject().value(QStringLiteral("testUrl")).toString();
    if (target.isEmpty()) {
        try {
            const Store store;
            const auto runtime = parseConfig(store.runtime(store.settings().value(QStringLiteral("active")).toString()));
            if (runtime["proxy-groups"]) for (const auto &entry : runtime["proxy-groups"])
                if (entry["name"] && entry["url"] && QString::fromStdString(entry["name"].as<std::string>()) == policy)
                    target = QString::fromStdString(entry["url"].as<std::string>());
        } catch (const std::exception &) {
            // A broken profile only costs the fallback address.
        }
    }
    if (target.isEmpty()) target = kDefaultTestUrl;
    const QUrl testTarget(target);
    if (testTarget.scheme() != QStringLiteral("http") && testTarget.scheme() != QStringLiteral("https")) {
        emit message(tr("请输入 HTTP 或 HTTPS 测试地址"), true);
        return;
    }
    QStringList names;
    if (node.isEmpty()) for (const auto &value : m_proxyData.value(policy).toObject().value(QStringLiteral("all")).toArray()) names.append(value.toString());
    else names.append(node);
    names.removeDuplicates();
    if (names.isEmpty()) return;

    ++m_testEpoch;
    m_testSummary.clear();
    {
        Store store;
        auto settings = store.settings();
        settings[QStringLiteral("delayTestUrl")] = m_testUrl.trimmed();
        settings[QStringLiteral("delayTestTimeout")] = m_testTimeoutMs;
        settings[QStringLiteral("delayTestConcurrency")] = m_testConcurrency;
        store.setSettings(settings);
    }
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("url"), testTarget.toString());
    query.addQueryItem(QStringLiteral("timeout"), QString::number(m_testTimeoutMs));
    m_testQuery = query.toString(QUrl::FullyEncoded);
    m_testing.clear();
    m_testQueue.clear();
    for (const auto &name : names) {
        m_testResults.remove(name);
        m_testing.insert(name);
        m_testQueue.enqueue(name);
    }
    emit message(tr("正在测速 %1 个节点…").arg(names.size()), false);
    rebuild();
    runTests();
}

// runNodeTests(): individual requests keep the successful results when another
// node times out. The request outlives the node's own timeout so a node that
// answers late is still recorded.
void ProxyController::runTests()
{
    while (m_activeTests < m_testConcurrency && !m_testQueue.isEmpty()) {
        const auto name = m_testQueue.dequeue();
        const auto epoch = m_testEpoch;
        ++m_activeTests;
        const auto provider = m_nodeProvider.value(name);
        const auto query = QStringLiteral("?") + m_testQuery;
        const auto path = provider.isEmpty()
            ? QStringLiteral("/proxies/") + Api::segment(name) + QStringLiteral("/delay") + query
            : QStringLiteral("/providers/proxies/") + Api::segment(provider) + QLatin1Char('/') + Api::segment(name)
                + QStringLiteral("/healthcheck") + query;
        m_api->get(path, [this, name, epoch](const QJsonValue &value, const QString &error) {
            if (epoch != m_testEpoch) return; // source changed or results cleared
            --m_activeTests;
            const int delay = error.isEmpty() ? value.toObject().value(QStringLiteral("delay")).toInt() : 0;
            m_testResults[name] = qMakePair(QDateTime::currentDateTimeUtc(), delay);
            m_testErrors[name] = error;
            int passed = 0, failed = 0;
            const bool finished = !testsRunning(m_testQueue, m_activeTests);
            if (finished) {
                tally(m_testing, m_testResults, &passed, &failed);
                m_testSummary = tr("测速完成：%1 个成功，%2 个失败").arg(passed).arg(failed);
            }
            rebuild();
            if (!finished) {
                runTests();
                return;
            }
            emit message(m_testSummary, passed == 0);
            refresh();
        }, m_testTimeoutMs + kRequestGraceMs);
    }
}
} // namespace Party
