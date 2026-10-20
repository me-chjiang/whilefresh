import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Shapes
import WhileFresh

Item {
    id: nav
    property int currentTab: 0
    property int nearCount: 0
    signal tabRequested(int index)
    height: Theme.navHeight + 36
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: Theme.navHeight
        color: Theme.navPaper
        Rectangle { width: parent.width; height: 1.5; color: Theme.navRule }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.gapSmall
            anchors.rightMargin: Theme.gapSmall
            anchors.bottomMargin: Theme.gapSmall
            spacing: 0
            Repeater {
                model: ["冰箱", "菜谱", "清冰箱", "待买", "我的"]
                delegate: Item {
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Column {
                        visible: index !== 2
                        anchors.centerIn: parent
                        spacing: 1
                        IconGlyph {
                            name: ["fridge", "book-2", "chef-hat", "list-check", "user"][index]
                            tint: nav.currentTab === index ? Theme.tomato : Theme.mutedTab
                            width: 25
                            height: 25
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: modelData
                            font.pixelSize: Theme.textMeta
                            font.weight: Theme.weightBold
                            color: nav.currentTab === index ? Theme.tomato : Theme.mutedTab
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                    TapHandler { enabled: index !== 2; onTapped: nav.tabRequested(index) }
                }
            }
        }
    }
    Item {
        width: Theme.stampDiameter + Theme.gap
        height: width
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.color: Theme.tomato
            border.width: nav.currentTab === 2 ? 5 : 0
            opacity: 0.22
        }
        Rectangle {
            id: stamp
            anchors.centerIn: parent
            width: Theme.stampDiameter
            height: width
            radius: width / 2
            rotation: -8
            color: Theme.tomato
            border.color: "white"
            border.width: 5
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: Theme.shadowTwoBlur
                shadowVerticalOffset: Theme.shadowTwoOffset
                shadowOpacity: Theme.shadowTwoOpacity
                shadowColor: Theme.shadowColor
            }
            Shape {
                anchors.fill: parent
                anchors.margins: Theme.gapSmall
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: Theme.textOnTomato
                    strokeWidth: 1.5
                    strokeStyle: ShapePath.DashLine
                    dashPattern: [3, 2]
                    PathAngleArc { centerX: (Theme.stampDiameter - Theme.gapSmall * 2) / 2; centerY: (Theme.stampDiameter - Theme.gapSmall * 2) / 2; radiusX: (Theme.stampDiameter - Theme.gapSmall * 2) / 2; radiusY: (Theme.stampDiameter - Theme.gapSmall * 2) / 2; startAngle: 0; sweepAngle: 360 }
                }
            }
            Column {
                anchors.centerIn: parent
                spacing: 0
                IconGlyph { name: "chef-hat"; tint: Theme.textOnTomato; width: 29; height: 29; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: "清冰箱"; font.family: Theme.serif; font.pixelSize: Theme.textTape; font.weight: Theme.weightBlack; color: Theme.textOnTomato; anchors.horizontalCenter: parent.horizontalCenter }
            }
        }
        Rectangle {
            visible: nav.nearCount > 0
            anchors.right: parent.right
            anchors.top: parent.top
            width: 28
            height: 28
            radius: Theme.radiusTag
            rotation: -8
            color: Theme.mustard
            border.color: "white"
            border.width: 2
            Text { anchors.centerIn: parent; text: String(nav.nearCount); color: Theme.ink; font.pixelSize: Theme.textSubtitle; font.weight: Theme.weightStrong }
        }
        TapHandler { onTapped: nav.tabRequested(2) }
    }
}
