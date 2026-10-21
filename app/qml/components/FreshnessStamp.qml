import QtQuick
import WhileFresh

Rectangle {
    id: stamp
    property string status: "fresh"
    implicitWidth: label.implicitWidth + Theme.gap
    implicitHeight: Theme.stampHeight
    radius: Theme.radiusTag
    color: Theme.selectedPaper
    border.width: status === "overdue" ? 1.2 : 1.5
    border.color: Theme.freshnessColor(status)
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2.5
        radius: Theme.radiusTag
        color: "transparent"
        border.color: Theme.overdue
        border.width: 1.2
        visible: stamp.status === "overdue"
    }
    Text {
        id: label
        anchors.centerIn: parent
        text: Theme.freshnessLabel(stamp.status)
        font.pixelSize: Theme.textMeta
        font.weight: Theme.weightStrong
        color: Theme.freshnessColor(stamp.status)
    }
}
