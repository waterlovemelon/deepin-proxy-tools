// SPDX-License-Identifier: LGPL-3.0-or-later
// 设置：控制器、端口、系统代理、绕过、备份与 WebDAV（数据来自 dccData.settings）。
import QtQuick 2.15
import QtQuick.Dialogs
import QtQuick.Layouts 1.15
import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D
import "Style.js" as Style

DccObject {
    id: root

    // 运行配置 / 生效配置预览：只有本页发起的文档请求才打开窗口。
    property bool documentPending: false

    // WebDAV 表单值放在根对象上：每个 page 是独立组件，字段 id 不能跨组件访问。
    property string davUrl: ""
    property string davUser: ""
    property string davPassword: ""

    // 端口与更新周期显示纯数字：不加千位分隔符，0 显示为“关闭”。
    function portText(value) {
        return value === 0 ? qsTr("关闭") : String(value)
    }

    function portValue(text) {
        var trimmed = String(text).trim()
        if (trimmed.length === 0 || trimmed === qsTr("关闭"))
            return 0
        var parsed = parseInt(trimmed, 10)
        return isNaN(parsed) ? -1 : parsed
    }

    function minutesText(value) {
        return value === 0 ? qsTr("关闭自动更新") : String(value) + qsTr(" 分钟")
    }

    function minutesValue(text) {
        var parsed = parseInt(String(text), 10)
        return isNaN(parsed) ? 0 : parsed
    }

    function editProfile() {
        // 备份恢复 / 订阅导入都可能改变本地配置，先重新读取再编辑。
        dccData.profiles.reload()
        if (dccData.profiles.count === 0) {
            dccData.report(qsTr("请先在“订阅”页导入配置"), true)
            return
        }
        documentDialog.editingProfile = true
        documentDialog.title = qsTr("编辑完整配置")
        documentDialog.helpText = ""
        documentDialog.example = ""
        documentDialog.editable = true
        documentDialog.openText(dccData.profiles.document(dccData.profiles.currentId))
    }

    function showRuntimeConfig() {
        documentPending = true
        dccData.settings.showRuntimeConfig()
    }

    DccObject {
        name: "settingsMessage"
        parentName: "dccproxy/settings"
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
        name: "settings_controllerTitle"
        parentName: "dccproxy/settings"
        displayName: qsTr("控制器")
        weight: 5
    }

    DccObject {
        name: "controllerGroup"
        parentName: "dccproxy/settings"
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "endpoint"
            parentName: "dccproxy/settings/controllerGroup"
            weight: 10
            displayName: qsTr("控制器地址")
            pageType: DccObject.Editor
            page: D.LineEdit {
                implicitWidth: Style.controlWidth
                placeholderText: "http://127.0.0.1:16666"
                text: dccData.settings.endpoint
                onTextChanged: if (dccData.settings.endpoint !== text) dccData.settings.endpoint = text
            }
        }

        DccObject {
            name: "secret"
            parentName: "dccproxy/settings/controllerGroup"
            weight: 20
            displayName: qsTr("API 密钥")
            pageType: DccObject.Editor
            page: D.PasswordEdit {
                id: secretField
                implicitWidth: Style.controlWidth
                // 只保留“显示密码”的眼睛按钮，不再叠一个清空图标。
                clearButton.active: false
                text: dccData.settings.secret
                onTextChanged: if (dccData.settings.secret !== text) dccData.settings.secret = text
            }
        }

        DccObject {
            name: "corePath"
            parentName: "dccproxy/settings/controllerGroup"
            weight: 30
            displayName: qsTr("内核路径")
            pageType: DccObject.Editor
            page: RowLayout {
                spacing: Style.gap

                D.LineEdit {
                    Layout.preferredWidth: 300
                    implicitWidth: Style.controlWidth
                    placeholderText: "/usr/lib/dcc-proxy/mihomo"
                    text: dccData.settings.corePath
                    onTextChanged: if (dccData.settings.corePath !== text) dccData.settings.corePath = text
                }

                D.Button {
                    text: qsTr("选择")
                    onClicked: coreDialog.open()
                }
            }
        }
    }

    DccTitleObject {
        name: "settings_portsTitle"
        parentName: "dccproxy/settings"
        displayName: qsTr("端口")
        weight: 15
    }

    DccObject {
        name: "portsGroup"
        parentName: "dccproxy/settings"
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "mixedPort"
            parentName: "dccproxy/settings/portsGroup"
            weight: 10
            displayName: qsTr("混合端口（HTTP / SOCKS）")
            pageType: DccObject.Editor
            page: D.SpinBox {
                implicitWidth: Style.spinWidth
                from: 1
                to: 65535
                editable: true
                inputMethodHints: Qt.ImhDigitsOnly
                // 初始值 0 低于 from，DTK 的 IntValidator 会把文本反复打回形成绑定环。
                validator: null
                textFromValue: function (value) {
                    return String(value)
                }
                valueFromText: function (text) {
                    var parsed = parseInt(String(text), 10)
                    return isNaN(parsed) ? -1 : parsed
                }
                value: dccData.settings.mixedPort
                onValueModified: dccData.settings.mixedPort = value
            }
        }

        DccObject {
            name: "httpPort"
            parentName: "dccproxy/settings/portsGroup"
            weight: 20
            displayName: qsTr("HTTP 端口")
            pageType: DccObject.Editor
            page: D.SpinBox {
                implicitWidth: Style.spinWidth
                from: 0
                to: 65535
                editable: true
                inputMethodHints: Qt.ImhDigitsOnly
                // “关闭”不是数字，DTK 默认的 IntValidator 会反复回退文本形成绑定环。
                validator: null
                textFromValue: function (value) {
                    return root.portText(value)
                }
                valueFromText: function (text) {
                    return root.portValue(text)
                }
                value: dccData.settings.httpPort
                onValueModified: dccData.settings.httpPort = value
            }
        }

        DccObject {
            name: "socksPort"
            parentName: "dccproxy/settings/portsGroup"
            weight: 30
            displayName: qsTr("SOCKS 端口")
            pageType: DccObject.Editor
            page: D.SpinBox {
                implicitWidth: Style.spinWidth
                from: 0
                to: 65535
                editable: true
                inputMethodHints: Qt.ImhDigitsOnly
                // “关闭”不是数字，DTK 默认的 IntValidator 会反复回退文本形成绑定环。
                validator: null
                textFromValue: function (value) {
                    return root.portText(value)
                }
                valueFromText: function (text) {
                    return root.portValue(text)
                }
                value: dccData.settings.socksPort
                onValueModified: dccData.settings.socksPort = value
            }
        }

        DccObject {
            name: "redirPort"
            parentName: "dccproxy/settings/portsGroup"
            weight: 40
            displayName: qsTr("重定向端口")
            pageType: DccObject.Editor
            page: D.SpinBox {
                implicitWidth: Style.spinWidth
                from: 0
                to: 65535
                editable: true
                inputMethodHints: Qt.ImhDigitsOnly
                // “关闭”不是数字，DTK 默认的 IntValidator 会反复回退文本形成绑定环。
                validator: null
                textFromValue: function (value) {
                    return root.portText(value)
                }
                valueFromText: function (text) {
                    return root.portValue(text)
                }
                value: dccData.settings.redirPort
                onValueModified: dccData.settings.redirPort = value
            }
        }

        DccObject {
            name: "tproxyPort"
            parentName: "dccproxy/settings/portsGroup"
            weight: 50
            displayName: qsTr("透明代理端口")
            pageType: DccObject.Editor
            page: D.SpinBox {
                implicitWidth: Style.spinWidth
                from: 0
                to: 65535
                editable: true
                inputMethodHints: Qt.ImhDigitsOnly
                // “关闭”不是数字，DTK 默认的 IntValidator 会反复回退文本形成绑定环。
                validator: null
                textFromValue: function (value) {
                    return root.portText(value)
                }
                valueFromText: function (text) {
                    return root.portValue(text)
                }
                value: dccData.settings.tproxyPort
                onValueModified: dccData.settings.tproxyPort = value
            }
        }
    }

    DccObject {
        name: "portsActions"
        parentName: "dccproxy/settings"
        weight: 25
        pageType: DccObject.Item
        canSearch: false
        page: ColumnLayout {
            spacing: Style.gap

            RowLayout {
                Layout.fillWidth: true
                spacing: Style.gap

                Item {
                    Layout.fillWidth: true
                }

                D.Button {
                    text: qsTr("保存并应用端口")
                    onClicked: dccData.settings.applyPorts()
                }
            }

            D.Label {
                Layout.fillWidth: true
                text: qsTr("其它端口填 0 关闭。运行中修改会重启本插件内核，并同步已启用的系统代理；失败恢复原配置。")
                wrapMode: Text.WordWrap
                font: D.DTK.fontManager.t8
                opacity: 0.6
            }
        }
    }

    DccTitleObject {
        name: "settings_behaviorTitle"
        parentName: "dccproxy/settings"
        displayName: qsTr("行为")
        weight: 35
    }

    DccObject {
        name: "behaviorGroup"
        parentName: "dccproxy/settings"
        weight: 40
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "unifiedDelay"
            parentName: "dccproxy/settings/behaviorGroup"
            weight: 10
            displayName: qsTr("统一延迟")
            description: qsTr("与 Clash Party 一致，重启内核后生效")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.settings.unifiedDelay
                onCheckedChanged: if (dccData.settings.unifiedDelay !== checked) dccData.settings.unifiedDelay = checked
            }
        }

        DccObject {
            name: "autoSystemProxy"
            parentName: "dccproxy/settings/behaviorGroup"
            weight: 20
            displayName: qsTr("启动内核时自动启用系统代理")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.settings.autoSystemProxy
                onCheckedChanged: if (dccData.settings.autoSystemProxy !== checked) dccData.settings.autoSystemProxy = checked
            }
        }

        DccObject {
            name: "systemNodeDns"
            parentName: "dccproxy/settings/behaviorGroup"
            weight: 30
            displayName: qsTr("节点域名使用系统 DNS")
            description: qsTr("重启内核后生效")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.settings.systemNodeDns
                onCheckedChanged: if (dccData.settings.systemNodeDns !== checked) dccData.settings.systemNodeDns = checked
            }
        }

        DccObject {
            name: "autoStart"
            parentName: "dccproxy/settings/behaviorGroup"
            weight: 40
            displayName: qsTr("登录后自动启动上次使用的配置")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.settings.autoStart
                onCheckedChanged: if (dccData.settings.autoStart !== checked) dccData.settings.autoStart = checked
            }
        }

        DccObject {
            name: "updateMinutes"
            parentName: "dccproxy/settings/behaviorGroup"
            weight: 50
            displayName: qsTr("订阅更新周期")
            pageType: DccObject.Editor
            page: D.SpinBox {
                implicitWidth: Style.spinWidth
                from: 0
                to: 10080
                editable: true
                inputMethodHints: Qt.ImhDigitsOnly
                // “关闭自动更新”不是数字，跳过 IntValidator 避免绑定环。
                validator: null
                textFromValue: function (value) {
                    return root.minutesText(value)
                }
                valueFromText: function (text) {
                    return root.minutesValue(text)
                }
                value: dccData.settings.updateMinutes
                onValueModified: dccData.settings.updateMinutes = value
            }
        }
    }

    DccTitleObject {
        name: "settings_actionsTitle"
        parentName: "dccproxy/settings"
        displayName: qsTr("操作")
        weight: 45
    }

    DccObject {
        name: "settingsActions"
        parentName: "dccproxy/settings"
        weight: 50
        pageType: DccObject.Item
        canSearch: false
        page: RowLayout {
            spacing: Style.gap

            Item {
                Layout.fillWidth: true
            }

            D.RecommandButton {
                text: qsTr("保存并连接")
                onClicked: dccData.settings.apply()
            }

            D.Button {
                text: qsTr("编辑完整配置")
                onClicked: root.editProfile()
            }

            D.Button {
                text: qsTr("运行配置")
                onClicked: root.showRuntimeConfig()
            }
        }
    }

    DccTitleObject {
        name: "settings_systemProxyTitle"
        parentName: "dccproxy/settings"
        displayName: qsTr("系统代理")
        weight: 55
    }

    DccObject {
        name: "systemProxy"
        parentName: "dccproxy/settings"
        weight: 60
        pageType: DccObject.Item
        canSearch: false
        page: ColumnLayout {
            spacing: Style.gap

            RowLayout {
                Layout.fillWidth: true
                spacing: Style.gap

                Item {
                    Layout.fillWidth: true
                }

                D.Button {
                    text: qsTr("启用系统代理")
                    onClicked: dccData.service.setSystemProxy("manual", dccData.settings.mixedPort)
                }

                D.Button {
                    text: qsTr("关闭系统代理")
                    onClicked: dccData.service.setSystemProxy("none", dccData.settings.mixedPort)
                }

                D.Button {
                    text: qsTr("PAC")
                    onClicked: dccData.service.setSystemProxy("auto", dccData.settings.mixedPort)
                }
            }

            D.Label {
                Layout.fillWidth: true
                text: qsTr("系统代理写入当前桌面环境，关闭后恢复为直连；使用“代理绕过”放行局域网与本地地址。")
                wrapMode: Text.WordWrap
                font: D.DTK.fontManager.t8
                opacity: 0.6
            }
        }
    }

    DccTitleObject {
        name: "settings_bypassTitle"
        parentName: "dccproxy/settings"
        displayName: qsTr("代理绕过")
        weight: 65
    }

    DccObject {
        name: "bypassGroup"
        parentName: "dccproxy/settings"
        weight: 70
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "bypass"
            parentName: "dccproxy/settings/bypassGroup"
            weight: 10
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                // 组卡片本身不带内边距，这里统一留出与 DCC 一致的 10 px。
                implicitHeight: bypassLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: bypassLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: Style.gap

                    D.Label {
                        Layout.fillWidth: true
                        text: qsTr("每行一项，支持域名、*.example.com、IP 和 CIDR 网段。匹配的地址直连，适用于系统代理和内置 PAC。TUN 和应用手动指定代理时请在规则中设置 DIRECT。")
                        wrapMode: Text.WordWrap
                        font: D.DTK.fontManager.t8
                        opacity: 0.6
                    }

                    D.TextArea {
                        id: bypassEditor
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        wrapMode: TextEdit.WrapAnywhere
                        text: dccData.settings.bypass
                        onTextChanged: if (dccData.settings.bypass !== text) dccData.settings.bypass = text
                        font.pixelSize: D.DTK.fontManager.t8.pixelSize
                        font.family: "monospace"
                    }

                    // 绕过内容格式错误时就在编辑器下方提示，避免误保存。
                    MessageBar {
                        Layout.fillWidth: true
                        text: dccData.settings.bypassError
                        error: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Style.gap

                        Item {
                            Layout.fillWidth: true
                        }

                        D.RecommandButton {
                            text: qsTr("保存并应用绕过")
                            enabled: dccData.settings.bypassValid
                            onClicked: dccData.settings.applyBypass()
                        }

                        D.Button {
                            text: qsTr("填入默认绕过")
                            onClicked: dccData.settings.fillDefaultBypass()
                        }
                    }
                }
            }
        }
    }

    DccTitleObject {
        name: "settings_backupTitle"
        parentName: "dccproxy/settings"
        displayName: qsTr("备份")
        weight: 75
    }

    DccObject {
        name: "backup"
        parentName: "dccproxy/settings"
        weight: 80
        pageType: DccObject.Item
        canSearch: false
        page: ColumnLayout {
            spacing: Style.gap

            RowLayout {
                Layout.fillWidth: true
                spacing: Style.gap

                Item {
                    Layout.fillWidth: true
                }

                D.Button {
                    text: qsTr("本地备份")
                    onClicked: backupDialog.open()
                }

                D.Button {
                    text: qsTr("恢复备份")
                    onClicked: restoreDialog.open()
                }
            }

            D.Label {
                Layout.fillWidth: true
                text: qsTr("备份包含订阅与密钥，请妥善保存。恢复会覆盖当前配置，原配置自动备份。")
                wrapMode: Text.WordWrap
                font: D.DTK.fontManager.t8
                opacity: 0.6
            }
        }
    }

    DccTitleObject {
        name: "settings_webdavTitle"
        parentName: "dccproxy/settings"
        displayName: qsTr("WebDAV")
        weight: 85
    }

    DccObject {
        name: "webdavGroup"
        parentName: "dccproxy/settings"
        weight: 90
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "davUrl"
            parentName: "dccproxy/settings/webdavGroup"
            weight: 10
            displayName: qsTr("WebDAV 文件 URL")
            pageType: DccObject.Editor
            page: D.LineEdit {
                implicitWidth: Style.controlWidth
                placeholderText: "https://server/path/dcc-proxy-backup.json"
                text: root.davUrl
                onTextChanged: root.davUrl = text
            }
        }

        DccObject {
            name: "davUser"
            parentName: "dccproxy/settings/webdavGroup"
            weight: 20
            displayName: qsTr("用户名")
            pageType: DccObject.Editor
            page: D.LineEdit {
                implicitWidth: Style.controlWidth
                text: root.davUser
                onTextChanged: root.davUser = text
            }
        }

        DccObject {
            name: "davPassword"
            parentName: "dccproxy/settings/webdavGroup"
            weight: 30
            displayName: qsTr("密码")
            pageType: DccObject.Editor
            page: D.PasswordEdit {
                implicitWidth: Style.controlWidth
                // 与 API 密钥一致：只保留显示密码的眼睛按钮。
                clearButton.active: false
                text: root.davPassword
                onTextChanged: root.davPassword = text
            }
        }

        DccObject {
            name: "webdavActions"
            parentName: "dccproxy/settings/webdavGroup"
            weight: 40
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                implicitHeight: webdavLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: webdavLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: Style.gap

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Style.gap

                        Item {
                            Layout.fillWidth: true
                        }

                        D.Button {
                            text: qsTr("上传备份")
                            enabled: root.davUrl.length > 0
                            onClicked: dccData.settings.webdav(root.davUrl, root.davUser, root.davPassword, true)
                        }

                        D.Button {
                            text: qsTr("下载并恢复")
                            enabled: root.davUrl.length > 0
                            onClicked: confirmDialog.ask(qsTr("恢复备份"), qsTr("下载并覆盖本地配置？"), function () {
                                dccData.settings.webdav(root.davUrl, root.davUser, root.davPassword, false)
                            })
                        }
                    }

                    D.Label {
                        Layout.fillWidth: true
                        text: qsTr("WebDAV 需要 HTTPS 文件 URL，且 URL 中不能带用户名和密码。")
                        wrapMode: Text.WordWrap
                        font: D.DTK.fontManager.t8
                        opacity: 0.6
                    }
                }
            }
        }
    }

    DocumentDialog {
        id: documentDialog
        property bool editingProfile: false
        onAccepted: function (text) {
            if (documentDialog.editingProfile)
                dccData.profiles.saveCurrent(text)
        }
    }

    D.DialogWindow {
        id: confirmDialog
        property string message: ""
        property var run: null
        width: 420
        modality: Qt.WindowModal

        function ask(title, message, run) {
            confirmDialog.title = title
            confirmDialog.message = message
            confirmDialog.run = run
            confirmDialog.show()
        }

        onClosing: function (close) {
            close.accepted = false
            confirmDialog.hide()
        }

        ColumnLayout {
            spacing: 10
            width: confirmDialog.width - confirmDialog.leftPadding - confirmDialog.rightPadding

            D.Label {
                Layout.fillWidth: true
                text: confirmDialog.message
                wrapMode: Text.WordWrap
            }

            D.DialogButtonBox {
                Layout.alignment: Qt.AlignRight
                standardButtons: D.DialogButtonBox.Ok | D.DialogButtonBox.Cancel
                onAccepted: {
                    confirmDialog.hide()
                    if (confirmDialog.run)
                        confirmDialog.run()
                }
                onRejected: confirmDialog.hide()
            }
        }
    }

    FileDialog {
        id: coreDialog
        title: qsTr("选择 Mihomo / Smart 内核")
        fileMode: FileDialog.OpenFile
        onAccepted: dccData.settings.chooseCore(dccData.localPath(selectedFile))
    }

    FileDialog {
        id: backupDialog
        title: qsTr("备份")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("备份文件 (*.json)"), qsTr("所有文件 (*)")]
        onAccepted: {
            dccData.settings.backupTo(dccData.localPath(selectedFile))
            dccData.report(qsTr("备份完成（包含订阅和密钥，请妥善保存）"), false)
        }
    }

    FileDialog {
        id: restoreDialog
        title: qsTr("恢复备份")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("备份文件 (*.json)"), qsTr("所有文件 (*)")]
        onAccepted: confirmDialog.ask(qsTr("恢复备份"), qsTr("覆盖本地配置？当前配置将自动备份。"), function () {
            dccData.settings.restoreFrom(dccData.localPath(selectedFile))
            dccData.profiles.reload()
            dccData.report(qsTr("已恢复，重启内核后生效"), false)
        })
    }

    // WebDAV 下载并恢复是异步的：完成消息是唯一的结束信号，此时重新读取订阅列表。
    Connections {
        target: dccData
        function onMessage(text, error) {
            if (!error && text === qsTr("备份恢复完成"))
                dccData.profiles.reload()
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
            documentDialog.editingProfile = false
            documentDialog.title = document.title
            documentDialog.helpText = document.help
            documentDialog.editable = document.editable
            documentDialog.example = document.example
            documentDialog.openText(document.text)
        }
    }
}
