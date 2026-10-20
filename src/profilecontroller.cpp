// SPDX-License-Identifier: LGPL-3.0-or-later
// Subscription profiles for the V25 control center plugin (dccData.profiles).
#include "plugindata.h"
#include "config.h"
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUrl>
#include <stdexcept>

namespace Party {
namespace {
// The facade header is frozen, so the id of the running profile - the source of
// ActiveRole and of the automatic restart after an update - is cached in a
// dynamic property instead of a data member. reload()/stateChanged() refresh it.
const char kActiveProperty[] = "dccProxyActiveProfile";
const int kGlobalUpdateMinutes = 360;

// Same starting point as the old "新建" button (Panel::editProfile(true)).
const char kNewProfileTemplate[] =
    "mixed-port: 16666\nmode: rule\nlog-level: info\nproxies: []\n"
    "proxy-groups:\n  - name: PROXY\n    type: select\n    proxies: [DIRECT]\n"
    "rules:\n  - MATCH,PROXY\n";

// Report through the model's message() signal, which ProxyPlugin relays into the
// status area. Stands in for the private helper the frozen header cannot hold.
void report(ProfileModel *model, const QString &text, bool error = false) { model->message(text, error); }

QJsonObject recordOf(const QJsonArray &list, const QString &id)
{
    for (const auto &value : list) if (value.toObject()["id"].toString() == id) return value.toObject();
    return {};
}

// Panel::fetchProfiles: download a subscription with the options of the profile
// being imported/updated, then store it and report the outcome.
void fetchProfile(ProfileModel *model, Service *service, const QUrl &url, const QString &name, const QString &id)
{
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https")) { report(model, QObject::tr("订阅 URL 无效"), true); return; }
    const Store store;
    QJsonObject options;
    for (const auto &value : store.profiles()) {
        const auto profile = value.toObject();
        if (profile["id"].toString() == id) { options = profile; break; }
    }
    const int port = localProxyPort(store);
    report(model, QObject::tr("正在下载订阅…"));
    fetchSubscription(url, options, port, model, [model, service, url, name, id](const QByteArray &bytes, const QJsonObject &metadata, const QString &error) {
        if (!error.isEmpty()) { report(model, error, true); return; }
        try {
            Store target;
            // A new subscription is named after the provider's suggestion (the
            // panel's content-disposition) or the host, like the old dialog did;
            // an update keeps the name the user already has.
            QString effectiveName = name.trimmed();
            if (effectiveName.isEmpty() && id.isEmpty()) {
                effectiveName = metadata["suggestedName"].toString().trimmed();
                if (effectiveName.isEmpty()) effectiveName = url.host();
            }
            const auto savedId = target.saveProfile(effectiveName, url.toString(), bytes, id);
            target.updateProfileOptions(savedId, metadata);
            const bool active = target.settings()["active"].toString() == savedId;
            model->reload();
            model->setCurrentId(savedId);
            report(model, id.isEmpty() ? QObject::tr("订阅已导入，可在节点页查看") : QObject::tr("订阅已更新"));
            if (active) service->restart(savedId);
        } catch (const std::exception &e) {
            report(model, QString::fromUtf8(e.what()), true);
        }
    });
}
} // namespace

ProfileModel::ProfileModel(Service *service, QObject *parent) : QAbstractListModel(parent), m_service(service)
{
    // The daemon writes settings["active"] when a start succeeds; follow it so
    // the ✓ marker moves without user interaction.
    if (m_service) connect(m_service, &Service::stateChanged, this, [this] {
        if (m_profiles.isEmpty()) return;
        const auto active = Store().settings()["active"].toString();
        if (property(kActiveProperty).toString() == active) return;
        setProperty(kActiveProperty, active);
        emit changed();
    });
    reload();
}

int ProfileModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_profiles.size();
}

QVariant ProfileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_profiles.size()) return {};
    const auto profile = m_profiles.at(index.row()).toObject();
    const auto url = profile["url"].toString();
    switch (role) {
    case IdRole: return profile["id"].toString();
    case NameRole: return profile["name"].toString();
    case UrlRole: return url.isEmpty() ? tr("本地") : url;
    case LocalRole: return url.isEmpty();
    case UpdatedRole: {
        const auto stamp = QDateTime::fromString(profile["updated"].toString(), Qt::ISODate);
        return stamp.isValid() ? stamp.toLocalTime().toString("yyyy-MM-dd HH:mm") : QString();
    }
    case ActiveRole: return profile["id"].toString() == property(kActiveProperty).toString();
    case Qt::DisplayRole: return profile["name"].toString();
    default: return {};
    }
}

QHash<int, QByteArray> ProfileModel::roleNames() const
{
    return {{IdRole, "id"}, {NameRole, "name"}, {UrlRole, "url"}, {LocalRole, "local"},
            {UpdatedRole, "updated"}, {ActiveRole, "active"}};
}

void ProfileModel::setCurrentId(const QString &id)
{
    if (m_current == id) return;
    m_current = id;
    emit currentChanged();
}

QString ProfileModel::currentName() const { return current()["name"].toString(); }

QJsonObject ProfileModel::current() const { return recordOf(m_profiles, m_current); }

void ProfileModel::reload()
{
    const Store store;
    const auto list = store.profiles();
    const auto active = store.settings()["active"].toString();
    const bool keep = !recordOf(list, m_current).isEmpty();
    const auto next = keep ? m_current : (list.isEmpty() ? QString() : list.first().toObject()["id"].toString());
    const bool currentMoved = next != m_current;
    beginResetModel();
    m_profiles = list;
    m_current = next;
    setProperty(kActiveProperty, active);
    endResetModel();
    emit changed();
    if (currentMoved) emit currentChanged();
}

void ProfileModel::importUrl(const QString &url, const QString &name)
{
    const auto text = url.trimmed();
    if (text.isEmpty()) { emit message(tr("请先输入订阅 URL"), true); return; }
    fetchProfile(this, m_service, QUrl(text), name, {});
}

void ProfileModel::importFile(const QString &path)
{
    try {
        Store store;
        const auto id = store.saveProfile(QFileInfo(path).completeBaseName(), {}, readFile(path));
        reload();
        setCurrentId(id);
        emit message(tr("配置已导入，可在节点页查看"), false);
    } catch (const std::exception &e) {
        emit message(QString::fromUtf8(e.what()), true);
    }
}

void ProfileModel::createProfile(const QString &name)
{
    if (name.trimmed().isEmpty()) return;
    try {
        Store store;
        const auto id = store.saveProfile(name, {}, QByteArray(kNewProfileTemplate));
        reload();
        setCurrentId(id);
        emit message(tr("已保存；使用此配置后生效"), false);
    } catch (const std::exception &e) {
        emit message(QString::fromUtf8(e.what()), true);
    }
}

void ProfileModel::saveCurrent(const QByteArray &data)
{
    const auto profile = current();
    if (profile.isEmpty()) { emit message(tr("请先选择一个订阅"), false); return; }
    try {
        parseConfig(data);
        Store store;
        const auto id = store.saveProfile(profile["name"].toString(), profile["url"].toString(), data, profile["id"].toString());
        reload();
        emit saved(id);
        emit message(tr("已保存；使用此配置后生效"), false);
    } catch (const std::exception &e) {
        emit message(QString::fromUtf8(e.what()), true);
    }
}

void ProfileModel::updateCurrent()
{
    const auto profile = current();
    if (profile.isEmpty()) { emit message(tr("请先选择一个订阅"), false); return; }
    if (profile["url"].toString().isEmpty()) { emit message(tr("本地配置无需更新"), false); return; }
    fetchProfile(this, m_service, QUrl(profile["url"].toString()), profile["name"].toString(), profile["id"].toString());
}

void ProfileModel::activateCurrent()
{
    const auto profile = current();
    if (profile.isEmpty() || !m_service) return;
    m_service->start(profile["id"].toString());
}

void ProfileModel::removeCurrent()
{
    const auto profile = current();
    if (profile.isEmpty()) return;
    try {
        Store store;
        store.removeProfile(profile["id"].toString());
        reload();
    } catch (const std::exception &e) {
        emit message(QString::fromUtf8(e.what()), true);
    }
}

void ProfileModel::applyOptions(const QString &id, const QVariantMap &options)
{
    const auto target = id.isEmpty() ? m_current : id;
    if (target.isEmpty()) { emit message(tr("请先选择一个订阅"), false); return; }
    const auto url = options.value("url").toString();
    if (!url.isEmpty() && QUrl(url).scheme() != "http" && QUrl(url).scheme() != "https") {
        emit message(tr("订阅 URL 无效"), true);
        return;
    }
    QJsonObject record{{"name", options.value("name").toString()},
                       {"url", url},
                       {"userAgent", options.value("userAgent").toString()},
                       {"authToken", options.value("authToken").toString()},
                       {"timeoutSeconds", options.value("timeoutSeconds", 30).toInt()},
                       {"autoUpdate", options.value("autoUpdate", true).toBool()},
                       {"useProxy", options.value("useProxy", false).toBool()}};
    const int minutes = options.value("updateMinutes").toInt();
    record["updateMinutes"] = minutes > 0 ? minutes : Store().settings()["updateMinutes"].toInt(kGlobalUpdateMinutes);
    try {
        Store store;
        store.updateProfileOptions(target, record);
        reload();
    } catch (const std::exception &e) {
        emit message(QString::fromUtf8(e.what()), true);
    }
}

QString ProfileModel::optionsJson(const QString &id) const
{
    const auto profile = recordOf(m_profiles, id.isEmpty() ? m_current : id);
    if (profile.isEmpty()) return QStringLiteral("{}");
    return QString::fromUtf8(QJsonDocument(profile).toJson(QJsonDocument::Indented));
}

QString ProfileModel::document(const QString &id) const
{
    try {
        return QString::fromUtf8(Store().profile(id.isEmpty() ? m_current : id));
    } catch (const std::exception &) {
        return {};
    }
}
} // namespace Party
