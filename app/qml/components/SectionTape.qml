import QtQuick
import WhileFresh

Rectangle {
    id: tape
    property string label: ""
    property string iconName: ""
    property string count: ""
    property color tapeColor: Theme.tapeSage
    property real angle: -1.5
    implicitWidth: caption.implicitWidth + (iconName ? Theme.gapLarge : 0) + (count ? Theme.gapLarge : 0) + Theme.gapLarge * 2
    implicitHeight: Theme.tapeHeight
    radius: Theme.radiusTag
    color: tapeColor
    rotation: angle
    Image {
        anchors.fill: parent
        anchors.margins: Theme.radiusTag
        source: Qt.resolvedUrl("../../assets/textures/tape-stripe.png")
        fillMode: Image.Tile
    }
    Row {
        anchors.centerIn: parent
        spacing: Theme.gapSmall
        IconGlyph {
            visible: tape.iconName !== ""
            name: tape.iconName
            width: visible ? 18 : 0
            height: width
        }
        Text {
            id: caption
            text: tape.label
            color: Theme.ink
            font.family: Theme.serif
            font.pixelSize: Theme.textTape
            font.weight: Theme.weightBlack
            font.letterSpacing: 1
        }
        Text {
            visible: tape.count !== ""
            text: tape.count
            color: Theme.muted
            font.pixelSize: Theme.textSubtitle
            font.weight: Theme.weightBold
        }
    }
}
