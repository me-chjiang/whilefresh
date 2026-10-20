import QtQuick
import QtQuick.Controls.Basic
import WhileFresh

Popup {
    id: sheet
    property string heading: ""
    property string tapeText: ""
    property Component bodyComponent
    readonly property var bodyItem: contents.item
    property int sheetHeight: 600
    modal: true
    focus: true
    parent: Overlay.overlay
    padding: 0
    width: parent ? parent.width : 390
    height: Math.min(parent ? parent.height - Theme.gapLarge : sheetHeight, sheetHeight)
    x: 0
    y: parent ? parent.height - height : 0
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: Theme.card; radius: Theme.radiusCard }
    Overlay.modal: Rectangle { color: Theme.mask }
    contentItem: Item {
        SectionTape {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 0
            label: sheet.tapeText
            tapeColor: Theme.tapeSage
            z: 2
        }
        Text {
            id: title
            x: Theme.gapLarge
            y: Theme.gapLarge + Theme.gapSmall
            text: sheet.heading
            font.family: Theme.serif
            font.pixelSize: Theme.textSheetTitle
            font.weight: Theme.weightBlack
            color: Theme.ink
        }
        Loader {
            id: contents
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: title.bottom
            anchors.topMargin: Theme.gapSmall
            anchors.bottom: parent.bottom
            sourceComponent: sheet.bodyComponent
            onStatusChanged: if (status === Loader.Error) console.error("Import sheet content failed to load")
        }
        Column {
            anchors.centerIn: parent
            width: parent.width - Theme.gapLarge * 2
            spacing: Theme.gap
            visible: contents.status === Loader.Error
            Text {
                width: parent.width
                text: "页面暂时无法加载，请关闭后重试。"
                wrapMode: Text.Wrap
                color: Theme.ink
                font.pixelSize: Theme.textBody
            }
            InkButton {
                width: parent.width
                text: "重新加载"
                onClicked: {
                    contents.active = false
                    Qt.callLater(function() { contents.active = true })
                }
            }
        }
    }
}
