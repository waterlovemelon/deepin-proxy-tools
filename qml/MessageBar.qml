// SPDX-License-Identifier: LGPL-3.0-or-later
// One-line status/error message used at the top of every page.
import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.deepin.dtk 1.0 as D
import "Style.js" as Style

Item {
    id: root
    property string text: ""
    property bool error: false
    visible: text.length > 0
    implicitHeight: visible ? label.implicitHeight + Style.gap : 0

    D.Label {
        id: label
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        text: root.text
        visible: root.visible
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
        font: D.DTK.fontManager.t8
        color: root.error ? Qt.rgba(0.82, 0.24, 0.24, 1) : palette.windowText
        opacity: root.error ? 1 : 0.65
    }
}
