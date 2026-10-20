// SPDX-License-Identifier: LGPL-3.0-or-later
// dccData facade for the V25 control center plugin (module name: dccproxy).
// The QML pages talk to dccData only; no QWidget code is involved.
#pragma once
#include "api.h"
#include "config.h"
#include <QAbstractItemModel>
#include <QAbstractListModel>
#include <QAbstractTableModel>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QQueue>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <functional>

class QNetworkAccessManager;
class QTimer;

namespace Party {
class ProxyPlugin;

// ---------------------------------------------------------------- service ----
// Owns the dde-mihomo-daemon lifecycle through systemd --user and forwards
// configuration commands over the daemon's local socket.
class Service : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(bool root READ root NOTIFY stateChanged)
    Q_PROPERTY(QString profile READ profile NOTIFY stateChanged)
    Q_PROPERTY(bool unitAvailable READ unitAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool unitEnabled READ unitEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool tunEnabled READ tunEnabled NOTIFY stateChanged)
    Q_PROPERTY(QString tunDevice READ tunDevice NOTIFY stateChanged)
    Q_PROPERTY(bool tunAuthorized READ tunAuthorized NOTIFY stateChanged)
    Q_PROPERTY(bool tunProcessAuthorized READ tunProcessAuthorized NOTIFY stateChanged)
public:
    explicit Service(QObject *parent = nullptr);
    bool running() const { return m_running; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    bool root() const { return m_root; }
    QString profile() const { return m_profile; }
    bool unitAvailable() const { return m_unitAvailable; }
    bool unitEnabled() const { return m_unitEnabled; }
    bool tunEnabled() const { return m_tunEnabled; }
    QString tunDevice() const { return m_tunDevice; }
    bool tunAuthorized() const { return m_tunAuthorized; }
    bool tunProcessAuthorized() const { return m_tunProcessAuthorized; }

    // Lifecycle through systemd --user (never starts the binary directly).
    Q_INVOKABLE void start(const QString &profile = {});
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restart(const QString &profile = {});
    Q_INVOKABLE void setEnabled(bool enabled); // systemctl --user enable/disable
    Q_INVOKABLE void refresh();                // re-read unit state + daemon status
    // Configuration commands answered by the daemon.
    Q_INVOKABLE void setTun(const QVariantMap &options);
    Q_INVOKABLE void repairTun();
    Q_INVOKABLE void setPorts(const QVariantMap &ports);
    Q_INVOKABLE void setBypass(const QStringList &entries);
    Q_INVOKABLE void setSystemProxy(const QString &mode, int port); // manual|auto|none
    Q_INVOKABLE void authorizeCore(); // re-apply root:root 4755 to bundled core

    bool daemonReachable() const { return m_reachable; }
    // Raw command channel, used by the controllers for start/stop/ports/tun.
    void request(const QJsonObject &command, std::function<void(const QJsonValue &, const QString &)> done);

signals:
    void stateChanged();
    void message(const QString &text, bool error);
    void tunChanged();

private:
    void applyStatus(const QJsonObject &state);
    void runSystemctl(const QStringList &arguments, std::function<void(bool, const QString &)> done);
    bool m_running = false, m_busy = false, m_root = false, m_reachable = false;
    bool m_unitAvailable = false, m_unitEnabled = false, m_tunEnabled = false, m_tunAuthorized = false;
    bool m_tunProcessAuthorized = false;
    QString m_error, m_profile, m_tunDevice;
};

// --------------------------------------------------------------- profiles ----
class ProfileModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
    Q_PROPERTY(QString currentId READ currentId WRITE setCurrentId NOTIFY currentChanged)
public:
    enum Roles { IdRole = Qt::UserRole + 1, NameRole, UrlRole, LocalRole, UpdatedRole, ActiveRole };
    explicit ProfileModel(Service *service, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString currentId() const { return m_current; }
    void setCurrentId(const QString &id);
    QString currentName() const;
    QJsonObject current() const;
    Q_INVOKABLE void reload();

    // Import, edit and activate subscriptions. All of them report through
    // ProxyPlugin::message() supplied by the owner.
    Q_INVOKABLE void importUrl(const QString &url, const QString &name = {});
    Q_INVOKABLE void importFile(const QString &path);
    Q_INVOKABLE void createProfile(const QString &name);
    Q_INVOKABLE void saveCurrent(const QByteArray &data);
    Q_INVOKABLE void updateCurrent();
    Q_INVOKABLE void activateCurrent();
    Q_INVOKABLE void removeCurrent();
    Q_INVOKABLE void applyOptions(const QString &id, const QVariantMap &options);
    Q_INVOKABLE QString optionsJson(const QString &id) const;
    Q_INVOKABLE QString document(const QString &id) const;

signals:
    void changed();
    void currentChanged();
    void saved(const QString &id);
    void message(const QString &text, bool error);

private:
    Service *m_service = nullptr;
    QJsonArray m_profiles;
    QString m_current;
};

// ------------------------------------------------------------------ nodes ----
class NodeModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
public:
    enum Roles { NameRole = Qt::UserRole + 1, DisplayNameRole, FlagRole, TypeRole, DelayRole,
                 DelayTextRole, TestingRole, ActiveRole, ErrorRole, AliveRole };
    explicit NodeModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString nameAt(int row) const;

    struct Entry { QString name; QString displayName; QString flag; QString type; int delay = -1;
                   bool testing = false; bool active = false; bool alive = true; QString error; };
    void reset(QVector<Entry> entries);
signals:
    void changed();
private:
    QVector<Entry> m_entries;
};

class GroupModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
public:
    enum Roles { NameRole = Qt::UserRole + 1, DisplayNameRole, TypeRole, NowRole, RouteRole,
                 ExpandedRole, CountRole, NodesRole, AutomaticRole, TestableRole };
    explicit GroupModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    struct Entry { QString name; QString displayName; QString type; QString now; QString route;
                   bool expanded = false; bool automatic = false; bool testable = false; NodeModel *nodes = nullptr; };
    void reset(QVector<Entry> entries);
    void updateRow(int row, const Entry &entry);
    int rowOf(const QString &name) const;
signals:
    void changed();
private:
    QVector<Entry> m_entries;
};

// --------------------------------------------------- proxies (policies) ----
class ProxyController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(int sourceIndex READ sourceIndex WRITE setSourceIndex NOTIFY sourcesChanged)
    Q_PROPERTY(GroupModel *groups READ groups CONSTANT)
    Q_PROPERTY(bool previewing READ previewing NOTIFY changed)
    Q_PROPERTY(QString sourceHint READ sourceHint NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString currentGroup READ currentGroup WRITE setCurrentGroup NOTIFY changed)
    Q_PROPERTY(NodeModel *nodes READ nodes CONSTANT)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filtersChanged)
    Q_PROPERTY(int sortIndex READ sortIndex WRITE setSortIndex NOTIFY filtersChanged)
    Q_PROPERTY(bool hideUnavailable READ hideUnavailable WRITE setHideUnavailable NOTIFY filtersChanged)
    Q_PROPERTY(QString testUrl READ testUrl WRITE setTestUrl NOTIFY testSettingsChanged)
    Q_PROPERTY(int testTimeout READ testTimeout WRITE setTestTimeout NOTIFY testSettingsChanged)
    Q_PROPERTY(int testConcurrency READ testConcurrency WRITE setTestConcurrency NOTIFY testSettingsChanged)
    Q_PROPERTY(QString testSummary READ testSummary NOTIFY changed)
public:
    ProxyController(Api *api, Service *service, QObject *parent = nullptr);
    QStringList sources() const { return m_sources; }
    int sourceIndex() const { return m_sourceIndex; }
    void setSourceIndex(int index);
    GroupModel *groups() { return &m_groups; }
    bool previewing() const { return m_previewing; }
    QString sourceHint() const { return m_hint; }
    bool connected() const { return m_connected; }
    QString currentGroup() const { return m_currentGroup; }
    void setCurrentGroup(const QString &group);
    NodeModel *nodes() { return &m_nodes; }
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    int sortIndex() const { return m_sortIndex; }
    void setSortIndex(int index);
    bool hideUnavailable() const { return m_hideUnavailable; }
    void setHideUnavailable(bool hide);
    QString testUrl() const { return m_testUrl; }
    void setTestUrl(const QString &url);
    int testTimeout() const { return m_testTimeoutMs; }
    void setTestTimeout(int ms);
    int testConcurrency() const { return m_testConcurrency; }
    void setTestConcurrency(int concurrency);
    QString testSummary() const { return m_testSummary; }

    Q_INVOKABLE void refresh();                    // poll controller or rebuild preview
    Q_INVOKABLE void setExpanded(const QString &group, bool expanded);
    Q_INVOKABLE void selectNode(const QString &group, const QString &node);
    Q_INVOKABLE void restoreAutomatic(const QString &group);
    Q_INVOKABLE void testGroup(const QString &group);
    Q_INVOKABLE void testNode(const QString &group, const QString &node);
    Q_INVOKABLE bool testable() const;             // runtime node data usable
    // Runtime polling only runs while the pages that show it are on screen.
    Q_INVOKABLE void setActive(bool active);
    bool active() const { return m_active; }
    // Policy used when adding a website rule: DIRECT in direct mode, GLOBAL in
    // global mode while connected, otherwise the current (or first) real group.
    Q_INVOKABLE QString websiteRulePolicy(const QString &mode) const;
    Q_INVOKABLE QString currentSourceId() const;   // subscription id of the selected source, empty for the live view
    void setApiConnected(bool connected);
    void clearTests();

signals:
    void changed();
    void sourcesChanged();
    void filtersChanged();
    void testSettingsChanged();
    void message(const QString &text, bool error);

private:
    void rebuild();      // applies current data to the models
    void previewProfile(const QString &id);
    void pollRuntime();
    void runTests();
    Api *m_api = nullptr;
    Service *m_service = nullptr;
    GroupModel m_groups{this};
    NodeModel m_nodes{this};
    QStringList m_sources;
    QStringList m_sourceIds;
    int m_sourceIndex = 0;
    QString m_currentGroup;
    QString m_hint, m_search, m_testUrl, m_testSummary;
    int m_sortIndex = 0, m_testConcurrency = 50;
    bool m_previewing = true, m_connected = false, m_selecting = false, m_hideUnavailable = false;
    bool m_active = false;
    QJsonObject m_proxyData;
    QMap<QString, QJsonObject> m_providerProxies;
    QMap<QString, QString> m_nodeProvider;
    QString m_previewFingerprint;
    QByteArray m_runtimeFingerprint;
    QByteArray m_rebuildSignature;
    QSet<QString> m_reads, m_testing;
    QQueue<QString> m_testQueue;
    QMap<QString, QPair<QDateTime, int>> m_testResults;
    QMap<QString, QString> m_testErrors;
    QString m_testQuery;
    int m_activeTests = 0, m_testTimeoutMs = 5000;
    quint64 m_testEpoch = 0;
};

// --------------------------------------------------- runtime data models ----
class ConnectionModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY changed)
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
public:
    enum Roles { IdRole = Qt::UserRole + 1, TargetRole, ProcessRole, KindRole, RuleRole, ChainRole,
                 UploadRole, DownloadRole, UploadTextRole, DownloadTextRole };
    explicit ConnectionModel(Api *api, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE void closeAt(int row);
    Q_INVOKABLE void closeAll();
signals:
    void changed();
    void message(const QString &text, bool error);
private:
    Api *m_api = nullptr;
    QJsonArray m_items;
    QString m_search;
};

class RuleModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY changed)
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
public:
    enum Roles { TypeRole = Qt::UserRole + 1, PayloadRole, ProxyRole, DisabledRole, IndexRole };
    explicit RuleModel(Api *api, Service *service, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setDisabled(int row, bool disabled);
    // Website rules are prepended to the configuration of one subscription;
    // `profileId` empty means the active subscription, then the first one.
    Q_INVOKABLE QString websiteRulePreview(const QString &input, const QString &policy) const;
    Q_INVOKABLE void addWebsiteRule(const QString &input, const QString &policy, const QString &profileId = {});
signals:
    void changed();
    void message(const QString &text, bool error);
private:
    Api *m_api = nullptr;
    Service *m_service = nullptr;
    QJsonArray m_items;
    QString m_search;
};

class ProviderModel : public QAbstractTableModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
public:
    enum Roles { KindRole = Qt::UserRole + 1, NameRole, VehicleRole, UpdatedRole, EntriesRole, HealthRole };
    explicit ProviderModel(Api *api, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void updateAt(int row);
    Q_INVOKABLE void healthCheckAt(int row);
    Q_INVOKABLE void updateGeo();
signals:
    void changed();
    void message(const QString &text, bool error);
private:
    Api *m_api = nullptr;
    QJsonArray m_items;
};

class LogModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY changed)
    Q_PROPERTY(QString level READ level WRITE setLevel NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    enum Roles { TextRole = Qt::UserRole + 1 };
    explicit LogModel(Api *api, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool paused() const { return m_paused; }
    void setPaused(bool paused);
    QString level() const { return m_level; }
    void setLevel(const QString &level);
    QString status() const { return m_status; }
    Q_INVOKABLE void activate();   // page became visible: load history + stream
    Q_INVOKABLE void deactivate(); // page left: stop streaming
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool exportTo(const QString &path);
    Q_INVOKABLE QString daemonLogPath() const;
    Q_INVOKABLE QString daemonLogTail() const;
signals:
    void changed();
    void message(const QString &text, bool error);
private:
    void syncStreams();
    Api *m_api = nullptr;
    QStringList m_lines;
    QString m_level = "info", m_status;
    bool m_paused = false, m_active = false, m_historyLoaded = false;
};

// ------------------------------------------------------------- documents ----
// One editable document: subscription YAML, runtime configuration, overrides.
class DocumentController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY changed)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    Q_PROPERTY(QString help READ help WRITE setHelp NOTIFY changed)
    Q_PROPERTY(QString example READ example WRITE setExample NOTIFY changed)
    Q_PROPERTY(bool editable READ editable WRITE setEditable NOTIFY changed)
public:
    using QObject::QObject;
    QString title() const { return m_title; }
    void setTitle(const QString &title) { m_title = title; emit changed(); }
    QString text() const { return m_text; }
    void setText(const QString &text) { m_text = text; emit changed(); }
    QString help() const { return m_help; }
    void setHelp(const QString &help) { m_help = help; emit changed(); }
    QString example() const { return m_example; }
    void setExample(const QString &example) { m_example = example; emit changed(); }
    bool editable() const { return m_editable; }
    void setEditable(bool editable) { m_editable = editable; emit changed(); }
    Q_INVOKABLE void fillExample() { if (!m_example.isEmpty()) setText(m_example); }
signals:
    void changed();
    void accepted();
private:
    QString m_title, m_text, m_help, m_example;
    bool m_editable = false;
};

// -------------------------------------------------------------- settings ----
class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY changed)
    Q_PROPERTY(QString secret READ secret WRITE setSecret NOTIFY changed)
    Q_PROPERTY(QString corePath READ corePath WRITE setCorePath NOTIFY changed)
    Q_PROPERTY(bool unifiedDelay READ unifiedDelay WRITE setUnifiedDelay NOTIFY changed)
    Q_PROPERTY(bool autoSystemProxy READ autoSystemProxy WRITE setAutoSystemProxy NOTIFY changed)
    Q_PROPERTY(bool systemNodeDns READ systemNodeDns WRITE setSystemNodeDns NOTIFY changed)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY changed)
    Q_PROPERTY(int updateMinutes READ updateMinutes WRITE setUpdateMinutes NOTIFY changed)
    Q_PROPERTY(int mixedPort READ mixedPort WRITE setMixedPort NOTIFY changed)
    Q_PROPERTY(int httpPort READ httpPort WRITE setHttpPort NOTIFY changed)
    Q_PROPERTY(int socksPort READ socksPort WRITE setSocksPort NOTIFY changed)
    Q_PROPERTY(int redirPort READ redirPort WRITE setRedirPort NOTIFY changed)
    Q_PROPERTY(int tproxyPort READ tproxyPort WRITE setTproxyPort NOTIFY changed)
    Q_PROPERTY(QString bypass READ bypass WRITE setBypass NOTIFY bypassEdited)
    Q_PROPERTY(QString bypassError READ bypassError NOTIFY bypassEdited)
    Q_PROPERTY(bool bypassValid READ bypassValid NOTIFY bypassEdited)
    Q_PROPERTY(Party::DocumentController *document READ document CONSTANT)
public:
    explicit SettingsController(Service *service, QObject *parent = nullptr);
    QString endpoint() const { return m_endpoint; }
    void setEndpoint(const QString &endpoint);
    QString secret() const { return m_secret; }
    void setSecret(const QString &secret);
    QString corePath() const { return m_corePath; }
    void setCorePath(const QString &path);
    bool unifiedDelay() const { return m_unifiedDelay; }
    void setUnifiedDelay(bool on);
    bool autoSystemProxy() const { return m_autoSystemProxy; }
    void setAutoSystemProxy(bool on);
    bool systemNodeDns() const { return m_systemNodeDns; }
    void setSystemNodeDns(bool on) { m_systemNodeDns = on; emit changed(); }
    bool autoStart() const { return m_autoStart; }
    void setAutoStart(bool on);
    int updateMinutes() const { return m_updateMinutes; }
    void setUpdateMinutes(int minutes);
    int mixedPort() const { return m_ports["mixed-port"].toInt(); }
    void setMixedPort(int port);
    int httpPort() const { return m_ports["port"].toInt(); }
    void setHttpPort(int port);
    int socksPort() const { return m_ports["socks-port"].toInt(); }
    void setSocksPort(int port);
    int redirPort() const { return m_ports["redir-port"].toInt(); }
    void setRedirPort(int port);
    int tproxyPort() const { return m_ports["tproxy-port"].toInt(); }
    void setTproxyPort(int port);
    QString bypass() const { return m_bypass; }
    void setBypass(const QString &text);
    QString bypassError() const { return m_bypassError; }
    bool bypassValid() const { return m_bypassError.isEmpty(); }

    Q_INVOKABLE void load();          // read settings + controlled.yaml
    Q_INVOKABLE void apply();         // 保存并连接: validate endpoint, save, systemd enable
    Q_INVOKABLE void applyPorts();    // 保存并应用端口
    Q_INVOKABLE void applyBypass();   // 保存并应用绕过
    Q_INVOKABLE void fillDefaultBypass();
    Q_INVOKABLE void setMode(const QString &mode);       // rule|global|direct
    Q_INVOKABLE void editSection(const QString &section); // dns|sniffer|hosts|listeners|tunnels
    Q_INVOKABLE void saveSection(const QString &section, const QString &text);
    Q_INVOKABLE void showRuntimeConfig();  // 运行配置 (controller /configs)
    Q_INVOKABLE void showRuntimeProfile(); // 生效配置 (work/config.yaml)
    Q_INVOKABLE void backupTo(const QString &path);
    Q_INVOKABLE void restoreFrom(const QString &path);
    Q_INVOKABLE void webdav(const QString &url, const QString &user, const QString &password, bool upload);
    Q_INVOKABLE void chooseCore(const QString &path);
    Q_INVOKABLE void notifyBypass();  // re-validate after QML edits the text

    DocumentController *document() { return &m_document; }

signals:
    void changed();
    void bypassEdited();
    void message(const QString &text, bool error);
    void systemProxyChanged();

private:
    Service *m_service = nullptr;
    DocumentController m_document{this};
    QString m_endpoint, m_secret, m_corePath, m_bypass, m_bypassError;
    QVariantMap m_ports;
    int m_updateMinutes = 360;
    bool m_unifiedDelay = true, m_autoSystemProxy = true, m_systemNodeDns = true, m_autoStart = false;
};

// ------------------------------------------------------------- overrides ----
class OverrideController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool yamlEnabled READ yamlEnabled WRITE setYamlEnabled NOTIFY changed)
    Q_PROPERTY(bool scriptEnabled READ scriptEnabled WRITE setScriptEnabled NOTIFY changed)
    Q_PROPERTY(QString yaml READ yaml WRITE setYaml NOTIFY changed)
    Q_PROPERTY(QString script READ script WRITE setScript NOTIFY changed)
public:
    explicit OverrideController(Service *service, QObject *parent = nullptr);
    bool yamlEnabled() const { return m_yamlEnabled; }
    void setYamlEnabled(bool on) { m_yamlEnabled = on; emit changed(); }
    bool scriptEnabled() const { return m_scriptEnabled; }
    void setScriptEnabled(bool on) { m_scriptEnabled = on; emit changed(); }
    QString yaml() const { return m_yaml; }
    void setYaml(const QString &yaml) { m_yaml = yaml; emit changed(); }
    QString script() const { return m_script; }
    void setScript(const QString &script) { m_script = script; emit changed(); }
    Q_INVOKABLE void load();
    Q_INVOKABLE void save();
    Q_INVOKABLE void fillExample();
signals:
    void changed();
    void message(const QString &text, bool error);
private:
    Service *m_service = nullptr;
    QString m_yaml, m_script;
    bool m_yamlEnabled = true, m_scriptEnabled = false;
};

// ------------------------------------------------------------------- tun ----
class TunController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY edited)
    Q_PROPERTY(QString stack READ stack WRITE setStack NOTIFY edited)
    Q_PROPERTY(int mtu READ mtu WRITE setMtu NOTIFY edited)
    Q_PROPERTY(QString dnsHijack READ dnsHijack WRITE setDnsHijack NOTIFY edited)
    Q_PROPERTY(QString routeExclude READ routeExclude WRITE setRouteExclude NOTIFY edited)
    Q_PROPERTY(bool autoRoute READ autoRoute WRITE setAutoRoute NOTIFY edited)
    Q_PROPERTY(bool autoRedirect READ autoRedirect WRITE setAutoRedirect NOTIFY edited)
    Q_PROPERTY(bool autoDetectInterface READ autoDetectInterface WRITE setAutoDetectInterface NOTIFY edited)
    Q_PROPERTY(bool strictRoute READ strictRoute WRITE setStrictRoute NOTIFY edited)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit TunController(Service *service, QObject *parent = nullptr);
    QString device() const { return m_device; }
    void setDevice(const QString &device) { m_device = device; emit edited(); }
    QString stack() const { return m_stack; }
    void setStack(const QString &stack) { m_stack = stack; emit edited(); }
    int mtu() const { return m_mtu; }
    void setMtu(int mtu) { m_mtu = mtu; emit edited(); }
    QString dnsHijack() const { return m_dnsHijack; }
    void setDnsHijack(const QString &value) { m_dnsHijack = value; emit edited(); }
    QString routeExclude() const { return m_routeExclude; }
    void setRouteExclude(const QString &value) { m_routeExclude = value; emit edited(); }
    bool autoRoute() const { return m_autoRoute; }
    void setAutoRoute(bool on) { m_autoRoute = on; emit edited(); }
    bool autoRedirect() const { return m_autoRedirect; }
    void setAutoRedirect(bool on) { m_autoRedirect = on; emit edited(); }
    bool autoDetectInterface() const { return m_autoDetectInterface; }
    void setAutoDetectInterface(bool on) { m_autoDetectInterface = on; emit edited(); }
    bool strictRoute() const { return m_strictRoute; }
    void setStrictRoute(bool on) { m_strictRoute = on; emit edited(); }
    QString status() const { return m_status; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void load();
    Q_INVOKABLE void apply();          // 保存并应用
    Q_INVOKABLE void setEnabled(bool enabled);
    Q_INVOKABLE void authorize();      // 内核授权
signals:
    void changed();
    void edited();
    void message(const QString &text, bool error);
private:
    void applyOptions(const QVariantMap &options);
    void refreshStatus();
    Service *m_service = nullptr;
    QString m_device = "Mihomo", m_stack = "mixed", m_dnsHijack = "any:53", m_routeExclude, m_status;
    int m_mtu = 1500;
    bool m_autoRoute = true, m_autoRedirect = false, m_autoDetectInterface = true, m_strictRoute = false, m_busy = false;
};

// ----------------------------------------------------------------- tools ----
class ToolsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString dnsResult READ dnsResult NOTIFY changed)
public:
    explicit ToolsController(Api *api, QObject *parent = nullptr);
    QString dnsResult() const { return m_dnsResult; }
    Q_INVOKABLE void queryDns(const QString &name, const QString &type);
    Q_INVOKABLE void flushCache(const QString &kind); // dns|fakeip
signals:
    void changed();
    void message(const QString &text, bool error);
private:
    Api *m_api = nullptr;
    QString m_dnsResult;
};

// ------------------------------------------------------------- overview ----
class OverviewController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)
    Q_PROPERTY(QString kernelText READ kernelText NOTIFY changed)
    Q_PROPERTY(QString trafficText READ trafficText NOTIFY changed)
    Q_PROPERTY(QString statisticsText READ statisticsText NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
public:
    explicit OverviewController(Api *api, Service *service, QObject *parent = nullptr);
    QString stateText() const { return m_stateText; }
    QString kernelText() const { return m_kernelText; }
    QString trafficText() const { return m_trafficText; }
    QString statisticsText() const { return m_statisticsText; }
    QString mode() const { return m_mode; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();
signals:
    void changed();
    void message(const QString &text, bool error);
private:
    void updateState();
    Api *m_api = nullptr;
    Service *m_service = nullptr;
    QString m_stateText, m_kernelText, m_trafficText, m_statisticsText, m_mode = "rule";
    bool m_active = false, m_connected = false;
};

// ------------------------------------------------------------ dccData root ----
class ProxyPlugin : public QObject {
    Q_OBJECT
    Q_PROPERTY(Party::Service *service READ service CONSTANT)
    Q_PROPERTY(Party::OverviewController *overview READ overview CONSTANT)
    Q_PROPERTY(Party::ProxyController *proxies READ proxies CONSTANT)
    Q_PROPERTY(Party::ProfileModel *profiles READ profiles CONSTANT)
    Q_PROPERTY(Party::ConnectionModel *connections READ connections CONSTANT)
    Q_PROPERTY(Party::RuleModel *rules READ rules CONSTANT)
    Q_PROPERTY(Party::ProviderModel *providers READ providers CONSTANT)
    Q_PROPERTY(Party::LogModel *logs READ logs CONSTANT)
    Q_PROPERTY(Party::OverrideController *override READ override CONSTANT)
    Q_PROPERTY(Party::SettingsController *settings READ settings CONSTANT)
    Q_PROPERTY(Party::TunController *tun READ tun CONSTANT)
    Q_PROPERTY(Party::ToolsController *tools READ tools CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString activePage READ activePage NOTIFY activePageChanged)
public:
    explicit ProxyPlugin(QObject *parent = nullptr);
    Service *service() { return &m_service; }
    OverviewController *overview() { return &m_overview; }
    ProxyController *proxies() { return &m_proxies; }
    ProfileModel *profiles() { return &m_profiles; }
    ConnectionModel *connections() { return &m_connections; }
    RuleModel *rules() { return &m_rules; }
    ProviderModel *providers() { return &m_providers; }
    LogModel *logs() { return &m_logs; }
    OverrideController *override() { return &m_override; }
    SettingsController *settings() { return &m_settings; }
    TunController *tun() { return &m_tun; }
    ToolsController *tools() { return &m_tools; }
    bool connected() const { return m_connected; }
    QString version() const;

    QString activePage() const { return m_activePage; }
    // The QML page tree reports DccApp.activeObject here: the plugin idles
    // (no polling, no streams) while the control center shows another module.
    Q_INVOKABLE void setActivePage(const QString &page);

    Q_INVOKABLE QString formatSize(double bytes) const;
    Q_INVOKABLE bool writeTextFile(const QString &path, const QString &text);
    Q_INVOKABLE QString localPath(const QUrl &url) const;
    Q_INVOKABLE void refresh();       // manual refresh button
    Q_INVOKABLE void report(const QString &text, bool error); // controllers reuse the status area
    Q_INVOKABLE void openDocument(int which); // 0 = runtime profile, 1 = live config

    static QString sectionHelp(const QString &section);
    static QByteArray sectionExample(const QString &section);
    static QByteArray yamlOverrideExample();

signals:
    void message(const QString &text, bool error);
    void connectedChanged();
    void activePageChanged();

private:
    void setConnected(bool connected);
    QString m_activePage = QStringLiteral("dccproxy");
    Service m_service;
    Api m_api;
    QNetworkAccessManager *m_download = nullptr;
    OverviewController m_overview;
    ProxyController m_proxies;
    ProfileModel m_profiles;
    ConnectionModel m_connections;
    RuleModel m_rules;
    ProviderModel m_providers;
    LogModel m_logs;
    OverrideController m_override;
    SettingsController m_settings;
    TunController m_tun;
    ToolsController m_tools;
    QTimer *m_refreshTimer = nullptr;
    bool m_connected = false;
};
} // namespace Party
