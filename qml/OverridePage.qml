// SPDX-License-Identifier: LGPL-3.0-or-later
// 覆写：YAML 与 JavaScript 覆写编辑、保存与生效配置预览（数据来自 dccData.override）。
import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D
import "Style.js" as Style

DccObject {
    id: root

    // 生效配置预览：只有本页发起的文档请求才打开窗口。
    property bool documentPending: false

    DccObject {
        name: "overrideMessage"
        parentName: "dccproxy/override"
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
        name: "override_yamlTitle"
        parentName: "dccproxy/override"
        displayName: qsTr("YAML 覆写")
        weight: 5
    }

    DccObject {
        name: "yamlOverrideGroup"
        parentName: "dccproxy/override"
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "yamlOverrideEnabled"
            parentName: "dccproxy/override/yamlOverrideGroup"
            weight: 10
            displayName: qsTr("启用 YAML 覆写")
            description: qsTr("映射递归合并，列表整体替换，null 删除字段")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.override.yamlEnabled
                onCheckedChanged: if (dccData.override.yamlEnabled !== checked) dccData.override.yamlEnabled = checked
            }
        }

        DccObject {
            name: "yamlOverrideEditor"
            parentName: "dccproxy/override/yamlOverrideGroup"
            weight: 20
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                // 组卡片本身不带内边距，这里统一留出与 DCC 一致的 10 px。
                implicitHeight: yamlLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: yamlLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: Style.gap

                    D.Label {
                        Layout.fillWidth: true
                        text: qsTr("同名映射递归合并，列表整体替换，null 删除字段；保存后重启内核生效。")
                        wrapMode: Text.WordWrap
                        font: D.DTK.fontManager.t8
                        opacity: 0.6
                    }

                    D.TextArea {
                        id: yamlEditor
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: 240
                        Layout.minimumHeight: 160
                        // DTK 的 TextArea 由内容宽度推导隐式宽度，配合占位文本会与布局互相依赖；
                        // 编辑器始终占满整行，固定隐式宽度断开该绑定环。
                        implicitWidth: 0
                        placeholderText: "mixed-port: 16666\nallow-lan: false"
                        wrapMode: TextEdit.WrapAnywhere
                        selectByMouse: true
                        text: dccData.override.yaml
                        onTextChanged: if (dccData.override.yaml !== text) dccData.override.yaml = text
                        font.pixelSize: D.DTK.fontManager.t8.pixelSize
                        font.family: "monospace"
                    }
                }
            }
        }
    }

    DccTitleObject {
        name: "override_scriptTitle"
        parentName: "dccproxy/override"
        displayName: qsTr("JavaScript 覆写")
        weight: 15
    }

    DccObject {
        name: "scriptOverrideGroup"
        parentName: "dccproxy/override"
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "scriptOverrideEnabled"
            parentName: "dccproxy/override/scriptOverrideGroup"
            weight: 10
            displayName: qsTr("启用 JavaScript 覆写")
            description: qsTr("隔离执行，限时 4 秒")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.override.scriptEnabled
                onCheckedChanged: if (dccData.override.scriptEnabled !== checked) dccData.override.scriptEnabled = checked
            }
        }

        DccObject {
            name: "scriptOverrideEditor"
            parentName: "dccproxy/override/scriptOverrideGroup"
            weight: 20
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                implicitHeight: scriptLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: scriptLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: Style.gap

                    D.Label {
                        Layout.fillWidth: true
                        text: qsTr("函数 main(config) 返回修改后的配置，在隔离环境中限时执行。")
                        wrapMode: Text.WordWrap
                        font: D.DTK.fontManager.t8
                        opacity: 0.6
                    }

                    D.TextArea {
                        id: scriptEditor
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: 200
                        Layout.minimumHeight: 140
                        implicitWidth: 0
                        placeholderText: "function main(config) {\n  return config;\n}"
                        wrapMode: TextEdit.WrapAnywhere
                        selectByMouse: true
                        text: dccData.override.script
                        onTextChanged: if (dccData.override.script !== text) dccData.override.script = text
                        font.pixelSize: D.DTK.fontManager.t8.pixelSize
                        font.family: "monospace"
                    }
                }
            }
        }
    }

    DccObject {
        name: "overrideActions"
        parentName: "dccproxy/override"
        weight: 25
        pageType: DccObject.Item
        canSearch: false
        page: RowLayout {
            spacing: Style.gap

            Item {
                Layout.fillWidth: true
            }

            D.Button {
                text: qsTr("填入 YAML 示例")
                onClicked: dccData.override.fillExample()
            }

            D.RecommandButton {
                text: qsTr("保存覆写")
                onClicked: dccData.override.save()
            }

            D.Button {
                text: qsTr("预览生效配置")
                onClicked: {
                    root.documentPending = true
                    dccData.settings.showRuntimeProfile()
                }
            }
        }
    }

    DocumentDialog {
        id: documentDialog
    }

    Connections {
        target: dccData.settings.document
        function onChanged() {
            documentTimer.restart()
        }
    }

    Timer {
        id: documentTimer
        interval: 0
        onTriggered: {
            if (!root.documentPending)
                return
            root.documentPending = false
            var document = dccData.settings.document
            documentDialog.title = document.title
            documentDialog.helpText = document.help
            documentDialog.editable = document.editable
            documentDialog.example = document.example
            documentDialog.openText(document.text)
        }
    }
}
