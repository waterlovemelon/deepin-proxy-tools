// SPDX-License-Identifier: LGPL-3.0-or-later
// Module entry: only metadata, dccData is not available in this file.
import org.deepin.dcc 1.0

DccObject {
    name: "dccproxy" // must match the plugin name used by the control center
    parentName: "root"
    displayName: qsTr("私人代理")
    description: qsTr("节点选择、订阅与代理设置")
    icon: "proxy"
    weight: 300
    visible: true
}
