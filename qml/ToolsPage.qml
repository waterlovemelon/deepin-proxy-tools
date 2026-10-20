// SPDX-License-Identifier: LGPL-3.0-or-later
// 工具：DNS 查询、缓存清理与 Sub-Store 导入。
import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D
import "Style.js" as Style

DccObject {
    id: root

    readonly property var dnsTypes: ["A", "AAAA", "CNAME", "TXT", "MX", "HTTPS"]

    // 每个 page 是独立组件，控件 id 不能跨组件访问，查询参数放在根对象上。
    property string domainName: "example.com"
    property int dnsTypeIndex: 0

    // 只有本页发起的查询才弹出结果窗口。
    property bool dnsPending: false

    function query() {
        if (domainName.length === 0)
            return
        dnsPending = true
        dccData.tools.queryDns(domainName, root.dnsTypes[dnsTypeIndex])
    }

    DccObject {
        name: "toolsMessage"
        parentName: "dccproxy/tools"
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
        name: "tools_dnsTitle"
        parentName: "dccproxy/tools"
        displayName: qsTr("DNS 查询")
        weight: 5
    }

    DccObject {
        name: "dnsQueryGroup"
        parentName: "dccproxy/tools"
        weight: 10
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "dnsDomain"
            parentName: "dccproxy/tools/dnsQueryGroup"
            weight: 10
            displayName: qsTr("域名")
            pageType: DccObject.Editor
            page: RowLayout {
                spacing: Style.gap

                D.LineEdit {
                    implicitWidth: Style.controlWidth
                    text: root.domainName
                    placeholderText: "example.com"
                    onTextChanged: root.domainName = text
                    onAccepted: root.query()
                }

                D.RecommandButton {
                    text: qsTr("查询")
                    enabled: root.domainName.length > 0
                    onClicked: root.query()
                }
            }
        }

        DccObject {
            name: "dnsType"
            parentName: "dccproxy/tools/dnsQueryGroup"
            weight: 20
            displayName: qsTr("类型")
            pageType: DccObject.Editor
            page: D.ComboBox {
                id: typeBox
                implicitWidth: Style.spinWidth
                flat: true
                model: root.dnsTypes
                currentIndex: root.dnsTypeIndex
                onActivated: root.dnsTypeIndex = typeBox.currentIndex
            }
        }

        DccObject {
            name: "dnsHint"
            parentName: "dccproxy/tools/dnsQueryGroup"
            weight: 30
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                implicitHeight: dnsHintLabel.implicitHeight + 2 * Style.margin

                D.Label {
                    id: dnsHintLabel
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    text: qsTr("查询结果会在独立窗口中显示。")
                    wrapMode: Text.WordWrap
                    font: D.DTK.fontManager.t8
                    opacity: 0.6
                }
            }
        }
    }

    DccTitleObject {
        name: "tools_cacheTitle"
        parentName: "dccproxy/tools"
        displayName: qsTr("缓存")
        weight: 15
    }

    DccObject {
        name: "cacheGroup"
        parentName: "dccproxy/tools"
        weight: 20
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "cacheActions"
            parentName: "dccproxy/tools/cacheGroup"
            weight: 10
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                implicitHeight: cacheLayout.implicitHeight + 2 * Style.margin

                RowLayout {
                    id: cacheLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: Style.gap

                    D.Label {
                        text: qsTr("清除内核的 DNS 与 FakeIP 缓存")
                        font: D.DTK.fontManager.t7
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    D.Button {
                        text: qsTr("清除 DNS 缓存")
                        onClicked: dccData.tools.flushCache("dns")
                    }

                    D.Button {
                        text: qsTr("清除 FakeIP 缓存")
                        onClicked: dccData.tools.flushCache("fakeip")
                    }
                }
            }
        }
    }

    DccTitleObject {
        name: "tools_subStoreTitle"
        parentName: "dccproxy/tools"
        displayName: qsTr("Sub-Store")
        weight: 25
    }

    DccObject {
        name: "subStoreGroup"
        parentName: "dccproxy/tools"
        weight: 30
        pageType: DccObject.Item
        page: DccGroupView {}

        DccObject {
            name: "subStore"
            parentName: "dccproxy/tools/subStoreGroup"
            weight: 10
            pageType: DccObject.Item
            canSearch: false
            page: Item {
                implicitHeight: subStoreLayout.implicitHeight + 2 * Style.margin

                ColumnLayout {
                    id: subStoreLayout
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Style.margin
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Style.gap

                        D.LineEdit {
                            id: subStoreField
                            Layout.fillWidth: true
                            implicitWidth: Style.controlWidth
                            placeholderText: qsTr("Clash / Mihomo 配置 URL")
                        }

                        D.RecommandButton {
                            text: qsTr("从 Sub-Store 导入")
                            onClicked: {
                                if (subStoreField.text.trim().length === 0)
                                    dccData.report(qsTr("请先填写 Sub-Store 配置 URL"), true)
                                else
                                    dccData.profiles.importUrl(subStoreField.text, "Sub-Store")
                            }
                        }
                    }

                    D.Label {
                        Layout.fillWidth: true
                        text: qsTr("支持导入 Sub-Store 导出的完整 Mihomo 配置。")
                        wrapMode: Text.WordWrap
                        font: D.DTK.fontManager.t8
                        opacity: 0.6
                    }
                }
            }
        }
    }

    DccObject {
        name: "toolsHint"
        parentName: "dccproxy/tools"
        weight: 35
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
                text: qsTr("完整的 DNS、嗅探、TUN、监听器和隧道配置可在概览页编辑。")
                wrapMode: Text.WordWrap
                font: D.DTK.fontManager.t8
                opacity: 0.6
            }
        }
    }

    DocumentDialog {
        id: dnsDialog
        title: qsTr("DNS 查询结果")
    }

    Connections {
        target: dccData.tools
        function onChanged() {
            if (!root.dnsPending)
                return
            root.dnsPending = false
            dnsDialog.openText(dccData.tools.dnsResult)
        }
    }
}
