// SPDX-License-Identifier: LGPL-3.0-or-later
// 资源：节点与规则提供者、GEO 数据（数据来自 dccData.providers）。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D
import "Style.js" as Style

DccObject {
    id: root

    // 固定列用 width，名称列弹性占满剩余宽度。
    readonly property var columns: [
        { "title": qsTr("类型"), "role": "kind", "width": 100 },
        { "title": qsTr("名称"), "role": "name" },
        { "title": qsTr("来源"), "role": "vehicle", "width": 160 },
        { "title": qsTr("更新时间"), "role": "updated", "width": 200 },
        { "title": qsTr("条目数"), "role": "entries", "width": 90 }
    ]

    function cellText(item, column) {
        if (!item)
            return ""
        var value = item[column.role]
        return value === undefined ? "" : String(value)
    }

    DccObject {
        name: "providersMessage"
        parentName: "dccproxy/providers"
        weight: 0
        pageType: DccObject.Item
        canSearch: false
        page: MessageBar {
            id: message
            Connections {
                target: dccData
                function onMessage(text, error) {
                    message.text = text
                    message.error = error
                }
            }
        }
    }

    DccTitleObject {
        name: "providers_listTitle"
        parentName: "dccproxy/providers"
        displayName: qsTr("资源列表")
        weight: 5
    }

    DccObject {
        name: "providerTableGroup"
        parentName: "dccproxy/providers"
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "providerTable"
            parentName: "dccproxy/providers/providerTableGroup"
            weight: 10
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                // 组卡片本身不带内边距，这里统一留出与 DCC 一致的 10 px。
                implicitHeight: tableLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: tableLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: Style.gap

                    TableHeader {
                        Layout.fillWidth: true
                        columns: root.columns
                    }

                    D.ListView {
                        id: providerList
                        Layout.fillWidth: true
                        visible: count > 0
                        Layout.preferredHeight: visible ? Math.min(420, count * Style.rowHeight) : 0
                        clip: true
                        model: dccData.providers
                        ScrollBar.vertical: D.ScrollBar {}

                        delegate: Rectangle {
                            id: providerRow

                            readonly property var rowData: model
                            // 只有节点提供者支持健康检查。
                            readonly property bool proxyProvider: model.health

                            width: providerList.width
                            height: Style.rowHeight
                            radius: 6
                            color: ListView.isCurrentItem
                                   ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.16)
                                   : (rowMouse.containsMouse
                                      ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.08)
                                      : "transparent")

                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: providerList.currentIndex = index
                            }

                            RowLayout {
                                anchors.fill: parent
                                spacing: Style.gap

                                Repeater {
                                    model: root.columns

                                    delegate: D.Label {
                                        Layout.preferredWidth: modelData.width !== undefined ? modelData.width : -1
                                        Layout.fillWidth: modelData.width === undefined
                                        text: root.cellText(providerRow.rowData, modelData)
                                        elide: Text.ElideRight
                                        font: D.DTK.fontManager.t7
                                    }
                                }
                            }
                        }
                    }

                    EmptyHint {
                        Layout.fillWidth: true
                        visible: dccData.providers.count === 0
                        text: qsTr("没有可用的提供者资源")
                        hint: qsTr("导入带节点或规则提供者的订阅后，这里会显示它们的来源与更新时间。")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Style.gap

                        Item {
                            Layout.fillWidth: true
                        }

                        D.Button {
                            text: qsTr("更新资源")
                            enabled: providerList.currentIndex >= 0
                            onClicked: dccData.providers.updateAt(providerList.currentIndex)
                        }

                        D.Button {
                            text: qsTr("节点健康检查")
                            enabled: providerList.currentItem !== null && providerList.currentItem.proxyProvider
                            onClicked: dccData.providers.healthCheckAt(providerList.currentIndex)
                        }

                        D.Button {
                            text: qsTr("更新 GEO 数据")
                            onClicked: dccData.providers.updateGeo()
                        }
                    }
                }

                Component.onCompleted: dccData.providers.refresh()
            }
        }
    }
}
