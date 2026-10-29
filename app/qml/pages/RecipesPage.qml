import QtQuick
import QtQuick.Controls.Basic
import WhileFresh

Item {
    id: page
    signal recipeRequested(var recipe)
    signal backRequested()
    property bool canGoBack: false
    property string query: ""
    readonly property var filteredRecipes: App.recipes.filter(function(recipe) { return recipe.name.indexOf(page.query) >= 0 })
    PageHeader {
        id: header
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        heading: "菜谱"
        subtitle: "按匹配度浏览"
        backVisible: page.canGoBack
        onBackRequested: page.backRequested()
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
        Item {
            width: parent.width
            height: parent.height - y
            EmptyState {
                visible: page.filteredRecipes.length === 0
                width: parent.width
                anchors.top: parent.top
                iconName: "book-2"
                heading: "没有找到菜谱"
                detail: "试试别的菜名，或改选食材"
            }
            ListView {
                id: list
                anchors.fill: parent
                visible: page.filteredRecipes.length > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                spacing: Theme.gapSmall
                reuseItems: true
                model: page.filteredRecipes
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
