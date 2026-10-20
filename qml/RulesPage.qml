// SPDX-License-Identifier: LGPL-3.0-or-later
// 规则：网址规则添加、运行中的分流规则列表、搜索、启用 / 禁用与高级编辑。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D
import "Style.js" as Style

DccObject {
    id: root

    // 固定列用 width，弹性列不写 width（由 TableHeader 与行共同均分）。
    readonly property var columns: [
        { "title": qsTr("类型"), "role": "type", "width": 110 },
        { "title": qsTr("匹配内容"), "role": "payload" },
        { "title": qsTr("目标策略"), "role": "proxy", "width": 180 },
        { "title": qsTr("状态"), "role": "disabled", "width": 80 }
    ]

    // 网址规则：输入内容、预览文本与“添加”按钮的可用状态。
    property string websiteInput: ""
    property string websitePreview: ""
    property bool websiteValid: false
    // 预览文本是错误说明（而不是正常规则）时用警示色显示。
    property bool websitePreviewError: false

    // 高级规则编辑：只有本页发起的文档请求才打开窗口。
    property bool documentPending: false

    // 列表选中项：操作行与列表在不同的 page 组件里，状态放在根对象上。
    property int selectedRuleIndex: -1
    property bool selectedRuleDisabled: false

    function selectRule(index, disabled) {
        selectedRuleIndex = index
        selectedRuleDisabled = disabled
    }

    function clearRuleSelection() {
        selectedRuleIndex = -1
        selectedRuleDisabled = false
    }

    function policy() {
        return dccData.proxies.websiteRulePolicy(dccData.overview.mode)
    }

    function refreshWebsiteRule() {
        if (dccData.profiles.count === 0) {
            websitePreview = qsTr("先导入订阅即可添加网址规则。")
            websiteValid = false
            websitePreviewError = false
            return
        }
        var target = policy()
        if (websiteInput.trim().length === 0) {
            websitePreview = qsTr("只需填写网址。自动提取域名并关联当前策略组 %1；切换该组内的节点后，规则继续跟随该组。网址规则优先于订阅规则，在规则模式下生效。").arg(target)
            websiteValid = false
            websitePreviewError = false
            return
        }
        var preview = dccData.rules.websiteRulePreview(websiteInput, target)
        // 生成的规则形如“类型,匹配内容,策略[,选项]”，否则是错误说明。
        var fields = preview.split(",")
        websiteValid = fields.length >= 3 && fields[2] === target
        websitePreview = preview
        websitePreviewError = !websiteValid
    }

    function addWebsiteRule() {
        if (!websiteValid)
            return
        dccData.rules.addWebsiteRule(websiteInput, policy(), dccData.proxies.currentSourceId())
        websiteInput = ""
        refreshWebsiteRule()
    }

    function cellText(item, column) {
        if (!item)
            return ""
        var value = item[column.role]
        if (column.role === "disabled")
            return value ? qsTr("禁用") : qsTr("启用")
        return value === undefined ? "" : String(value)
    }

    function editSection() {
        documentPending = true
        dccData.settings.editSection("rules")
    }

    DccObject {
        name: "rulesMessage"
        parentName: "dccproxy/rules"
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
        name: "rules_websiteTitle"
        parentName: "dccproxy/rules"
        displayName: qsTr("网址规则")
        weight: 5
    }

    DccObject {
        name: "websiteRuleGroup"
        parentName: "dccproxy/rules"
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "websiteRule"
            parentName: "dccproxy/rules/websiteRuleGroup"
            weight: 10
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                // 组卡片本身不带内边距，这里统一留出与 DCC 一致的 10 px。
                implicitHeight: websiteLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: websiteLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    // 预览行紧贴输入行，与 DCC 的 6 px 辅助文字间距一致。
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Style.gap

                        D.LineEdit {
                            id: websiteField
                            Layout.fillWidth: true
                            implicitWidth: Style.controlWidth
                            placeholderText: qsTr("填写网址，例如 https://example.com/path 或 *.example.com")
                            onTextChanged: {
                                root.websiteInput = text
                                root.refreshWebsiteRule()
                            }
                            onAccepted: root.addWebsiteRule()
                        }

                        D.RecommandButton {
                            text: qsTr("添加网址规则")
                            // DTK renders a disabled RecommendButton with pale
                            // blue text; keep it enabled and explain the reason.
                            onClicked: {
                                if (root.websiteValid) {
                                    root.addWebsiteRule()
                                } else {
                                    root.websitePreviewError = true
                                    if (dccData.profiles.count === 0)
                                        root.websitePreview = qsTr("先导入订阅即可添加网址规则。")
                                }
                            }
                        }
                    }

                    D.Label {
                        Layout.fillWidth: true
                        text: root.websitePreview
                        visible: text.length > 0
                        wrapMode: Text.WordWrap
                        font: D.DTK.fontManager.t8
                        opacity: root.websitePreviewError ? 1 : 0.6
                        color: root.websitePreviewError ? Qt.rgba(0.82, 0.24, 0.24, 1) : palette.windowText
                    }
                }

                Component.onCompleted: root.refreshWebsiteRule()
            }
        }
    }

    DccTitleObject {
        name: "rules_listTitle"
        parentName: "dccproxy/rules"
        displayName: qsTr("规则列表")
        weight: 15
    }

    DccObject {
        name: "ruleTableGroup"
        parentName: "dccproxy/rules"
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "ruleTable"
            parentName: "dccproxy/rules/ruleTableGroup"
            weight: 10
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                implicitHeight: tableLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: tableLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: Style.gap

                    // 工具栏独占一行；没有规则时连同表头一起隐藏，空状态保持紧凑。
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Style.gap
                        visible: dccData.rules.count > 0

                        D.SearchEdit {
                            id: search
                            implicitWidth: Style.searchWidth
                            Layout.preferredWidth: Style.searchWidth
                            placeholder: qsTr("搜索规则")
                            text: dccData.rules.search
                            onTextChanged: if (dccData.rules.search !== text) dccData.rules.search = text
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    // 表头 + 紧贴其下的细分隔线，列宽与数据行完全一致。
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: dccData.rules.count > 0
                        spacing: 4

                        TableHeader {
                            Layout.fillWidth: true
                            columns: root.columns
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.10)
                        }
                    }

                    D.ListView {
                        id: ruleList
                        Layout.fillWidth: true
                        visible: count > 0
                        Layout.preferredHeight: visible ? Math.min(420, count * Style.rowHeight) : 0
                        clip: true
                        model: dccData.rules
                        ScrollBar.vertical: D.ScrollBar {}

                        onCurrentIndexChanged: {
                            if (currentIndex >= 0 && currentItem)
                                root.selectRule(currentIndex, currentItem.ruleDisabled)
                            else
                                root.clearRuleSelection()
                        }
                        onCountChanged: if (count === 0) root.clearRuleSelection()

                        delegate: Rectangle {
                            id: ruleRow

                            readonly property var rowData: model
                            readonly property bool ruleDisabled: model.disabled

                            width: ruleList.width
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
                                onClicked: {
                                    ruleList.currentIndex = index
                                    root.selectRule(index, ruleRow.ruleDisabled)
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                spacing: Style.gap
                                // 禁用的规则整体变淡，但仍可选中后重新启用。
                                opacity: ruleRow.ruleDisabled ? 0.5 : 1

                                Repeater {
                                    model: root.columns

                                    delegate: D.Label {
                                        Layout.preferredWidth: modelData.width !== undefined ? modelData.width : -1
                                        Layout.fillWidth: modelData.width === undefined
                                        text: root.cellText(ruleRow.rowData, modelData)
                                        elide: Text.ElideRight
                                        font: D.DTK.fontManager.t7
                                    }
                                }
                            }
                        }
                    }

                    EmptyHint {
                        Layout.fillWidth: true
                        visible: dccData.rules.count === 0
                        text: qsTr("没有可用的分流规则")
                        hint: qsTr("启用内核并使用带规则的订阅后，这里会显示运行中的规则。")
                    }
                }

                Timer {
                    interval: 3000
                    repeat: true
                    running: true
                    onTriggered: dccData.rules.refresh()
                }

                Component.onCompleted: dccData.rules.refresh()
            }
        }
    }

    // 操作行在卡片之外，与其它页面一致；选中状态来自根对象。
    DccObject {
        name: "rulesActions"
        parentName: "dccproxy/rules"
        weight: 25
        pageType: DccObject.Item
        canSearch: false
        page: RowLayout {
            spacing: Style.gap

            Item {
                Layout.fillWidth: true
            }

            D.Button {
                text: qsTr("启用 / 禁用规则")
                enabled: root.selectedRuleIndex >= 0
                onClicked: {
                    var next = !root.selectedRuleDisabled
                    dccData.rules.setDisabled(root.selectedRuleIndex, next)
                    root.selectedRuleDisabled = next
                }
            }

            D.Button {
                text: qsTr("高级规则编辑")
                onClicked: root.editSection()
            }
        }
    }

    DocumentDialog {
        id: documentDialog
        onAccepted: function (text) {
            if (documentDialog.editable)
                dccData.settings.saveSection("rules", text)
        }
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
