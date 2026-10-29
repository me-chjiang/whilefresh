import QtQuick
import QtQuick.Layouts
import WhileFresh

Rectangle {
    id: card
    property var recipe: ({})
    readonly property bool showCoverage: recipe.usedSelected !== undefined || recipe.unusedSelected !== undefined
    signal opened(var recipe)
    height: Math.max(Theme.recipeHeight, cardContent.implicitHeight + Theme.gap * 2)
    radius: Theme.radiusRow
    color: Theme.card
    Rectangle {
        width: parent.width
        height: 3
        radius: Theme.radiusTag
        color: Theme.tomato
    }
    RowLayout {
        id: cardContent
        anchors.fill: parent
        anchors.topMargin: Theme.gap
        anchors.bottomMargin: Theme.gap
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
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapSmall
                IconGlyph { name: "book-2"; tint: Theme.muted; Layout.preferredWidth: 15; Layout.preferredHeight: 15 }
                Text { Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; text: card.recipe.sourceName || "原菜谱"; font.pixelSize: Theme.textMeta; font.weight: Theme.weightBold; color: Theme.muted }
                Text { text: "有 " + (card.recipe.haveCount || 0); font.pixelSize: Theme.textMeta; font.weight: Theme.weightBold; color: Theme.fresh }
                Text { text: "· 缺 " + (card.recipe.missingCount || 0); font.pixelSize: Theme.textMeta; font.weight: Theme.weightBold; color: Theme.nearing }
            }
            Text {
                visible: card.showCoverage && card.recipe.usedSelected && card.recipe.usedSelected.length > 0
                Layout.fillWidth: true
                text: "用到已选：" + (card.recipe.usedSelected || []).join("、")
                font.pixelSize: Theme.textMeta
                color: Theme.fresh
                wrapMode: Text.Wrap
            }
            Text {
                visible: card.showCoverage && card.recipe.unusedSelected && card.recipe.unusedSelected.length > 0
                Layout.fillWidth: true
                text: "未用到：" + (card.recipe.unusedSelected || []).join("、")
                font.pixelSize: Theme.textMeta
                color: Theme.muted
                wrapMode: Text.Wrap
            }
        }
        Text {
            text: "难度 " + (card.recipe.difficulty || 1) + "/5"
            font.pixelSize: Theme.textMeta
            color: Theme.muted
        }
        IconGlyph { name: "chevron-right"; tint: Theme.outline; Layout.preferredWidth: 20; Layout.preferredHeight: 20 }
    }
    TapHandler { onTapped: card.opened(card.recipe) }
}
