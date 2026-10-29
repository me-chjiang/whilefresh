import QtQuick
import WhileFresh

Item {
    id: header
    property string heading: ""
    property string subtitle: ""
    property string actionText: ""
    property bool backVisible: false
    signal backRequested()
    signal actionTriggered()
    height: Theme.topHeight
    Item {
        id: backButton
        visible: header.backVisible
        width: visible ? Theme.touch : 0
        height: Theme.touch
        anchors.verticalCenter: parent.verticalCenter
        IconGlyph {
            anchors.centerIn: parent
            name: "chevron-right"
            rotation: 180
            tint: Theme.ink
            width: 22
            height: 22
        }
        TapHandler { onTapped: header.backRequested() }
    }
    Text {
        id: headingText
        x: backButton.width + Theme.gapSmall
        anchors.verticalCenter: parent.verticalCenter
        text: header.heading
        font.family: Theme.serif
        font.pixelSize: Theme.textTitle
        font.weight: Theme.weightBlack
        font.letterSpacing: 2
        color: Theme.ink
        z: 1
        elide: Text.ElideRight
        width: Math.min(implicitWidth, Math.max(0, header.width - x - (header.actionText ? 80 : 0)))
    }
    Rectangle {
        x: headingText.x - Theme.radiusTag
        y: headingText.y + headingText.height - 12
        width: headingText.width + Theme.gapSmall
        height: 11
        radius: Theme.radiusTag
        color: Theme.tapeSand
    }
    Text {
        anchors.left: headingText.right
        anchors.leftMargin: Theme.gap
        anchors.baseline: headingText.baseline
        text: header.subtitle
        color: Theme.muted
        font.pixelSize: Theme.textSubtitle
        elide: Text.ElideRight
        width: Math.max(0, header.width - x - (header.actionText ? 100 : Theme.gapSmall))
    }
    Rectangle {
        visible: header.actionText !== ""
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: actionLabel.implicitWidth + Theme.gapLarge
        height: Theme.touch
        radius: Theme.radiusRow
        color: Theme.selectedPaper
        border.color: Theme.outline
        border.width: 1
        Text {
            id: actionLabel
            anchors.centerIn: parent
            text: header.actionText
            font.pixelSize: Theme.textControl
            font.weight: Theme.weightBold
            color: Theme.muted
        }
        TapHandler { onTapped: header.actionTriggered() }
    }
}
