// SPDX-License-Identifier: LGPL-3.0-or-later
// 连接：实时连接表、搜索、关闭选中 / 全部连接（数据来自 dccData.connections）。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D

import "Style.js" as Style

DccObject {
    id: root

    // 表格列定义：表头与数据行共用，保证列对齐。只有目标列自适应，其余列宽固定。
    readonly property var columns: [
        { "title": qsTr("目标"), "role": "target" },
        { "title": qsTr("进程"), "role": "process", "width": 90 },
        { "title": qsTr("类型"), "role": "kind", "width": 56 },
        { "title": qsTr("规则"), "role": "rule", "width": 150 },
        { "title": qsTr("代理链"), "role": "chain", "width": 110 },
        { "title": qsTr("上传"), "role": "uploadText", "width": 60 },
        { "title": qsTr("下载"), "role": "downloadText", "width": 60 }
    ]

    // 行内左右留白，表头与数据行一致。
    readonly property int rowPadding: 12

    // 刷新后按连接 id 还原选中行，与旧面板一致。
    property string selectedId: ""
    property int currentRow: -1

    function cellText(item, column) {
        if (!item)
            return ""
        var value = item[column.role]
        return value === undefined ? "" : String(value)
    }

    DccObject {
        name: "connections_message"
        parentName: "dccproxy/connections"
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
        name: "connections_title"
        parentName: "dccproxy/connections"
        displayName: qsTr("实时连接")
        weight: 5
    }

    DccObject {
        name: "connections_search"
        parentName: "dccproxy/connections"
        weight: 10
        pageType: DccObject.Item
        canSearch: false
        page: ColumnLayout {
            D.SearchEdit {
                id: search
                Layout.preferredWidth: Style.searchWidth
                Layout.maximumWidth: Style.searchWidth
                Layout.alignment: Qt.AlignRight
                placeholder: qsTr("搜索域名、进程、规则、节点")
                text: dccData.connections.search
                onTextChanged: if (dccData.connections.search !== text) dccData.connections.search = text
            }
        }
    }

    DccObject {
        name: "connectionsTable"
        parentName: "dccproxy/connections"
        weight: 15
        pageType: DccObject.Item
        canSearch: false
        page: DccGroupView {}

        DccObject {
            name: "connections_list"
            parentName: "dccproxy/connections/connectionsTable"
            weight: 10
            pageType: DccObject.Item
            page: ColumnLayout {
                spacing: 2

                TableHeader {
                    Layout.fillWidth: true
                    Layout.leftMargin: root.rowPadding
                    Layout.rightMargin: root.rowPadding
                    Layout.topMargin: 6
                    columns: root.columns
                }

                D.ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.leftMargin: root.rowPadding
                    Layout.rightMargin: root.rowPadding
                    Layout.preferredHeight: dccData.connections.count === 0 ? 0
                                                                            : Math.min(480, Math.max(160, dccData.connections.count * 40))
                    visible: dccData.connections.count > 0
                    clip: true
                    model: dccData.connections
                    ScrollBar.vertical: D.ScrollBar {}

                    delegate: Rectangle {
                        id: rowItem

                        readonly property var rowData: model
                        readonly property bool selected: ListView.isCurrentItem

                        width: list.width
                        height: 40
                        radius: 6
                        color: selected ? Qt.rgba(palette.highlight.r, palette.highlight.g,
                                                  palette.highlight.b, 0.12)
                                        : (rowHover.containsMouse
                                           ? Qt.rgba(palette.highlight.r, palette.highlight.g,
                                                     palette.highlight.b, 0.06)
                                           : "transparent")

                        RowLayout {
                            anchors.fill: parent
                            spacing: 10

                            Repeater {
                                model: root.columns

                                delegate: D.Label {
                                    Layout.fillWidth: modelData.width === undefined
                                    Layout.preferredWidth: modelData.width !== undefined ? modelData.width : -1
                                    Layout.alignment: Qt.AlignVCenter
                                    text: root.cellText(rowItem.rowData, modelData)
                                    elide: Text.ElideRight
                                    horizontalAlignment: index >= 5 ? Text.AlignRight : Text.AlignLeft
                                    font: D.DTK.fontManager.t7
                                    opacity: index === 0 ? 0.95 : 0.7
                                }
                            }
                        }

                        MouseArea {
                            id: rowHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: list.currentIndex = index
                        }
                    }

                    onCurrentIndexChanged: {
                        root.currentRow = currentIndex
                        if (currentIndex >= 0)
                            root.selectedId = dccData.connections.idAt(currentIndex)
                    }
                }

                EmptyHint {
                    Layout.fillWidth: true
                    visible: dccData.connections.count === 0
                    text: qsTr("当前没有连接")
                    hint: qsTr("启动内核后，新的连接会显示在这里。")
                }

                // 刷新后按 id 还原选中行；连接已消失时清空选择。
                Connections {
                    target: dccData.connections
                    function onChanged() {
                        Qt.callLater(function () {
                            if (root.selectedId === "")
                                return
                            for (var row = 0; row < dccData.connections.count; ++row) {
                                if (dccData.connections.idAt(row) === root.selectedId) {
                                    list.currentIndex = row
                                    return
                                }
                            }
                            root.selectedId = ""
                            list.currentIndex = -1
                        })
                    }
                }
            }
        }
    }

    DccObject {
        name: "connections_actions"
        parentName: "dccproxy/connections"
        weight: 20
        pageType: DccObject.Item
        page: ColumnLayout {
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Style.margin
                Layout.rightMargin: Style.margin
                Layout.topMargin: 2
                Layout.bottomMargin: 6
                spacing: Style.gap

                Item {
                    Layout.fillWidth: true
                }
                D.Button {
                    text: qsTr("关闭选中连接")
                    enabled: root.currentRow >= 0
                    onClicked: dccData.connections.closeAt(root.currentRow)
                }
                D.WarningButton {
                    text: qsTr("关闭全部连接")
                    onClicked: dccData.connections.closeAll()
                }
            }
        }
    }

    Timer {
        // Poll only while the connections page is showing (the facade reports
        // the control center's active page).
        running: dccData.activePage === "connections"
        interval: 3000
        repeat: true
        onTriggered: dccData.connections.refresh()
    }

    Connections {
        target: dccData
        function onActivePageChanged() {
            if (dccData.activePage === "connections")
                dccData.connections.refresh()
        }
    }
}
