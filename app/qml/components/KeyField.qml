import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import WhileFresh

Rectangle {
    id: root
    property string label: ""
    property string field: ""
    property bool configured: false
    property string error: ""
    width: parent.width
    height: column.height + Theme.gap * 2
    radius: Theme.radiusCard
    color: Theme.card
    Column {
        id: column
        x: Theme.gap
        y: Theme.gap
        width: parent.width - Theme.gap * 2
        spacing: Theme.gapSmall
        RowLayout {
            width: parent.width
            Text { text: root.label; font.pixelSize: Theme.textBody; font.weight: Theme.weightBold; color: Theme.ink; Layout.fillWidth: true }
            Text { text: root.configured ? "已配置" : "未配置"; font.pixelSize: Theme.textMeta; color: root.configured ? Theme.green : Theme.muted }
        }
        TextField {
            id: input
            width: parent.width
            height: Theme.touch
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            font.pixelSize: Theme.textControl
            background: Rectangle { color: Theme.selectedPaper; radius: Theme.radiusTag; border.color: Theme.rule }
        }
        Text { visible: root.error !== ""; text: root.error; font.pixelSize: Theme.textMeta; color: Theme.tomato }
        Row {
            spacing: Theme.gapSmall
            InkButton {
                text: "保存"
                enabled: input.text.trim() !== ""
                onClicked: {
                    if (App.settings.saveSecret(root.field, input.text)) {
                        input.text = ""
                        root.error = ""
                    } else {
                        root.error = "保存失败"
                    }
                }
            }
            InkButton {
                text: "清除"
                outline: true
                enabled: root.configured
                onClicked: root.error = App.settings.clearSecret(root.field) ? "" : "清除失败"
            }
        }
    }
}
