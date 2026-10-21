import QtQuick
import QtQuick.Controls.Basic
import WhileFresh

Button {
    id: button
    property string iconName: ""
    property color accent: Theme.green
    property bool outline: false
    property bool titleStyle: false
    implicitHeight: Theme.touch
    implicitWidth: content.implicitWidth + Theme.gapLarge * 2
    padding: Theme.gapSmall
    background: Rectangle {
        radius: Theme.radiusRow
        color: button.outline ? Theme.card : button.accent
        border.color: button.outline ? Theme.outline : button.accent
        border.width: button.outline ? 1.5 : 0
        opacity: button.pressed ? 0.8 : 1
    }
    contentItem: Row {
        id: content
        spacing: Theme.gapSmall
        anchors.centerIn: parent
        IconGlyph {
            visible: button.iconName !== ""
            name: button.iconName
            tint: button.outline ? Theme.ink : Theme.textOnGreen
            width: visible ? Theme.touch / 2 : 0
            height: width
        }
        Text {
            text: button.text
            font.family: button.titleStyle ? Theme.serif : ""
            font.pixelSize: button.titleStyle ? Theme.textRecipe : Theme.textControl
            font.weight: button.titleStyle ? Theme.weightBlack : Theme.weightBold
            color: button.outline ? Theme.ink : Theme.textOnGreen
            verticalAlignment: Text.AlignVCenter
        }
    }
}
