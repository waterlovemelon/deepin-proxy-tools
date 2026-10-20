// SPDX-License-Identifier: LGPL-3.0-or-later
// Contract tests for the V25 dccData facade: role names that the QML pages
// bind, user-visible formatting, and the document/YAML helpers.
#include "plugindata.h"
#include <QAbstractItemModel>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

using namespace Party;

namespace {
QString roleName(const QAbstractItemModel &model, int role)
{
    return QString::fromUtf8(model.roleNames().value(role));
}
} // namespace

class FacadeContractTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // Never touch the real user configuration directory.
        m_data = new QTemporaryDir;
        QVERIFY(m_data->isValid());
        qputenv("DCC_PROXY_DATA_DIR", m_data->path().toUtf8());
    }

    void cleanupTestCase() { delete m_data; }

    void sizeFormatting()
    {
        ProxyPlugin plugin;
        QCOMPARE(plugin.formatSize(0), QString("0 B"));
        QCOMPARE(plugin.formatSize(1023), QString("1023 B"));
        QCOMPARE(plugin.formatSize(1024), QString("1.0 KiB"));
        QCOMPARE(plugin.formatSize(1536), QString("1.5 KiB"));
        QCOMPARE(plugin.formatSize(1024.0 * 1024 * 1024), QString("1.0 GiB"));
    }

    void sectionExamplesAreValidYaml()
    {
        QVERIFY(ProxyPlugin::sectionHelp("dns").contains("DNS"));
        for (const auto &section : {"dns", "sniffer", "hosts", "listeners", "tunnels"}) {
            const auto example = ProxyPlugin::sectionExample(section);
            const auto node = parseConfig(example);
            QVERIFY2(node[section], section);
            QVERIFY(node[section].IsDefined());
        }
        QVERIFY(parseConfig(ProxyPlugin::yamlOverrideExample()).IsMap());
        QVERIFY(parseConfig(ProxyPlugin::sectionExample("tunnels"))["tunnels"].IsSequence());
    }

    void nodeModelRoles()
    {
        NodeModel model;
        NodeModel::Entry entry;
        entry.name = "🇺🇸 Example 01";
        entry.displayName = "Example 01";
        entry.type = "ss";
        entry.delay = 0;
        NodeModel::Entry testing;
        testing.name = "Example 02";
        testing.type = "vmess";
        testing.delay = -1;
        testing.testing = true;
        NodeModel::Entry healthy;
        healthy.name = "Example 03";
        healthy.type = "trojan";
        healthy.delay = 128;
        healthy.active = true;
        healthy.alive = false;
        healthy.error = "timeout";
        model.reset({entry, testing, healthy});

        QCOMPARE(model.rowCount(), 3);
        for (const auto &role : {NodeModel::NameRole, NodeModel::DisplayNameRole, NodeModel::FlagRole,
                                 NodeModel::TypeRole, NodeModel::DelayRole, NodeModel::DelayTextRole,
                                 NodeModel::TestingRole, NodeModel::ActiveRole, NodeModel::ErrorRole,
                                 NodeModel::AliveRole})
            QVERIFY(!roleName(model, role).isEmpty());
        QCOMPARE(roleName(model, NodeModel::DelayTextRole), QString("delayText"));
        QCOMPARE(roleName(model, NodeModel::DisplayNameRole), QString("displayName"));
        QCOMPARE(roleName(model, NodeModel::AliveRole), QString("alive"));

        const auto index = [&model](int row) { return model.index(row, 0); };
        QCOMPARE(model.data(index(0), NodeModel::DelayTextRole).toString(), QString("超时"));
        QCOMPARE(model.data(index(0), NodeModel::NameRole).toString(), entry.name);
        QCOMPARE(model.data(index(1), NodeModel::DelayTextRole).toString(), QString("测速中"));
        QCOMPARE(model.data(index(2), NodeModel::DelayTextRole).toString(), QString("128 ms"));
        QCOMPARE(model.data(index(2), NodeModel::ActiveRole).toBool(), true);
        QCOMPARE(model.data(index(2), NodeModel::ErrorRole).toString(), QString("timeout"));
        QCOMPARE(model.nameAt(1), QString("Example 02"));
    }

    void groupModelRoles()
    {
        GroupModel model;
        NodeModel nodes;
        GroupModel::Entry entry;
        entry.name = "PROXY";
        entry.displayName = "PROXY";
        entry.type = "Selector";
        entry.now = "Example 01";
        entry.route = "PROXY → Example 01";
        entry.automatic = true;
        entry.testable = true;
        entry.nodes = &nodes;
        model.reset({entry});

        QCOMPARE(roleName(model, GroupModel::DisplayNameRole), QString("displayName"));
        QCOMPARE(roleName(model, GroupModel::NodesRole), QString("nodes"));
        QCOMPARE(roleName(model, GroupModel::AutomaticRole), QString("automatic"));
        const auto index = model.index(0, 0);
        QCOMPARE(model.data(index, GroupModel::RouteRole).toString(), entry.route);
        QCOMPARE(model.data(index, GroupModel::AutomaticRole).toBool(), true);
        QCOMPARE(model.data(index, GroupModel::NodesRole).value<QObject *>(), static_cast<QObject *>(&nodes));
        QCOMPARE(model.rowOf("PROXY"), 0);
        QCOMPARE(model.rowOf("MISSING"), -1);

        entry.route = "PROXY → DIRECT";
        model.updateRow(0, entry);
        QCOMPARE(model.data(model.index(0, 0), GroupModel::RouteRole).toString(), QString("PROXY → DIRECT"));
    }

    void profileRolesFollowTheStore()
    {
        Service service;
        ProfileModel model(&service);
        Store store;
        const auto id = store.saveProfile("样例订阅", "https://example.test/sub.yaml", QByteArray("mixed-port: 16666\n"));
        model.reload();
        QCOMPARE(model.rowCount(), 1);
        const auto index = model.index(0, 0);
        QCOMPARE(roleName(model, ProfileModel::UpdatedRole), QString("updated"));
        QCOMPARE(roleName(model, ProfileModel::ActiveRole), QString("active"));
        QCOMPARE(model.data(index, ProfileModel::IdRole).toString(), id);
        QCOMPARE(model.data(index, ProfileModel::NameRole).toString(), QString("样例订阅"));
        QCOMPARE(model.data(index, ProfileModel::UrlRole).toString(), QString("https://example.test/sub.yaml"));
        QCOMPARE(model.data(index, ProfileModel::LocalRole).toBool(), false);
        QVERIFY(!model.data(index, ProfileModel::ActiveRole).toBool());
        QCOMPARE(model.rowCount(), store.profiles().size());
        QVERIFY(model.document(id).contains("mixed-port"));
    }

    void documentControllerEdits()
    {
        DocumentController document;
        QVERIFY(document.text().isEmpty());
        document.setTitle("生效配置");
        document.setHelp("help");
        document.setExample("a: 1\n");
        document.fillExample();
        QCOMPARE(document.text(), QString("a: 1\n"));
        QCOMPARE(document.title(), QString("生效配置"));
        document.setEditable(true);
        QVERIFY(document.editable());
    }

private:
    QTemporaryDir *m_data = nullptr;
};

QTEST_MAIN(FacadeContractTest)
#include "test_plugindata.moc"
