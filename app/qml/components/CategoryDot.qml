import QtQuick
import WhileFresh

Item {
    id: dot
    property string category: "other"
    width: Theme.itemDot
    height: width
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Theme.categoryColor(dot.category)
        border.color: "white"
        border.width: 2.5
    }
}
