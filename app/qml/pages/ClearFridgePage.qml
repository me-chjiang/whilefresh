import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import WhileFresh

Item {
    id: page
    signal searchRequested(string keyword)
    PageHeader {
        id: header
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        heading: "清冰箱"
        subtitle: "用掉临期和先用的食材"
    }
    Flickable {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        contentHeight: content.height + Theme.gapLarge
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: content
            x: Theme.gap
            width: parent.width - Theme.gap * 2
            spacing: Theme.gapLarge
            Item { width: 1; height: Theme.gapSmall }
            Rectangle {
                id: note
                width: content.width
                height: panel.height + Theme.gapLarge * 2
                radius: Theme.radiusCard
                color: Theme.note
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: Theme.shadowOneBlur
                    shadowVerticalOffset: Theme.shadowOneOffset
                    shadowOpacity: Theme.shadowOneOpacity
                    shadowColor: Theme.shadowColor
                }
                SectionTape {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: -Theme.gap
                    label: "选料"
                    tapeColor: Theme.tapeBlue
                    angle: 1.5
                }
                Column {
                    id: panel
                    x: Theme.gap
                    y: Theme.gapLarge
                    width: note.width - Theme.gap * 2
                    spacing: Theme.gap
                    RowLayout {
                        width: parent.width
                        Text { text: "要用掉的食材"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink; Layout.fillWidth: true }
                        Text { text: "已选 " + App.selectedIds.length + " / " + App.items.length; font.pixelSize: Theme.textSubtitle; color: Theme.noteMuted }
                    }
                    Flow {
                        id: chips
                        width: parent.width
                        height: childrenRect.height
                        spacing: Theme.gapSmall
                        Repeater {
                            model: App.items
                            delegate: Item {
                                id: chipTarget
                                required property var modelData
                                readonly property bool selected: App.selectedIds.indexOf(modelData.id) >= 0
                                width: chipText.implicitWidth + 64 + (modelData.first || modelData.freshness === "nearing" ? 36 : 0)
                                height: Theme.touch
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width
                                    height: 40
                                    radius: Theme.radiusRow
                                    color: chipTarget.selected ? Theme.selectedPaper : "transparent"
                                    border.color: chipTarget.selected ? Theme.ink : Theme.chipOutline
                                    border.width: 1.5
                                    Row {
                                        anchors.centerIn: parent
                                        spacing: Theme.gapSmall
                                        Rectangle {
                                            width: 19
                                            height: 19
                                            radius: Theme.radiusTag
                                            color: chipTarget.selected ? Theme.tomato : "transparent"
                                            border.color: chipTarget.selected ? Theme.tomato : Theme.chipOutline
                                            border.width: 1.5
                                            Text { visible: chipTarget.selected; anchors.centerIn: parent; text: "✓"; color: Theme.textOnTomato; font.weight: Theme.weightBold }
                                        }
                                        CategoryDot { width: 16; height: 16; category: chipTarget.modelData.category }
                                        Text { id: chipText; text: chipTarget.modelData.name; font.pixelSize: Theme.textQuantity; font.weight: Theme.weightBold; color: chipTarget.selected ? Theme.ink : Theme.chipMuted }
                                        Text {
                                            visible: chipTarget.modelData.first || chipTarget.modelData.freshness === "nearing"
                                            text: chipTarget.modelData.first ? "先用" : "临期"
                                            font.pixelSize: Theme.textSmall
                                            font.weight: Theme.weightStrong
                                            color: chipTarget.modelData.first ? Theme.ink : Theme.nearing
                                        }
                                    }
                                }
                                TapHandler { onTapped: App.toggleSelected(chipTarget.modelData.id) }
                            }
                        }
                    }
                }
            }
            InkButton {
                width: content.width
                text: "搜下厨房"
                iconName: "notes"
                enabled: App.selectedIds.length > 0
                onClicked: page.searchRequested(App.items.filter(function(i) { return App.selectedIds.indexOf(i.id) >= 0 }).map(function(i) { return i.name }).join(" "))
            }
            EmptyState {
                visible: App.selectedIds.length === 0
                width: content.width
                height: visible ? implicitHeight + Theme.gapLarge : 0
                iconName: "chef-hat"
                heading: "选几样食材试试"
                detail: ""
            }
        }
    }
}
