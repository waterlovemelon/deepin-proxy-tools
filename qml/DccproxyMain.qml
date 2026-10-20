// SPDX-License-Identifier: LGPL-3.0-or-later
// Page tree. Every page is a DccObject child of the "dccproxy" module; the
// page content lives in the referenced QML file and may use dccData.
import QtQuick 2.15
import org.deepin.dcc 1.0

DccObject {
    // Tell the data object which page is on screen: polling, log streaming and
    // model rebuilds only run for the pages that need them.
    Connections {
        target: DccApp
        function onActiveObjectChanged() {
            const active = DccApp.activeObject
            dccData.setActivePage(active && active.name !== undefined ? active.name : "")
        }
    }

    Component.onCompleted: {
        const active = DccApp.activeObject
        dccData.setActivePage(active && active.name !== undefined ? active.name : "")
    }

    DccObject {
        name: "overview"
        parentName: "dccproxy"
        displayName: qsTr("概览")
        description: qsTr("内核状态、代理模式与流量")
        icon: "overview"
        weight: 10
        OverviewPage {}
    }
    DccObject {
        name: "proxies"
        parentName: "dccproxy"
        displayName: qsTr("代理组")
        description: qsTr("策略组、节点切换与测速")
        icon: "proxies"
        weight: 20
        ProxyGroupsPage {}
    }
    DccObject {
        name: "profiles"
        parentName: "dccproxy"
        displayName: qsTr("订阅")
        description: qsTr("导入、更新与启用配置")
        icon: "profiles"
        weight: 30
        ProfilesPage {}
    }
    DccObject {
        name: "connections"
        parentName: "dccproxy"
        displayName: qsTr("连接")
        description: qsTr("当前连接与流量")
        icon: "connections"
        weight: 40
        ConnectionsPage {}
    }
    DccObject {
        name: "rules"
        parentName: "dccproxy"
        displayName: qsTr("规则")
        description: qsTr("分流规则与网址规则")
        icon: "rules"
        weight: 50
        RulesPage {}
    }
    DccObject {
        name: "providers"
        parentName: "dccproxy"
        displayName: qsTr("资源")
        description: qsTr("节点与规则提供者、GEO 数据")
        icon: "providers"
        weight: 60
        ProvidersPage {}
    }
    DccObject {
        name: "logs"
        parentName: "dccproxy"
        displayName: qsTr("日志")
        description: qsTr("内核实时日志与后台记录")
        icon: "logs"
        weight: 70
        LogsPage {}
    }
    DccObject {
        name: "override"
        parentName: "dccproxy"
        displayName: qsTr("覆写")
        description: qsTr("YAML 与 JavaScript 覆写")
        icon: "override"
        weight: 80
        OverridePage {}
    }
    DccObject {
        name: "tools"
        parentName: "dccproxy"
        displayName: qsTr("工具")
        description: qsTr("DNS 查询、缓存与 Sub-Store")
        icon: "tools"
        weight: 90
        ToolsPage {}
    }
    DccObject {
        name: "settings"
        parentName: "dccproxy"
        displayName: qsTr("设置")
        description: qsTr("控制器、端口、系统代理与备份")
        icon: "settings"
        weight: 100
        SettingsPage {}
    }
    DccObject {
        name: "tun"
        parentName: "dccproxy"
        displayName: qsTr("虚拟网卡")
        description: qsTr("TUN 网卡、协议栈与路由")
        icon: "tun"
        weight: 110
        TunPage {}
    }
}
