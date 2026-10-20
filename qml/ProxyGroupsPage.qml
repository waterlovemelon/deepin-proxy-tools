// SPDX-License-Identifier: LGPL-3.0-or-later
// 代理组: subscription source, the policy group cards with their node grids,
// the speed test settings and the automatic selection of the current group.
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D

import "Style.js" as Style

DccObject {
    DccObject {
        name: "proxies_message"
        parentName: "dccproxy/proxies"
        weight: 1
        pageType: DccObject.Item
        page: MessageBar {
            id: messageBar
            Connections {
                target: dccData
                function onMessage(text, error) {
                    messageBar.text = text
                    messageBar.error = error
                }
            }
        }
    }

    DccTitleObject {
        name: "proxies_sourceTitle"
        parentName: "dccproxy/proxies"
        displayName: qsTr("订阅源")
        weight: 5
    }

    DccObject {
        name: "proxies_source"
        parentName: "dccproxy/proxies"
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "proxies_sourceSelect"
            parentName: "dccproxy/proxies/proxies_source"
            weight: 10
            pageType: DccObject.Editor
            displayName: qsTr("来源")
            description: dccData.proxies.sourceHint
            page: D.ComboBox {
                implicitWidth: Style.controlWidth
                model: dccData.proxies.sources
                currentIndex: dccData.proxies.sourceIndex
                onActivated: dccData.proxies.sourceIndex = index
            }
        }
    }

    DccTitleObject {
        name: "proxies_testTitle"
        parentName: "dccproxy/proxies"
        displayName: qsTr("测速")
        weight: 15
    }

    // Test URL, timeout and concurrency, labelled like every other editor row.
    DccObject {
        name: "proxies_testSettings"
        parentName: "dccproxy/proxies"
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "proxies_testUrl"
            parentName: "dccproxy/proxies/proxies_testSettings"
            weight: 10
            pageType: DccObject.Editor
            displayName: qsTr("测速地址")
            page: D.LineEdit {
                implicitWidth: Style.controlWidth
                text: dccData.proxies.testUrl
                placeholderText: qsTr("留空使用策略组地址")
                onEditingFinished: dccData.proxies.testUrl = text
            }
        }

        DccObject {
            name: "proxies_testTimeout"
            parentName: "dccproxy/proxies/proxies_testSettings"
            weight: 20
            pageType: DccObject.Editor
            displayName: qsTr("超时 (ms)")
            page: D.SpinBox {
                implicitWidth: Style.spinWidth
                locale: Qt.locale("C")
                from: 1
                to: 30000
                value: dccData.proxies.testTimeout
                onValueModified: dccData.proxies.testTimeout = value
            }
        }

        DccObject {
            name: "proxies_testConcurrency"
            parentName: "dccproxy/proxies/proxies_testSettings"
            weight: 30
            pageType: DccObject.Editor
            displayName: qsTr("并发")
            page: D.SpinBox {
                implicitWidth: Style.spinWidth
                locale: Qt.locale("C")
                from: 1
                to: 256
                value: dccData.proxies.testConcurrency
                onValueModified: dccData.proxies.testConcurrency = value
            }
        }

        DccObject {
            name: "proxies_testSummary"
            parentName: "dccproxy/proxies/proxies_testSettings"
            weight: 40
            pageType: DccObject.Item
            page: ColumnLayout {
                D.Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.topMargin: 4
                    Layout.bottomMargin: 6
                    visible: text !== ""
                    elide: Text.ElideRight
                    text: dccData.proxies.testSummary
                    font: D.DTK.fontManager.t8
                    opacity: 0.55
                }
            }
        }
    }

    DccTitleObject {
        name: "proxies_groupsTitle"
        parentName: "dccproxy/proxies"
        displayName: qsTr("策略组")
        weight: 25
    }

    // The current group, one card per policy group and the node grids.
    DccObject {
        name: "proxies_groups"
        parentName: "dccproxy/proxies"
        weight: 30
        pageType: DccObject.Item
        page: ColumnLayout {
            spacing: Style.gap

            // The group the actions below and the node grids apply to.
            Rectangle {
                readonly property color borderColor: Qt.rgba(palette.windowText.r, palette.windowText.g,
                                                             palette.windowText.b, 0.12)

                Layout.fillWidth: true
                implicitHeight: currentRow.implicitHeight + 20
                radius: 8
                color: palette.base
                border.width: 1
                border.color: borderColor

                RowLayout {
                    id: currentRow
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: Style.gap

                    D.Label {
                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("当前策略组")
                        font: D.DTK.fontManager.t7
                    }
                    D.Label {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        text: dccData.proxies.currentGroup !== "" ? dccData.proxies.currentGroup
                                                                  : qsTr("未选择")
                        elide: Text.ElideRight
                        font: D.DTK.fontManager.t7
                        opacity: 0.65
                    }
                    D.Button {
                        text: qsTr("恢复自动选择")
                        enabled: dccData.proxies.currentGroup !== "" && !dccData.proxies.previewing
                        onClicked: dccData.proxies.restoreAutomatic(dccData.proxies.currentGroup)
                    }
                }
            }

            Repeater {
                model: dccData.proxies.groups

                delegate: Rectangle {
                    id: groupCard

                    readonly property string groupName: model.name
                    readonly property bool expanded: model.expanded
                    readonly property var groupNodes: model.nodes
                    readonly property bool selected: dccData.proxies.currentGroup === groupName
                    readonly property color cardBorder: Qt.rgba(palette.windowText.r, palette.windowText.g,
                                                                palette.windowText.b, 0.12)
                    readonly property color hoverColor: Qt.rgba(palette.highlight.r, palette.highlight.g,
                                                               palette.highlight.b, 0.08)

                    Layout.fillWidth: true
                    implicitHeight: cardLayout.implicitHeight + 20
                    radius: 8
                    color: palette.base
                    border.width: selected ? 1.5 : 1
                    border.color: selected ? palette.highlight : cardBorder

                    // Hover tint stays behind the header and the node grid.
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: groupCard.hoverColor
                        visible: cardHover.containsMouse
                    }

                    MouseArea {
                        id: cardHover
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }

                    ColumnLayout {
                        id: cardLayout
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        Item {
                            Layout.fillWidth: true
                            implicitHeight: headerRow.implicitHeight

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onClicked: {
                                    dccData.proxies.setCurrentGroup(groupCard.groupName)
                                    dccData.proxies.setExpanded(groupCard.groupName, !groupCard.expanded)
                                }
                            }

                            RowLayout {
                                id: headerRow
                                anchors.fill: parent
                                spacing: 6

                                D.Label {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: groupCard.expanded ? "▾" : "▸"
                                    font: D.DTK.fontManager.t7
                                    opacity: 0.6
                                }

                                D.Label {
                                    Layout.maximumWidth: 240
                                    Layout.alignment: Qt.AlignVCenter
                                    text: model.displayName
                                    elide: Text.ElideRight
                                    font: Qt.font({
                                        "family": D.DTK.fontManager.t7.family,
                                        "pixelSize": D.DTK.fontManager.t7.pixelSize,
                                        "bold": true
                                    })
                                }

                                D.Label {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    text: model.type + " · " + qsTr("%1 个节点").arg(model.count)
                                          + (model.now !== "" ? " · " + qsTr("当前 %1").arg(model.now) : "")
                                    elide: Text.ElideRight
                                    font: D.DTK.fontManager.t8
                                    opacity: 0.55
                                }

                                D.Button {
                                    text: qsTr("测速")
                                    enabled: model.testable
                                    onClicked: dccData.proxies.testGroup(groupCard.groupName)
                                }
                                D.Button {
                                    text: qsTr("恢复自动")
                                    visible: model.automatic
                                    enabled: !dccData.proxies.previewing
                                    onClicked: dccData.proxies.restoreAutomatic(groupCard.groupName)
                                }
                            }
                        }

                        D.Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: 22
                            visible: text !== ""
                            elide: Text.ElideRight
                            text: model.route
                            font: D.DTK.fontManager.t8
                            opacity: 0.55
                        }

                        GridView {
                            id: nodeGrid

                            readonly property int columnCount: Math.max(1, Math.floor(width / 200))
                            readonly property int rowHeight: 52

                            Layout.fillWidth: true
                            Layout.topMargin: 2
                            Layout.preferredHeight: visible ? Math.ceil(groupNodes.count / columnCount) * rowHeight : 0
                            visible: groupCard.expanded && groupNodes.count > 0
                            cellWidth: width / columnCount
                            cellHeight: rowHeight
                            flow: GridView.FlowLeftToRight
                            interactive: false
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: groupNodes

                            delegate: NodeCard {
                                width: GridView.view.cellWidth
                                height: GridView.view.cellHeight
                                group: groupCard.groupName
                                nodeName: model.name
                                displayName: model.displayName
                                flag: model.flag
                                nodeType: model.type
                                delayText: model.delayText
                                testing: model.testing
                                active: model.active
                                error: model.error
                            }
                        }

                        D.Label {
                            Layout.fillWidth: true
                            Layout.leftMargin: 22
                            visible: groupCard.expanded && groupNodes.count === 0
                            text: qsTr("该策略组暂无节点")
                            font: D.DTK.fontManager.t8
                            opacity: 0.5
                        }
                    }
                }
            }

            EmptyHint {
                Layout.fillWidth: true
                visible: dccData.proxies.groups.count === 0
                text: qsTr("导入订阅后即可查看节点")
                hint: qsTr("按订阅保留原有策略组，导入后可在此预览、切换和测速。")
            }

            D.RecommandButton {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: Style.gap
                visible: dccData.proxies.groups.count === 0
                text: qsTr("前往订阅")
                onClicked: DccApp.showPage("dccproxy/profiles")
            }

            D.Label {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.topMargin: 2
                wrapMode: Text.Wrap
                text: qsTr("点击组名展开节点，点击卡片切换此组，点击延迟单独测速。规则模式按匹配的策略组分流；切换主节点不会覆盖其他组的独立选择。")
                font: D.DTK.fontManager.t8
                opacity: 0.55
            }
        }
    }
}
