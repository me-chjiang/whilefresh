import QtQuick
import QtQuick.Effects
import WhileFresh

Item {
    id: icon
    property string name: ""
    property color tint: Theme.ink
    width: Theme.touch / 2
    height: width
    Image {
        anchors.fill: parent
        source: icon.name === "" ? "" : Qt.resolvedUrl("../../assets/icons/" + icon.name + ".svg")
        fillMode: Image.PreserveAspectFit
        sourceSize.width: Math.round(width * Screen.devicePixelRatio)
        sourceSize.height: Math.round(height * Screen.devicePixelRatio)
        layer.enabled: true
        layer.effect: MultiEffect {
            colorization: 1
            colorizationColor: icon.tint
        }
    }
}
