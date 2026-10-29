import QtQuick
import QtQuick.Controls.Basic
import WhileFresh

Button {
    id: button
    property string iconName: ""
    property color accent: Theme.green
    property bool outline: false
    property bool titleStyle: false
    implicitHeight: Math.max(Theme.touch, content.implicitHeight + topPadding + bottomPadding)
    implicitWidth: buttonContent.implicitWidth + Theme.gapLarge * 2
    padding: Theme.gapSmall
    background: Rectangle {
        radius: Theme.radiusRow
        color: button.outline ? Theme.card : button.accent
        border.color: button.outline ? Theme.outline : button.accent
        border.width: button.outline ? 1.5 : 0
        opacity: button.pressed ? 0.8 : 1
    }
    contentItem: Item {
        id: buttonContent
        implicitWidth: label.implicitWidth + (button.iconName !== "" ? Theme.touch / 2 + Theme.gapSmall : 0)
        implicitHeight: content.implicitHeight
        Row {
            id: content
            anchors.centerIn: parent
            spacing: Theme.gapSmall
            IconGlyph {
                y: (content.height - height) / 2
                visible: button.iconName !== ""
                name: button.iconName
                tint: button.outline ? Theme.ink : Theme.textOnGreen
                width: visible ? Theme.touch / 2 : 0
                height: width
            }
            Text {
                id: label
                y: (content.height - height) / 2
                width: Math.min(implicitWidth, Math.max(0, button.availableWidth - (button.iconName !== "" ? Theme.touch / 2 + content.spacing : 0)))
                text: button.text
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.family: button.titleStyle ? Theme.serif : ""
                font.pixelSize: button.titleStyle ? Theme.textRecipe : Theme.textControl
                font.weight: button.titleStyle ? Theme.weightBlack : Theme.weightBold
                color: button.outline ? Theme.ink : Theme.textOnGreen
            }
        }
    }
}
