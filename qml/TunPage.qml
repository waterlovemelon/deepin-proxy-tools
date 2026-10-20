// SPDX-License-Identifier: LGPL-3.0-or-later
// 虚拟网卡：TUN 网卡、协议栈、路由选项与内核授权（数据来自 dccData.tun）。
import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D
import "Style.js" as Style

DccObject {
    id: root

    readonly property var stacks: ["mixed", "gvisor", "system", "mips"]

    // 页面消息优先；没有消息时把内核报错当成状态提示。
    property string messageText: ""
    property bool messageError: false

    DccObject {
        name: "tunMessage"
        parentName: "dccproxy/tun"
        weight: 0
        pageType: DccObject.Item
        canSearch: false
        page: MessageBar {
            id: message
            text: root.messageText.length > 0 ? root.messageText : dccData.service.error
            error: root.messageText.length > 0 ? root.messageError : dccData.service.error.length > 0
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
        name: "tun_statusTitle"
        parentName: "dccproxy/tun"
        displayName: qsTr("状态")
        weight: 5
    }

    DccObject {
        name: "tunStatusGroup"
        parentName: "dccproxy/tun"
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "tunStatus"
            parentName: "dccproxy/tun/tunStatusGroup"
            weight: 10
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                // 组卡片本身不带内边距，这里统一留出与 DCC 一致的 10 px。
                implicitHeight: statusLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: statusLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: Style.gap

                    D.Label {
                        Layout.fillWidth: true
                        text: dccData.tun.status
                        wrapMode: Text.WordWrap
                        font: D.DTK.fontManager.t7
                    }

                    D.Label {
                        Layout.fillWidth: true
                        visible: !dccData.service.root
                        text: qsTr("内核当前未以 root 运行，启用虚拟网卡前请先完成“内核授权”。")
                        wrapMode: Text.WordWrap
                        font: D.DTK.fontManager.t8
                        color: Qt.rgba(0.82, 0.24, 0.24, 1)
                    }
                }
            }
        }
    }

    DccTitleObject {
        name: "tun_deviceTitle"
        parentName: "dccproxy/tun"
        displayName: qsTr("虚拟网卡")
        weight: 15
    }

    DccObject {
        name: "tunOptions"
        parentName: "dccproxy/tun"
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "tunEnabled"
            parentName: "dccproxy/tun/tunOptions"
            weight: 10
            displayName: qsTr("启用虚拟网卡")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.service.tunEnabled
                enabled: !dccData.tun.busy
                onCheckedChanged: if (checked !== dccData.service.tunEnabled) dccData.tun.setEnabled(checked)
            }
        }

        DccObject {
            name: "device"
            parentName: "dccproxy/tun/tunOptions"
            weight: 20
            displayName: qsTr("网卡名称")
            pageType: DccObject.Editor
            page: D.LineEdit {
                implicitWidth: Style.controlWidth
                text: dccData.tun.device
                // Write back only when the user changed the field: an
                // unconditional write re-enters the binding and loops.
                onTextChanged: if (dccData.tun.device !== text) dccData.tun.device = text
            }
        }

        DccObject {
            name: "stack"
            parentName: "dccproxy/tun/tunOptions"
            weight: 30
            displayName: qsTr("协议栈")
            pageType: DccObject.Editor
            page: D.ComboBox {
                id: stackBox
                implicitWidth: Style.controlWidth
                flat: true
                model: root.stacks
                currentIndex: Math.max(0, root.stacks.indexOf(dccData.tun.stack))
                onActivated: dccData.tun.stack = root.stacks[stackBox.currentIndex]
            }
        }

        DccObject {
            name: "mtu"
            parentName: "dccproxy/tun/tunOptions"
            weight: 40
            displayName: "MTU"
            pageType: DccObject.Editor
            page: D.SpinBox {
                implicitWidth: Style.spinWidth
                from: 1
                to: 65535
                editable: true
                inputMethodHints: Qt.ImhDigitsOnly
                // 初始值 0 低于 from，DTK 的 IntValidator 会形成绑定环。
                validator: null
                textFromValue: function (value) {
                    return String(value)
                }
                valueFromText: function (text) {
                    var parsed = parseInt(String(text), 10)
                    return isNaN(parsed) ? -1 : parsed
                }
                value: dccData.tun.mtu
                onValueModified: dccData.tun.mtu = value
            }
        }

        DccObject {
            name: "dnsHijack"
            parentName: "dccproxy/tun/tunOptions"
            weight: 50
            displayName: qsTr("DNS 劫持（逗号分隔）")
            pageType: DccObject.Editor
            page: D.LineEdit {
                implicitWidth: Style.controlWidth
                placeholderText: "any:53"
                text: dccData.tun.dnsHijack
                onTextChanged: if (dccData.tun.dnsHijack !== text) dccData.tun.dnsHijack = text
            }
        }

        DccObject {
            name: "routeExclude"
            parentName: "dccproxy/tun/tunOptions"
            weight: 60
            displayName: qsTr("路由排除网段（CIDR）")
            description: qsTr("每行一个网段，匹配的流量不进入虚拟网卡")
            pageType: DccObject.Editor
            page: D.TextArea {
                implicitWidth: Style.controlWidth
                implicitHeight: 96
                placeholderText: "192.168.0.0/16\nfc00::/7"
                text: dccData.tun.routeExclude
                onTextChanged: if (dccData.tun.routeExclude !== text) dccData.tun.routeExclude = text
                font.pixelSize: D.DTK.fontManager.t8.pixelSize
                font.family: "monospace"
            }
        }

        DccObject {
            name: "autoRoute"
            parentName: "dccproxy/tun/tunOptions"
            weight: 70
            displayName: qsTr("自动设置路由")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.tun.autoRoute
                onCheckedChanged: if (dccData.tun.autoRoute !== checked) dccData.tun.autoRoute = checked
            }
        }

        DccObject {
            name: "autoRedirect"
            parentName: "dccproxy/tun/tunOptions"
            weight: 80
            displayName: qsTr("自动重定向（Linux）")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.tun.autoRedirect
                onCheckedChanged: if (dccData.tun.autoRedirect !== checked) dccData.tun.autoRedirect = checked
            }
        }

        DccObject {
            name: "autoDetectInterface"
            parentName: "dccproxy/tun/tunOptions"
            weight: 90
            displayName: qsTr("自动检测出口网卡")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.tun.autoDetectInterface
                onCheckedChanged: if (dccData.tun.autoDetectInterface !== checked) dccData.tun.autoDetectInterface = checked
            }
        }

        DccObject {
            name: "strictRoute"
            parentName: "dccproxy/tun/tunOptions"
            weight: 100
            displayName: qsTr("严格路由")
            pageType: DccObject.Editor
            page: D.Switch {
                checked: dccData.tun.strictRoute
                onCheckedChanged: if (dccData.tun.strictRoute !== checked) dccData.tun.strictRoute = checked
            }
        }
    }

    DccObject {
        name: "tunActions"
        parentName: "dccproxy/tun"
        weight: 25
        pageType: DccObject.Item
        canSearch: false
        page: RowLayout {
            spacing: Style.gap

            Item {
                Layout.fillWidth: true
            }

            D.RecommandButton {
                text: qsTr("保存并应用")
                enabled: !dccData.tun.busy
                onClicked: dccData.tun.apply()
            }

            D.Button {
                text: qsTr("内核授权")
                enabled: !dccData.tun.busy
                onClicked: dccData.tun.authorize()
            }
        }
    }

    DccObject {
        name: "tunHint"
        parentName: "dccproxy/tun"
        weight: 30
        pageType: DccObject.Item
        canSearch: false
        page: Item {
            implicitHeight: hintLabel.implicitHeight + 2 * Style.margin

            D.Label {
                id: hintLabel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Style.margin
                text: qsTr("启用时会联动开启内核 DNS，关闭虚拟网卡后保留 DNS。自动路由开启后，应用无需设置系统代理；系统代理绕过请通过 DIRECT 规则或路由排除配置。修改后点击保存并应用。")
                wrapMode: Text.WordWrap
                font: D.DTK.fontManager.t8
                opacity: 0.6
            }
        }
    }
}
