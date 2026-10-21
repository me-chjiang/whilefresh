import QtQuick
import WhileFresh

Item {
    id: paper
    Rectangle {
        anchors.fill: parent
        color: Theme.paper
    }
    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("../../assets/textures/paper-tile.png")
        fillMode: Image.Tile
    }
}
