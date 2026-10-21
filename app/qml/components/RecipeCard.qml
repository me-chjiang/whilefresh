import QtQuick
import QtQuick.Layouts
import WhileFresh

Rectangle {
    id: card
    property var recipe: ({})
    signal opened(var recipe)
    height: Theme.recipeHeight
    radius: Theme.radiusRow
    color: Theme.card
    Rectangle {
        width: parent.width
        height: 3
        radius: Theme.radiusTag
        color: Theme.tomato
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.gap
        anchors.rightMargin: Theme.gapSmall
        spacing: Theme.gapSmall
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Text {
                Layout.fillWidth: true
                text: card.recipe.name || ""
                font.family: Theme.serif
                font.pixelSize: Theme.textRecipe
                font.weight: Theme.weightBlack
                color: Theme.ink
                elide: Text.ElideRight
            }
            RowLayout {
                spacing: Theme.gapSmall
                IconGlyph { name: "book-2"; tint: Theme.muted; width: 15; height: 15 }
                Text { text: "HowToCook"; font.pixelSize: Theme.textMeta; font.weight: Theme.weightBold; color: Theme.muted }
                Text { text: "有 " + (card.recipe.haveCount || 0); font.pixelSize: Theme.textMeta; font.weight: Theme.weightBold; color: Theme.fresh }
                Text { text: "· 缺 " + (card.recipe.missingCount || 0); font.pixelSize: Theme.textMeta; font.weight: Theme.weightBold; color: Theme.nearing }
            }
        }
        Text {
            text: "难度 " + (card.recipe.difficulty || 1) + "/5"
            font.pixelSize: Theme.textMeta
            color: Theme.muted
        }
        IconGlyph { name: "chevron-right"; tint: Theme.outline; width: 20; height: 20 }
    }
    TapHandler { onTapped: card.opened(card.recipe) }
}
