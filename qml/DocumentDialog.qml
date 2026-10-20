// SPDX-License-Identifier: LGPL-3.0-or-later
// 可复用的文本文档窗口（运行配置 / 生效配置 / 后台日志 / 分节编辑）。
// 窗口只负责显示与编辑自己编辑器里的内容，从不直接改写数据：
// 调用方通过 accepted(text) 拿到结果后自行保存。
import QtQuick 2.15
import QtQuick.Layouts 1.15
import org.deepin.dtk 1.0 as D

D.DialogWindow {
    id: dialog

    // 编辑器上方的说明文字，为空时不占位置。
    property string helpText
    // true：显示“保存 / 取消”，accepted(text) 交给调用方处理；false：只读预览。
    property bool editable: false
    // 可选示例文本，供“填入示例”按钮使用。
    property string example

    signal accepted(string text)
    signal rejected()

    width: 760
    height: 620
    modality: Qt.WindowModal

    // 载入文本并显示窗口；openText 与 open 同义，调用方可按语义取名。
    function open(text) {
        editor.text = (text === undefined || text === null) ? "" : text
        dialog.show()
        dialog.raise()
        dialog.requestActivate()
    }

    function openText(text) {
        open(text)
    }

    onClosing: function (close) {
        // 与 D.DialogWindow 的默认行为一致：不真正关闭窗口，只隐藏。
        close.accepted = false
        dialog.hide()
        dialog.rejected()
    }

    ColumnLayout {
        spacing: 10
        width: dialog.width - dialog.leftPadding - dialog.rightPadding

        D.Label {
            Layout.fillWidth: true
            Layout.preferredHeight: text.length > 0 ? implicitHeight : 0
            visible: text.length > 0
            text: dialog.title
            elide: Text.ElideRight
            font: D.DTK.fontManager.t5
            horizontalAlignment: Text.AlignHCenter
        }

        D.Label {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? implicitHeight : 0
            visible: dialog.helpText.length > 0
            text: dialog.helpText
            wrapMode: Text.WordWrap
            font: D.DTK.fontManager.t8
            opacity: 0.6
        }

        D.TextArea {
            id: editor
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 320
            readOnly: !dialog.editable
            wrapMode: TextEdit.WrapAnywhere
            selectByMouse: true
            font.pixelSize: D.DTK.fontManager.t8.pixelSize
            font.family: "monospace"
        }

        RowLayout {
            Layout.fillWidth: true
            visible: dialog.editable && dialog.example.length > 0
            D.Button {
                text: qsTr("填入示例")
                flat: true
                onClicked: editor.text = dialog.example
            }
            Item {
                Layout.fillWidth: true
            }
        }

        D.DialogButtonBox {
            Layout.alignment: Qt.AlignRight
            standardButtons: dialog.editable ? D.DialogButtonBox.Save | D.DialogButtonBox.Cancel
                                             : D.DialogButtonBox.Close
            onAccepted: dialog.accepted(editor.text)
            onRejected: dialog.rejected()
        }
    }
}
