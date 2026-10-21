import QtQuick
import QtQuick.Controls.Basic
import WhileFresh

Item {
    id: page
    signal recipeRequested(var recipe)
    property string query: ""
    PageHeader {
        id: header
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        heading: "菜谱"
        subtitle: "HowToCook · 按匹配度浏览"
    }
    Column {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        spacing: Theme.gap
        Rectangle {
            width: parent.width
            height: Theme.touch
            radius: Theme.radiusRow
            color: Theme.card
            border.color: Theme.outline
            border.width: 1
            Row {
                anchors.fill: parent
                anchors.leftMargin: Theme.gap
                spacing: Theme.gapSmall
                IconGlyph { name: "notes"; tint: Theme.muted; width: 22; height: 22; anchors.verticalCenter: parent.verticalCenter }
                TextField {
                    width: parent.width - 38
                    height: Theme.touch
                    placeholderText: "搜索菜名"
                    font.pixelSize: Theme.textQuantity
                    color: Theme.ink
                    background: Item {}
                    onTextChanged: page.query = text.trim()
                }
            }
        }
        Flickable {
            width: parent.width
            height: parent.height - y
            contentHeight: list.height + Theme.gapLarge
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: list
                width: parent.width
                spacing: Theme.gapSmall
                EmptyState {
                    visible: App.recipes.filter(function(r) { return r.name.indexOf(page.query) >= 0 }).length === 0
                    width: parent.width
                    height: visible ? implicitHeight + Theme.gapLarge * 2 : 0
                    iconName: "book-2"
                    heading: "没有找到菜谱"
                    detail: "试试别的菜名，或改选食材"
                }
                Repeater {
                    model: App.recipes.filter(function(r) { return r.name.indexOf(page.query) >= 0 })
                    delegate: RecipeCard {
                        required property var modelData
                        width: list.width
                        recipe: modelData
                        onOpened: function(recipe) { page.recipeRequested(recipe) }
                    }
                }
            }
        }
    }
}
