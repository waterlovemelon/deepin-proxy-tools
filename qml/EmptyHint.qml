// SPDX-License-Identifier: LGPL-3.0-or-later
// Centered placeholder for empty lists and pages without data.
import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.deepin.dtk 1.0 as D

ColumnLayout {
    id: root
    property string text: ""
    property string hint: ""
    spacing: 6

    D.Label {
        Layout.fillWidth: true
        Layout.topMargin: 24
        text: root.text
        visible: root.text.length > 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        font: D.DTK.fontManager.t7
        opacity: 0.55
    }
    D.Label {
        Layout.fillWidth: true
        Layout.bottomMargin: 24
        text: root.hint
        visible: root.hint.length > 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        font: D.DTK.fontManager.t8
        opacity: 0.45
    }
}
