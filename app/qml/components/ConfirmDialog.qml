import QtQuick
import QtQuick.Controls.Basic
import WhileFresh

Dialog {
    id: confirm
    property string message: ""
    property string confirmText: "确认"
    property bool destructive: false
    signal acceptedAction()
    modal: true
    parent: Overlay.overlay
    width: Math.min(parent ? parent.width - Theme.gapLarge * 2 : 350, 400)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    padding: Theme.gapLarge
    background: Rectangle { radius: Theme.radiusCard; color: Theme.card }
    Overlay.modal: Rectangle { color: Theme.mask }
    contentItem: Column {
        spacing: Theme.gapLarge
        Text {
            width: parent.width
            text: confirm.message
            color: Theme.ink
            font.pixelSize: Theme.textGroup
            wrapMode: Text.Wrap
        }
        Row {
            spacing: Theme.gapSmall
            InkButton { text: "取消"; outline: true; onClicked: confirm.reject() }
            InkButton {
                text: confirm.confirmText
                accent: confirm.destructive ? Theme.tomato : Theme.green
                onClicked: { confirm.acceptedAction(); confirm.accept() }
            }
        }
    }
}
