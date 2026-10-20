// SPDX-License-Identifier: LGPL-3.0-or-later
// Column header for list pages. `columns` is a JavaScript array of objects:
//   { title: "名称", width: 180 }      fixed width
//   { title: "目标", ratio: 2 }        flexible width (shares the remainder)
// Rows must use the same width/ratio values so header and rows line up.
import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.deepin.dtk 1.0 as D

Item {
    id: root
    property var columns: []
    implicitHeight: 30

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Repeater {
            model: root.columns
            delegate: D.Label {
                text: modelData.title !== undefined ? modelData.title : ""
                font: D.DTK.fontManager.t8
                opacity: 0.55
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                Layout.preferredWidth: modelData.width !== undefined ? modelData.width : -1
                Layout.fillWidth: modelData.width === undefined
                Layout.preferredHeight: root.implicitHeight
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.10)
    }
}
