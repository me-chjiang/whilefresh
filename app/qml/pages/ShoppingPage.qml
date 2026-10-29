import QtQuick
import QtQuick.Layouts
import WhileFresh

Item {
    id: page
    property bool canGoBack: false
    signal backRequested()
    property QtObject platform
    signal notice(string text)
    readonly property var categories: [
        { code: "vegetable", label: "蔬菜" }, { code: "fruit", label: "水果" },
        { code: "meat", label: "肉类" }, { code: "seafood", label: "水产" },
        { code: "egg_dairy", label: "蛋奶" }, { code: "soy", label: "豆制品" },
        { code: "staple", label: "主食" }, { code: "seasoning", label: "调料" },
        { code: "processed", label: "加工食品" }, { code: "other", label: "其他" }
    ]
    PageHeader { id: header; x: Theme.gap; width: parent.width - Theme.gap * 2; backVisible: page.canGoBack; onBackRequested: page.backRequested(); heading: "待买"; subtitle: "做菜缺的料" }
    Flickable {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        contentHeight: content.height + Theme.gapLarge
        Column {
            id: content
            x: Theme.gap
            width: parent.width - Theme.gap * 2
            spacing: Theme.gap
            EmptyState {
                visible: App.shopping.length === 0
                width: content.width
                height: visible ? implicitHeight + Theme.gapLarge * 3 : 0
                iconName: "list-check"
                heading: "暂时没有待买的食材"
                detail: "选择菜谱后，可把缺料加入这里"
            }
            Repeater {
                model: App.shopping.length ? page.categories : []
                delegate: Item {
                    id: group
                    required property var modelData
                    readonly property var entries: App.shopping.filter(function(item) { return item.category === group.modelData.code })
                    visible: entries.length > 0
                    width: content.width
                    height: visible ? groupCard.height + Theme.gapSmall : 0
                    Rectangle {
                        id: groupCard
                        y: Theme.gap
                        width: parent.width
                        height: groupItems.height + Theme.gapLarge * 2
                        radius: Theme.radiusCard
                        color: Theme.note
                        Column {
                            id: groupItems
                            x: Theme.gap
                            y: Theme.gapLarge
                            width: parent.width - Theme.gap * 2
                            Repeater {
                                model: group.entries
                                delegate: Item {
                                    required property var modelData
                                    width: groupItems.width
                                    height: Theme.touch
                                    CategoryDot { category: modelData.category; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter }
                                    Text { x: Theme.itemDot + Theme.gap; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: Theme.ink; font.pixelSize: Theme.textBody; font.weight: Theme.weightBold; width: parent.width - Theme.touch - x; elide: Text.ElideRight }
                                    Item {
                                        anchors.right: parent.right
                                        width: Theme.touch
                                        height: parent.height
                                        IconGlyph { name: "x"; tint: Theme.muted; width: 20; height: 20; anchors.centerIn: parent }
                                        TapHandler { onTapped: App.removeShopping(modelData.id) }
                                    }
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.rule }
                                }
                            }
                        }
                    }
                    SectionTape { x: Theme.gap; label: group.modelData.label; count: String(group.entries.length); tapeColor: Theme.tapeSand }
                }
            }
            Row {
                visible: App.shopping.length > 0
                spacing: Theme.gapSmall
                InkButton { text: "复制清单"; iconName: "notes"; onClicked: { if (page.platform) page.platform.copyText(App.shoppingText()); page.notice("已复制待买清单") } }
                InkButton { text: "分享清单"; iconName: "list-check"; outline: true; onClicked: { if (page.platform) page.platform.shareText(App.shoppingText()) } }
            }
        }
    }
}
