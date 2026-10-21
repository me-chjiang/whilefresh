import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import WhileFresh

Item {
    id: page
    signal importRequested()
    signal editRequested(var item)
    signal notice(string message)
    property bool multiSelect: false
    property var checkedIds: []

    PageHeader {
        id: header
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        heading: "冰箱"
        subtitle: "共 " + MockData.items.length + " 件"
        actionText: "＋ 入库"
        onActionTriggered: page.importRequested()
    }
    Flickable {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        contentHeight: content.height + Theme.gapLarge * 2
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: content
            width: parent.width - Theme.gap * 2
            x: Theme.gap
            spacing: Theme.gapLarge
            Item { width: 1; height: Theme.gapSmall }
            EmptyState {
                visible: MockData.items.length === 0
                width: content.width
                height: visible ? implicitHeight + Theme.gapLarge * 2 : 0
                iconName: "fridge"
                heading: "冰箱里还没有食材"
                detail: "从截图入库，或手动记下一件食材"
                actionText: "＋ 入库 / 手动添加"
                onActionTriggered: page.importRequested()
            }
            Repeater {
                model: ["冷藏", "冷冻", "常温"]
                delegate: Item {
                    id: section
                    required property string modelData
                    readonly property var list: MockData.items.filter(function (item) { return item.location === section.modelData })
                    visible: list.length > 0
                    width: content.width
                    height: visible ? card.height + Theme.gapSmall : 0
                    Rectangle {
                        id: card
                        y: Theme.gap
                        width: parent.width
                        height: rows.height + Theme.gapLarge + Theme.gapSmall
                        radius: Theme.radiusCard
                        color: Theme.card
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowBlur: Theme.shadowOneBlur
                            shadowVerticalOffset: Theme.shadowOneOffset
                            shadowOpacity: Theme.shadowOneOpacity
                            shadowColor: Theme.shadowColor
                        }
                        Column {
                            id: rows
                            x: Theme.sectionPadding
                            y: Theme.gapLarge
                            width: card.width - Theme.sectionPadding * 2
                            Repeater {
                                model: section.list
                                delegate: PantryRow {
                                    required property var modelData
                                    width: rows.width
                                    ingredient: modelData
                                    multiSelect: page.multiSelect
                                    checked: page.checkedIds.indexOf(modelData.id) >= 0
                                    onEditRequested: function(item) { page.editRequested(item) }
                                    onSelectionRequested: function(item) {
                                        page.multiSelect = true
                                        page.checkedIds = page.checkedIds.indexOf(item.id) < 0 ? page.checkedIds.concat([item.id]) : page.checkedIds.filter(function(id) { return id !== item.id })
                                    }
                                    onUsed: function(item) {
                                        if (item.name === "鸡蛋") page.notice("鸡蛋标为默认存量多；需要手动标记家里没有")
                                        else MockData.removeItem(item.id)
                                    }
                                    onUnavailable: function(item) { MockData.removeItem(item.id) }
                                    onFirstRequested: function(item) { MockData.setItem(item.id, { first: !item.first }) }
                                }
                            }
                        }
                    }
                    SectionTape {
                        x: Theme.gapLarge
                        label: section.modelData
                        count: String(section.list.length)
                        iconName: section.modelData === "冷藏" ? "fridge" : section.modelData === "冷冻" ? "snowflake" : "sun"
                        tapeColor: section.modelData === "冷藏" ? Theme.tapeSage : section.modelData === "冷冻" ? Theme.tapeBlue : Theme.tapeSand
                    }
                }
            }
            InkButton {
                visible: page.multiSelect
                width: content.width
                text: "移除选中 " + page.checkedIds.length + " 件"
                accent: Theme.tomato
                onClicked: {
                    page.checkedIds.forEach(function(id) { MockData.removeItem(id) })
                    page.checkedIds = []
                    page.multiSelect = false
                }
            }
            InkButton {
                visible: page.multiSelect
                width: content.width
                text: "完成多选"
                outline: true
                onClicked: { page.multiSelect = false; page.checkedIds = [] }
            }
        }
    }
}
