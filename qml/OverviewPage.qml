// SPDX-License-Identifier: LGPL-3.0-or-later
// 概览: kernel state, traffic, service switch, proxy mode, the DNS/SNIFFER/
// HOSTS/LISTENERS/TUNNELS section editors and the manual refresh.
import QtQuick 2.15
import QtQuick.Layouts 1.15

import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D

import "Style.js" as Style

DccObject {
    id: root

    // The advanced rows open this document window; it is filled by
    // dccData.settings.editSection() and written back through saveSection().
    function openSection(name) {
        dccData.settings.editSection(name)
        sectionDialog.section = name
        sectionDialog.title = dccData.settings.document.title
        sectionDialog.helpText = dccData.settings.document.help
        sectionDialog.example = dccData.settings.document.example
        sectionDialog.editable = dccData.settings.document.editable
        sectionDialog.open(dccData.settings.document.text)
    }

    DccTitleObject {
        name: "overview_statusTitle"
        parentName: "dccproxy/overview"
        displayName: qsTr("状态")
        weight: 5
    }

    // Kernel text, traffic, statistics, backend errors and status messages.
    DccObject {
        name: "overview_status"
        parentName: "dccproxy/overview"
        weight: 10
        pageType: DccObject.Item
        page: ColumnLayout {
            id: statusColumn
            property string messageText: ""
            property bool messageError: false
            spacing: 6

            MessageBar {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                text: statusColumn.messageText !== "" ? statusColumn.messageText
                                                      : dccData.service.error
                error: statusColumn.messageText !== "" ? statusColumn.messageError
                                                       : dccData.service.error !== ""
            }

            D.Label {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                visible: text !== ""
                wrapMode: Text.Wrap
                text: dccData.overview.stateText
                font: D.DTK.fontManager.t7
            }

            D.Label {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                visible: text !== ""
                wrapMode: Text.Wrap
                text: dccData.overview.kernelText
                font: D.DTK.fontManager.t8
                opacity: 0.6
            }

            D.Label {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                visible: text !== ""
                wrapMode: Text.Wrap
                text: dccData.overview.trafficText
                font: D.DTK.fontManager.t8
                opacity: 0.6
            }

            D.Label {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                visible: text !== ""
                wrapMode: Text.Wrap
                text: dccData.overview.statisticsText
                font: D.DTK.fontManager.t8
                opacity: 0.6
            }

            Connections {
                target: dccData
                function onMessage(text, error) {
                    statusColumn.messageText = text
                    statusColumn.messageError = error
                }
            }
        }
    }

    DccTitleObject {
        name: "overview_serviceTitle"
        parentName: "dccproxy/overview"
        displayName: qsTr("代理服务")
        weight: 15
    }

    // 代理服务 switch, 代理模式 and the restart/refresh actions, one control group.
    DccObject {
        name: "overview_serviceGroup"
        parentName: "dccproxy/overview"
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "overview_service"
            parentName: "dccproxy/overview/overview_serviceGroup"
            weight: 10
            pageType: DccObject.Editor
            displayName: qsTr("启用代理服务")
            description: qsTr("关闭控制中心后后台继续运行")
            page: D.Switch {
                checked: dccData.service.running
                enabled: !dccData.service.busy
                onToggled: {
                    if (checked !== dccData.service.running) {
                        if (checked)
                            dccData.service.start()
                        else
                            dccData.service.stop()
                    }
                }
            }
        }

        DccObject {
            name: "overview_mode"
            parentName: "dccproxy/overview/overview_serviceGroup"
            weight: 20
            pageType: DccObject.Editor
            displayName: qsTr("代理模式")
            page: D.ComboBox {
                implicitWidth: Style.controlWidth
                model: [qsTr("规则"), qsTr("全局"), qsTr("直连")]
                currentIndex: dccData.overview.mode === "global" ? 1 : (dccData.overview.mode === "direct" ? 2 : 0)
                onActivated: dccData.settings.setMode(["rule", "global", "direct"][index])
            }
        }

        DccObject {
            name: "overview_actions"
            parentName: "dccproxy/overview/overview_serviceGroup"
            weight: 30
            pageType: DccObject.Item
            page: ColumnLayout {
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: Style.margin
                    Layout.rightMargin: Style.margin
                    Layout.topMargin: 6
                    Layout.bottomMargin: 6
                    spacing: Style.gap

                    Item {
                        Layout.fillWidth: true
                    }
                    D.Button {
                        text: qsTr("重启")
                        enabled: !dccData.service.busy
                        onClicked: dccData.service.restart()
                    }
                    D.Button {
                        text: qsTr("刷新")
                        onClicked: dccData.refresh()
                    }
                }
            }
        }
    }

    DccTitleObject {
        name: "overview_advancedTitle"
        parentName: "dccproxy/overview"
        displayName: qsTr("高级配置")
        weight: 25
    }

    // The five advanced sections, edited in the local document dialog below.
    DccObject {
        name: "overview_sectionsHint"
        parentName: "dccproxy/overview"
        weight: 27
        pageType: DccObject.Item
        page: ColumnLayout {
            D.Label {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.bottomMargin: 4
                wrapMode: Text.Wrap
                font: D.DTK.fontManager.t8
                opacity: 0.55
                text: qsTr("未启动内核时也可以编辑这些配置，启动或重启后生效。")
            }
        }
    }

    DccObject {
        name: "overview_sections"
        parentName: "dccproxy/overview"
        weight: 30
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "overview_section_dns"
            parentName: "dccproxy/overview/overview_sections"
            weight: 10
            pageType: DccObject.MenuEditor
            backgroundType: DccObject.ClickStyle
            displayName: qsTr("DNS")
            onActive: root.openSection("dns")
        }

        DccObject {
            name: "overview_section_sniffer"
            parentName: "dccproxy/overview/overview_sections"
            weight: 20
            pageType: DccObject.MenuEditor
            backgroundType: DccObject.ClickStyle
            displayName: qsTr("SNIFFER")
            onActive: root.openSection("sniffer")
        }

        DccObject {
            name: "overview_section_hosts"
            parentName: "dccproxy/overview/overview_sections"
            weight: 30
            pageType: DccObject.MenuEditor
            backgroundType: DccObject.ClickStyle
            displayName: qsTr("HOSTS")
            onActive: root.openSection("hosts")
        }

        DccObject {
            name: "overview_section_listeners"
            parentName: "dccproxy/overview/overview_sections"
            weight: 40
            pageType: DccObject.MenuEditor
            backgroundType: DccObject.ClickStyle
            displayName: qsTr("LISTENERS")
            onActive: root.openSection("listeners")
        }

        DccObject {
            name: "overview_section_tunnels"
            parentName: "dccproxy/overview/overview_sections"
            weight: 50
            pageType: DccObject.MenuEditor
            backgroundType: DccObject.ClickStyle
            displayName: qsTr("TUNNELS")
            onActive: root.openSection("tunnels")
        }
    }

    DccObject {
        name: "overview_note"
        parentName: "dccproxy/overview"
        weight: 35
        pageType: DccObject.Item
        page: ColumnLayout {
            D.Label {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.topMargin: 2
                Layout.bottomMargin: 6
                wrapMode: Text.Wrap
                font: D.DTK.fontManager.t8
                opacity: 0.55
                text: qsTr("关闭控制中心后，后台内核会继续运行。虚拟网卡请在“虚拟网卡”页面设置。")
            }
        }
    }

    DocumentDialog {
        id: sectionDialog
        property string section: ""
        editable: true

        onAccepted: function (text) {
            dccData.settings.saveSection(sectionDialog.section, text)
        }
    }
}
