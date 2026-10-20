// SPDX-License-Identifier: LGPL-3.0-or-later
// 日志：后台最近记录与实时日志（数据来自 dccData.logs）。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Layouts 1.15
import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D
import "Style.js" as Style

DccObject {
    id: root

    readonly property var logLevels: ["info", "warning", "error", "debug", "silent"]

    // 页面消息优先显示；没有消息时用日志流状态作为状态行。
    property string messageText: ""
    property bool messageError: false

    DccObject {
        name: "logsMessage"
        parentName: "dccproxy/logs"
        weight: 0
        pageType: DccObject.Item
        canSearch: false
        page: MessageBar {
            id: message
            text: root.messageText.length > 0
                  ? (dccData.logs.status.length > 0
                     ? root.messageText + "  ·  " + dccData.logs.status
                     : root.messageText)
                  : dccData.logs.status
            error: root.messageError
            Connections {
                target: dccData
                function onMessage(text, error) {
                    root.messageText = text
                    root.messageError = error
                }
            }
        }
    }

    DccTitleObject {
        name: "logs_viewerTitle"
        parentName: "dccproxy/logs"
        displayName: qsTr("实时日志")
        weight: 5
    }

    DccObject {
        name: "logViewerGroup"
        parentName: "dccproxy/logs"
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "logViewer"
            parentName: "dccproxy/logs/logViewerGroup"
            weight: 10
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                // 组卡片本身不带内边距，这里统一留出与 DCC 一致的 10 px。
                implicitHeight: viewerLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: viewerLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: Style.gap

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Style.gap

                        D.Label {
                            text: qsTr("级别")
                            font: D.DTK.fontManager.t7
                        }

                        D.ComboBox {
                            id: levelBox
                            implicitWidth: 140
                            flat: true
                            model: root.logLevels
                            currentIndex: Math.max(0, root.logLevels.indexOf(dccData.logs.level))
                            onActivated: dccData.logs.level = root.logLevels[levelBox.currentIndex]
                        }

                        Item {
                            Layout.preferredWidth: Style.gap
                        }

                        D.Label {
                            text: qsTr("暂停")
                            font: D.DTK.fontManager.t7
                        }

                        D.Switch {
                            checked: dccData.logs.paused
                            onCheckedChanged: if (dccData.logs.paused !== checked) dccData.logs.paused = checked
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        D.Button {
                            text: qsTr("清空")
                            onClicked: dccData.logs.clear()
                        }

                        D.Button {
                            text: qsTr("导出")
                            onClicked: exportDialog.open()
                        }

                        D.Button {
                            text: qsTr("后台日志")
                            onClicked: daemonDialog.openText(dccData.logs.daemonLogTail())
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.12)
                    }

                    // 终端式日志区：等宽字体、单行省略，空列表时显示居中占位。
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 420

                        D.ListView {
                            id: logList
                            anchors.fill: parent
                            clip: true
                            model: dccData.logs
                            ScrollBar.vertical: D.ScrollBar {}

                            // 只有停在末尾时才跟随新日志，方便向上翻看历史记录。
                            property bool followEnd: true
                            onMovementEnded: followEnd = atYEnd
                            onCountChanged: if (followEnd) positionViewAtEnd()

                            // D.Label keeps the full line available as a tooltip;
                            // a terminal row cannot wrap without losing the grid.
                            delegate: D.Label {
                                x: Style.margin
                                width: logList.width - 2 * Style.margin
                                height: 22
                                text: model.text
                                wrapMode: Text.NoWrap
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: D.DTK.fontManager.t8.pixelSize
                                font.family: "monospace"
                            }

            
                                                }

                        EmptyHint {
                            anchors.centerIn: parent
                            width: parent.width
                            visible: dccData.logs.count === 0
                            text: qsTr("暂无日志。")
                            hint: qsTr("启动内核并通过本插件访问网页后，新连接会显示在这里。")
                        }
                    }
                }
            }
        }
    }

    DocumentDialog {
        id: daemonDialog
        title: qsTr("后台日志")
        helpText: dccData.logs.daemonLogPath()
    }

    FileDialog {
        id: exportDialog
        title: qsTr("导出日志")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("日志文件 (*.log)"), qsTr("所有文件 (*)")]
        onAccepted: dccData.logs.exportTo(dccData.localPath(selectedFile))
    }
}
