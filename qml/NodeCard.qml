// SPDX-License-Identifier: LGPL-3.0-or-later
// One node of a policy group. ProxyGroupsPage fills the card from the NodeModel
// roles (name/displayName/flag/type/delayText/testing/active/error); every
// action goes through dccData.proxies, which also reports the preview mode.
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import org.deepin.dcc 1.0
import org.deepin.dtk 1.0 as D

Item {
    id: card

    // Filled by the owning group card.
    property string group: ""
    property string nodeName: ""
    property string displayName: ""
    property string flag: ""
    property string nodeType: ""
    property string delayText: ""
    property bool testing: false
    property bool active: false
    property string error: ""

    readonly property bool previewing: dccData.proxies.previewing

    implicitWidth: 180
    implicitHeight: 48

    // A 4 px inset keeps neighbouring cards apart while the delegate can fill
    // the whole grid cell.
    Rectangle {
        id: surface
        anchors.fill: parent
        anchors.margins: 4
        radius: 8
        color: card.active ? Qt.rgba(palette.highlight.r, palette.highlight.g,
                                      palette.highlight.b, 0.10)
                           : (bodyHover.containsMouse
                              ? Qt.rgba(palette.highlight.r, palette.highlight.g,
                                        palette.highlight.b, 0.08)
                              : palette.base)
        border.width: card.active ? 1.5 : 1
        border.color: card.active
                      ? palette.highlight
                      : Qt.rgba(palette.windowText.r, palette.windowText.g,
                                palette.windowText.b, 0.12)
        opacity: card.previewing ? 0.7 : 1.0

        // The card body selects the node; the delay area and the error mark sit
        // above it and keep their own clicks.
        MouseArea {
            id: bodyHover
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: dccData.proxies.selectNode(card.group, card.nodeName)
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 6

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 20
                Layout.preferredHeight: 15
                source: card.flag
                sourceSize: Qt.size(20, 15)
                fillMode: Image.PreserveAspectFit
            }

            D.Label {
                id: nameLabel
                Layout.fillWidth: true
                Layout.minimumWidth: 30
                Layout.alignment: Qt.AlignVCenter
                text: card.displayName
                elide: Text.ElideRight
                font: Qt.font({
                    "family": D.DTK.fontManager.t7.family,
                    "pixelSize": D.DTK.fontManager.t7.pixelSize,
                    "bold": card.active
                })
                opacity: card.active ? 1.0 : 0.9

                ToolTip.delay: 800
                ToolTip.visible: nameHover.containsMouse && text !== ""
                ToolTip.text: card.displayName

                MouseArea {
                    id: nameHover
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }

            D.Label {
                Layout.maximumWidth: 68
                Layout.alignment: Qt.AlignVCenter
                text: card.nodeType
                visible: text !== ""
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
                font: D.DTK.fontManager.t8
                opacity: 0.55
            }

            D.Label {
                Layout.alignment: Qt.AlignVCenter
                text: "✓"
                visible: card.active
                color: palette.highlight
                font: Qt.font({
                    "family": D.DTK.fontManager.t8.family,
                    "pixelSize": D.DTK.fontManager.t8.pixelSize,
                    "bold": true
                })
            }

            D.Label {
                Layout.alignment: Qt.AlignVCenter
                text: "⚠"
                visible: card.error !== ""
                color: Qt.rgba(0.82, 0.24, 0.24, 1)
                font: D.DTK.fontManager.t8

                ToolTip.visible: errorHover.containsMouse
                ToolTip.text: card.error

                MouseArea {
                    id: errorHover
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }

            // Only this area starts a speed test for the single node.
            Item {
                Layout.preferredWidth: 58
                Layout.fillHeight: true

                D.Label {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: card.delayText
                    visible: !card.testing
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, parent.width)
                    horizontalAlignment: Text.AlignRight
                    font: D.DTK.fontManager.t8
                    opacity: 0.7
                }

                D.BusyIndicator {
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    running: card.testing
                    visible: card.testing
                }

                MouseArea {
                    id: delayHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: dccData.proxies.testNode(card.group, card.nodeName)

                    ToolTip.delay: 800
                    ToolTip.visible: delayHover.containsMouse && !card.testing
                    ToolTip.text: qsTr("点击测速")
                }
            }
        }
    }
}
