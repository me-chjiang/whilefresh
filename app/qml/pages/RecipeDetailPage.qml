import QtQuick
import QtQuick.Layouts
import WhileFresh

Item {
    id: page
    required property var recipe
    signal backRequested()
    signal shoppingRequested()
    PageHeader {
        id: header
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        heading: "菜谱卡"
        actionText: "返回"
        onActionTriggered: page.backRequested()
    }
    Flickable {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        contentHeight: content.height + Theme.gapLarge
        clip: true
        Column {
            id: content
            x: Theme.gap
            width: parent.width - Theme.gap * 2
            spacing: Theme.gap
            Rectangle {
                width: parent.width
                height: intro.height + Theme.gapLarge * 2
                radius: Theme.radiusCard
                color: Theme.card
                Rectangle { width: parent.width; height: 3; color: Theme.tomato; radius: Theme.radiusTag }
                Column {
                    id: intro
                    x: Theme.gapLarge
                    y: Theme.gapLarge
                    width: parent.width - Theme.gapLarge * 2
                    spacing: Theme.gapSmall
                    Text { text: page.recipe.name; font.family: Theme.serif; font.pixelSize: Theme.textSheetTitle; font.weight: Theme.weightBlack; color: Theme.ink }
                    Text { text: "HowToCook  ·  有 " + page.recipe.haveCount + " · 缺 " + page.recipe.missingCount + "  ·  难度 " + page.recipe.difficulty + "/5"; font.pixelSize: Theme.textSubtitle; color: Theme.muted }
                    InkButton {
                        text: "查看 HowToCook 原菜谱"
                        iconName: "book-2"
                        outline: true
                        width: intro.width
                        onClicked: Qt.openUrlExternally(page.recipe.sourceUrl)
                    }
                }
            }
            Text { text: "原用料"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Rectangle {
                width: parent.width
                height: ingredientList.height + Theme.gapSmall * 2
                radius: Theme.radiusCard
                color: Theme.card
                Column {
                    id: ingredientList
                    x: Theme.gap
                    y: Theme.gapSmall
                    width: parent.width - Theme.gap * 2
                    Repeater {
                        model: page.recipe.ingredients
                        delegate: RowLayout {
                            required property var modelData
                            width: ingredientList.width
                            height: Theme.touch
                            spacing: Theme.gapSmall
                            Rectangle {
                                width: 38
                                height: Theme.badgeHeight
                                radius: Theme.radiusTag
                                color: modelData.has ? Theme.tapeSage : Theme.note
                                Text { anchors.centerIn: parent; text: modelData.has ? "有" : "缺"; font.pixelSize: Theme.textSmall; font.weight: Theme.weightStrong; color: modelData.has ? Theme.fresh : Theme.nearing }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.raw + (modelData.optional ? " · 可选" : "")
                                font.pixelSize: Theme.textControl
                                color: Theme.ink
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
            InkButton {
                visible: page.recipe.missingCount > 0
                width: content.width
                text: "缺料加入待买"
                accent: Theme.green
                onClicked: { App.addMissing(page.recipe.id); page.shoppingRequested() }
            }
            Text { text: "原步骤"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Repeater {
                model: page.recipe.steps
                delegate: Rectangle {
                    required property string modelData
                    width: content.width
                    height: stepText.implicitHeight + Theme.gapLarge * 2
                    radius: Theme.radiusRow
                    color: Theme.card
                    Text {
                        id: stepText
                        x: Theme.gap
                        y: Theme.gap
                        width: parent.width - Theme.gap * 2
                        text: modelData
                        wrapMode: Text.Wrap
                        color: Theme.ink
                        font.pixelSize: Theme.textControl
                        lineHeight: 1.35
                    }
                }
            }
        }
    }
}
