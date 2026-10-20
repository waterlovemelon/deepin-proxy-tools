// SPDX-License-Identifier: LGPL-3.0-or-later
// 订阅: import from URL or file, the profile table, the subscription options,
// activation, export and removal of the current profile.
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs
import QtQuick.Layouts 1.15

import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D

import "Style.js" as Style

DccObject {
    id: root

    readonly property var profileColumns: [
        { "title": qsTr("名称"), "width": 220 },
        { "title": qsTr("来源") },
        { "title": qsTr("更新时间"), "width": 140 }
    ]

    // Actions that operate on a row need a selected subscription.
    readonly property bool hasCurrent: dccData.profiles.count > 0 && dccData.profiles.currentId !== ""

    // Every action reports through the facade; the page keeps one status line.
    function requireCurrent() {
        var id = dccData.profiles.currentId
        if (id === "") {
            dccData.report(qsTr("请先选择一个订阅"), true)
            return ""
        }
        return id
    }

    // Returns false when the URL is empty so the field can keep the focus.
    function importSubscription(url) {
        url = (url || "").trim()
        if (url === "") {
            dccData.report(qsTr("请先输入订阅 URL"), true)
            return false
        }
        dccData.profiles.importUrl(url)
        return true
    }

    function createProfile(name) {
        name = (name || "").trim()
        if (name === "")
            return false
        dccData.profiles.createProfile(name)
        newProfileDialog.visible = false
        return true
    }

    function editCurrent() {
        var id = requireCurrent()
        if (id === "")
            return
        editorDialog.title = qsTr("编辑配置")
        editorDialog.open(dccData.profiles.document(id))
    }

    function openOptions() {
        var id = requireCurrent()
        if (id === "")
            return
        var options = {}
        try {
            options = JSON.parse(dccData.profiles.optionsJson(id))
        } catch (e) {
            options = {}
        }
        optionsDialog.profileId = id
        optionsName.text = options.name !== undefined ? options.name : ""
        optionsUrl.text = options.url !== undefined ? options.url : ""
        optionsAgent.text = options.userAgent !== undefined ? options.userAgent : ""
        optionsToken.text = options.authToken !== undefined ? options.authToken : ""
        optionsInterval.value = options.updateMinutes !== undefined ? options.updateMinutes : 0
        optionsTimeout.value = options.timeoutSeconds !== undefined ? options.timeoutSeconds : 30
        optionsAutomatic.checked = options.autoUpdate === undefined ? true : options.autoUpdate
        optionsUseProxy.checked = options.useProxy === undefined ? false : options.useProxy
        optionsDialog.visible = true
    }

    function exportProfile() {
        var id = requireCurrent()
        if (id === "")
            return
        var options = {}
        try {
            options = JSON.parse(dccData.profiles.optionsJson(id))
        } catch (e) {
            options = {}
        }
        exportFileDialog.exportId = id
        exportFileDialog.exportName = options.name !== undefined ? options.name : ""
        exportFileDialog.open()
    }

    function confirmRemove() {
        var id = requireCurrent()
        if (id === "")
            return
        var options = {}
        try {
            options = JSON.parse(dccData.profiles.optionsJson(id))
        } catch (e) {
            options = {}
        }
        deleteDialog.profileName = options.name !== undefined ? options.name : id
        deleteDialog.visible = true
    }

    DccObject {
        name: "profiles_message"
        parentName: "dccproxy/profiles"
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
        name: "profiles_importTitle"
        parentName: "dccproxy/profiles"
        displayName: qsTr("导入")
        weight: 5
    }

    // URL import, file import and 新建.
    DccObject {
        name: "profiles_import"
        parentName: "dccproxy/profiles"
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "profiles_importRow"
            parentName: "dccproxy/profiles/profiles_import"
            weight: 10
            pageType: DccObject.Item
            page: ColumnLayout {
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.topMargin: 8
                    spacing: Style.gap

                    D.Label {
                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("订阅 URL")
                        font: D.DTK.fontManager.t7
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    D.LineEdit {
                        id: urlField
                        implicitWidth: Style.controlWidth
                        placeholderText: qsTr("粘贴 Clash / Mihomo 订阅 URL")
                        onAccepted: {
                            if (!root.importSubscription(urlField.text))
                                urlField.forceActiveFocus()
                        }
                    }
                    D.RecommandButton {
                        Layout.preferredWidth: 104
                        text: qsTr("导入 URL")
                        onClicked: {
                            if (!root.importSubscription(urlField.text))
                                urlField.forceActiveFocus()
                        }
                    }
                    D.Button {
                        Layout.preferredWidth: 104
                        text: qsTr("导入文件")
                        onClicked: importFileDialog.open()
                    }
                    D.Button {
                        Layout.preferredWidth: 104
                        text: qsTr("新建")
                        onClicked: newProfileDialog.visible = true
                    }
                }

                D.Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.topMargin: 2
                    Layout.bottomMargin: 8
                    wrapMode: Text.Wrap
                    text: qsTr("URL 导入用于 Clash / Mihomo 订阅；更新会重新下载当前订阅，本地配置无需更新。")
                    font: D.DTK.fontManager.t8
                    opacity: 0.55
                }
            }
        }
    }

    DccTitleObject {
        name: "profiles_listTitle"
        parentName: "dccproxy/profiles"
        displayName: qsTr("订阅列表")
        weight: 15
    }

    // Profile table (名称 / 来源 / 更新时间) and the profile actions.
    DccObject {
        name: "profiles_list"
        parentName: "dccproxy/profiles"
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "profiles_table"
            parentName: "dccproxy/profiles/profiles_list"
            weight: 10
            pageType: DccObject.Item
            page: ColumnLayout {
                spacing: 4

                TableHeader {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.topMargin: 6
                    columns: root.profileColumns
                }

                D.ListView {
                    id: profileList
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.preferredHeight: dccData.profiles.count === 0 ? 0
                                                                         : Math.min(320, dccData.profiles.count * 40)
                    visible: dccData.profiles.count > 0
                    clip: true
                    model: dccData.profiles
                    ScrollBar.vertical: D.ScrollBar {}

                    delegate: Rectangle {
                        id: profileRow

                        readonly property bool selected: dccData.profiles.currentId === model.id

                        width: profileList.width
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

                            Item {
                                Layout.preferredWidth: 220
                                implicitHeight: nameRow.implicitHeight

                                RowLayout {
                                    id: nameRow
                                    anchors.fill: parent
                                    spacing: 4

                                    D.Label {
                                        Layout.preferredWidth: 14
                                        Layout.alignment: Qt.AlignVCenter
                                        text: model.active ? "✓" : ""
                                        color: palette.highlight
                                        font: Qt.font({
                                            "family": D.DTK.fontManager.t7.family,
                                            "pixelSize": D.DTK.fontManager.t7.pixelSize,
                                            "bold": true
                                        })
                                    }
                                    D.Label {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                        text: model.name
                                        elide: Text.ElideRight
                                        font: Qt.font({
                                            "family": D.DTK.fontManager.t7.family,
                                            "pixelSize": D.DTK.fontManager.t7.pixelSize,
                                            "bold": model.active
                                        })
                                    }
                                }
                            }

                            D.Label {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                text: model.url
                                elide: Text.ElideRight
                                font: D.DTK.fontManager.t8
                                opacity: 0.55
                            }

                            D.Label {
                                Layout.preferredWidth: 140
                                Layout.alignment: Qt.AlignVCenter
                                text: model.updated
                                elide: Text.ElideRight
                                font: D.DTK.fontManager.t8
                                opacity: 0.55
                            }
                        }

                        MouseArea {
                            id: rowHover
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dccData.profiles.currentId = model.id
                        }
                    }
                }

                EmptyHint {
                    Layout.fillWidth: true
                    visible: dccData.profiles.count === 0
                    text: qsTr("还没有订阅")
                    hint: qsTr("粘贴订阅 URL 或导入本地 YAML 文件。")
                }
            }
        }

        DccObject {
            name: "profiles_actions"
            parentName: "dccproxy/profiles/profiles_list"
            weight: 20
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
                        Layout.preferredWidth: 108
                        text: qsTr("订阅设置")
                        enabled: root.hasCurrent
                        onClicked: root.openOptions()
                    }
                    D.Button {
                        Layout.preferredWidth: 108
                        text: qsTr("编辑")
                        enabled: root.hasCurrent
                        onClicked: root.editCurrent()
                    }
                    D.Button {
                        Layout.preferredWidth: 108
                        text: qsTr("更新")
                        enabled: root.hasCurrent
                        onClicked: dccData.profiles.updateCurrent()
                    }
                    D.Button {
                        Layout.preferredWidth: 108
                        text: qsTr("导出")
                        enabled: root.hasCurrent
                        onClicked: root.exportProfile()
                    }
                    D.WarningButton {
                        text: qsTr("删除")
                        enabled: root.hasCurrent
                        onClicked: root.confirmRemove()
                    }
                    D.RecommandButton {
                        text: qsTr("使用此配置")
                        enabled: root.hasCurrent
                        onClicked: {
                            if (root.requireCurrent() !== "")
                                dccData.profiles.activateCurrent()
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: importFileDialog
        title: qsTr("导入配置文件")
        fileMode: FileDialog.OpenFile
        nameFilters: ["YAML (*.yaml *.yml)", "All (*)"]
        onAccepted: dccData.profiles.importFile(dccData.localPath(selectedFile))
    }

    FileDialog {
        id: exportFileDialog
        property string exportId: ""
        property string exportName: ""
        title: qsTr("导出配置")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "yaml"
        nameFilters: ["YAML (*.yaml *.yml)", "All (*)"]
        onAccepted: {
            var path = dccData.localPath(selectedFile)
            if (dccData.writeTextFile(path, dccData.profiles.document(exportFileDialog.exportId)))
                dccData.report(qsTr("已导出到 %1").arg(path))
        }
    }

    DocumentDialog {
        id: editorDialog
        title: qsTr("编辑配置")
        editable: true
        onAccepted: function (text) {
            dccData.profiles.saveCurrent(text)
        }
    }

    D.DialogWindow {
        id: newProfileDialog
        title: qsTr("新建配置")
        width: 420
        height: 220
        modality: Qt.ApplicationModal
        visible: false

        ColumnLayout {
            width: parent.width
            height: parent.height
            spacing: 10

            D.Label {
                Layout.fillWidth: true
                text: qsTr("名称")
                font: D.DTK.fontManager.t7
            }
            D.LineEdit {
                id: newNameField
                Layout.fillWidth: true
                onAccepted: root.createProfile(newNameField.text)
            }
            Item {
                Layout.fillHeight: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Style.gap

                Item {
                    Layout.fillWidth: true
                }
                D.Button {
                    text: qsTr("取消")
                    onClicked: newProfileDialog.visible = false
                }
                D.RecommandButton {
                    text: qsTr("创建")
                    enabled: newNameField.text.trim() !== ""
                    onClicked: root.createProfile(newNameField.text)
                }
            }
        }
    }

    D.DialogWindow {
        id: optionsDialog
        property string profileId: ""
        title: qsTr("订阅设置")
        width: 600
        height: 480
        modality: Qt.ApplicationModal
        visible: false

        function submit() {
            dccData.profiles.applyOptions(optionsDialog.profileId, {
                "name": optionsName.text,
                "url": optionsUrl.text,
                "userAgent": optionsAgent.text,
                "authToken": optionsToken.text,
                "updateMinutes": optionsInterval.value,
                "timeoutSeconds": optionsTimeout.value,
                "autoUpdate": optionsAutomatic.checked,
                "useProxy": optionsUseProxy.checked
            })
            optionsDialog.visible = false
        }

        ColumnLayout {
            width: parent.width
            height: parent.height
            spacing: 8

            RowLayout {
                spacing: Style.gap

                D.Label {
                    Layout.preferredWidth: 130
                    text: qsTr("名称")
                }
                D.LineEdit {
                    id: optionsName
                    Layout.fillWidth: true
                }
            }
            RowLayout {
                spacing: Style.gap

                D.Label {
                    Layout.preferredWidth: 130
                    text: qsTr("订阅 URL")
                }
                D.LineEdit {
                    id: optionsUrl
                    Layout.fillWidth: true
                }
            }
            RowLayout {
                spacing: Style.gap

                D.Label {
                    Layout.preferredWidth: 130
                    text: "User-Agent"
                }
                D.LineEdit {
                    id: optionsAgent
                    Layout.fillWidth: true
                    placeholderText: qsTr("留空使用 mihomo/1.19.16")
                }
            }
            RowLayout {
                spacing: Style.gap

                D.Label {
                    Layout.preferredWidth: 130
                    text: qsTr("Authorization 令牌")
                }
                D.PasswordEdit {
                    id: optionsToken
                    Layout.fillWidth: true
                }
            }
            RowLayout {
                spacing: Style.gap

                D.Label {
                    Layout.preferredWidth: 130
                    text: qsTr("更新周期")
                }
                D.SpinBox {
                    id: optionsInterval
                    implicitWidth: Style.spinWidth
                    from: 0
                    to: 10080
                    value: 0
                    textFromValue: function (value) {
                        return value === 0 ? qsTr("跟随全局") : value + qsTr(" 分钟")
                    }
                    valueFromText: function (text) {
                        var digits = text.replace(/[^0-9]/g, "")
                        return digits === "" ? 0 : Number(digits)
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            RowLayout {
                spacing: Style.gap

                D.Label {
                    Layout.preferredWidth: 130
                    text: qsTr("请求超时")
                }
                D.SpinBox {
                    id: optionsTimeout
                    implicitWidth: Style.spinWidth
                    from: 5
                    to: 180
                    value: 30
                    textFromValue: function (value) {
                        return value + qsTr(" 秒")
                    }
                    valueFromText: function (text) {
                        var digits = text.replace(/[^0-9]/g, "")
                        return digits === "" ? from : Number(digits)
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            D.CheckBox {
                id: optionsAutomatic
                text: qsTr("自动更新")
            }
            D.CheckBox {
                id: optionsUseProxy
                text: qsTr("通过正在运行的本地代理下载")
            }
            Item {
                Layout.fillHeight: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Style.gap

                Item {
                    Layout.fillWidth: true
                }
                D.Button {
                    text: qsTr("取消")
                    onClicked: optionsDialog.visible = false
                }
                D.RecommandButton {
                    text: qsTr("保存")
                    onClicked: optionsDialog.submit()
                }
            }
        }
    }

    D.DialogWindow {
        id: deleteDialog
        property string profileName: ""
        title: qsTr("删除配置")
        width: 420
        height: 220
        modality: Qt.ApplicationModal
        visible: false

        ColumnLayout {
            width: parent.width
            height: parent.height
            spacing: 10

            D.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("删除 %1？").arg(deleteDialog.profileName)
                font: D.DTK.fontManager.t7
            }
            Item {
                Layout.fillHeight: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Style.gap

                Item {
                    Layout.fillWidth: true
                }
                D.Button {
                    text: qsTr("取消")
                    onClicked: deleteDialog.visible = false
                }
                D.WarningButton {
                    text: qsTr("删除")
                    onClicked: {
                        dccData.profiles.removeCurrent()
                        deleteDialog.visible = false
                    }
                }
            }
        }
    }
}
