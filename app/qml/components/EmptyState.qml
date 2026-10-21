import QtQuick
import WhileFresh

Column {
    id: empty
    property string iconName: "fridge"
    property string heading: "这里还没有内容"
    property string detail: "可以继续添加"
    property string actionText: ""
    signal actionTriggered()
    spacing: Theme.gap
    IconGlyph { name: empty.iconName; tint: Theme.muted; width: 36; height: 36; anchors.horizontalCenter: parent.horizontalCenter }
    Text { text: empty.heading; font.family: Theme.serif; font.pixelSize: Theme.textPanel; color: Theme.ink; anchors.horizontalCenter: parent.horizontalCenter }
    Text { text: empty.detail; font.pixelSize: Theme.textSubtitle; color: Theme.muted; anchors.horizontalCenter: parent.horizontalCenter; wrapMode: Text.Wrap; width: Math.min(implicitWidth, empty.width - Theme.gapLarge) }
    InkButton {
        visible: empty.actionText !== ""
        text: empty.actionText
        anchors.horizontalCenter: parent.horizontalCenter
        onClicked: empty.actionTriggered()
    }
}
