import QtQuick
import QtQuick.Layouts
import WhileFresh

Item {
    id: page
    required property var recipe
    property var selectionContext: ({})
    signal backRequested()
    signal shoppingRequested()
    function stepBody(step) { return String(step).replace(/^\s*(?:步骤\s*)?\d+[.、．)]\s*/, "") }
    PageHeader {
        id: header
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        heading: "菜谱卡"
        backVisible: true
        onBackRequested: page.backRequested()
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
                    Text { width: parent.width; wrapMode: Text.Wrap; text: page.recipe.name; font.family: Theme.serif; font.pixelSize: Theme.textSheetTitle; font.weight: Theme.weightBlack; color: Theme.ink }
                    Text { width: parent.width; wrapMode: Text.Wrap; text: (page.recipe.sourceName || "原菜谱") + "  ·  有 " + page.recipe.haveCount + " · 缺 " + page.recipe.missingCount + "  ·  难度 " + page.recipe.difficulty + "/5"; font.pixelSize: Theme.textSubtitle; color: Theme.muted }
                    Text { visible: !!page.recipe.summary; width: parent.width; text: page.recipe.summary || ""; textFormat: Text.MarkdownText; wrapMode: Text.Wrap; font.pixelSize: Theme.textControl; color: Theme.ink; onLinkActivated: function(link) { Qt.openUrlExternally(link) } }
                    Text {
                        visible: !!page.selectionContext.usedSelected && page.selectionContext.usedSelected.length > 0
                        width: parent.width
                        text: "用到已选：" + (page.selectionContext.usedSelected || []).join("、")
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.textSubtitle
                        color: Theme.fresh
                    }
                    Text {
                        visible: !!page.selectionContext.unusedSelected && page.selectionContext.unusedSelected.length > 0
                        width: parent.width
                        text: "未用到：" + (page.selectionContext.unusedSelected || []).join("、")
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.textSubtitle
                        color: Theme.muted
                    }
                    InkButton {
                        visible: !!page.recipe.sourceUrl
                        text: "查看原菜谱"
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
                            height: Math.max(Theme.touch, ingredientText.implicitHeight + Theme.gapSmall * 2)
                            spacing: Theme.gapSmall
                            Rectangle {
                                Layout.preferredWidth: 38
                                Layout.preferredHeight: Theme.badgeHeight
                                radius: Theme.radiusTag
                                color: modelData.known === false ? Theme.selectedPaper : modelData.has ? Theme.tapeSage : Theme.note
                                Text { anchors.centerIn: parent; text: modelData.known === false ? "原文" : modelData.has ? "有" : "缺"; font.pixelSize: Theme.textSmall; font.weight: Theme.weightStrong; color: modelData.known === false ? Theme.muted : modelData.has ? Theme.fresh : Theme.nearing }
                            }
                            Text {
                                id: ingredientText
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: modelData.raw + (modelData.optional ? " · 可选" : "")
                                textFormat: Text.MarkdownText
                                font.pixelSize: Theme.textControl
                                color: Theme.ink
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
            Text { visible: !!page.recipe.quantities && page.recipe.quantities.length > 0; text: "参考用量"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Rectangle {
                visible: !!page.recipe.quantities && page.recipe.quantities.length > 0
                width: content.width
                height: visible ? quantityList.height + Theme.gap * 2 : 0
                radius: Theme.radiusCard
                color: Theme.card
                Column {
                    id: quantityList
                    x: Theme.gap
                    y: Theme.gap
                    width: parent.width - Theme.gap * 2
                    spacing: Theme.gapSmall
                    Repeater {
                        model: page.recipe.quantities || []
                        delegate: Text {
                            required property var modelData
                            width: quantityList.width
                            text: modelData.name + (modelData.expr ? "：" + modelData.expr : "")
                            textFormat: Text.MarkdownText
                            wrapMode: Text.Wrap
                            font.pixelSize: Theme.textControl
                            color: Theme.ink
                        }
                    }
                }
            }
            Repeater {
                model: page.recipe.ingredientNotes || []
                delegate: Text {
                    required property var modelData
                    width: content.width
                    text: modelData
                    textFormat: Text.MarkdownText
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.textControl
                    color: Theme.muted
                    onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                }
            }
            Repeater {
                model: page.recipe.quantityNotes || []
                delegate: Text {
                    required property var modelData
                    width: content.width
                    text: modelData
                    textFormat: Text.MarkdownText
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.textControl
                    color: Theme.muted
                    onLinkActivated: function(link) { Qt.openUrlExternally(link) }
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
                    required property int index
                    width: content.width
                    height: Math.max(stepNumber.implicitHeight, stepText.implicitHeight) + Theme.gap * 2
                    radius: Theme.radiusRow
                    color: Theme.card
                    Text {
                        id: stepNumber
                        x: Theme.gap
                        y: Theme.gap
                        text: String(index + 1) + "."
                        color: Theme.tomato
                        font.pixelSize: Theme.textControl
                        font.weight: Theme.weightBold
                    }
                    Text {
                        id: stepText
                        x: stepNumber.x + stepNumber.implicitWidth + Theme.gapSmall
                        y: Theme.gap
                        width: parent.width - x - Theme.gap
                        text: page.stepBody(modelData)
                        textFormat: Text.MarkdownText
                        wrapMode: Text.Wrap
                        color: Theme.ink
                        font.pixelSize: Theme.textControl
                        lineHeight: 1.35
                        onLinkActivated: function(link) { Qt.openUrlExternally(link) }
                    }
                }
            }
            Text { visible: !!page.recipe.notes; text: "补充说明"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Text {
                visible: !!page.recipe.notes
                width: content.width
                text: page.recipe.notes || ""
                textFormat: Text.MarkdownText
                wrapMode: Text.Wrap
                font.pixelSize: Theme.textControl
                color: Theme.muted
                onLinkActivated: function(link) { Qt.openUrlExternally(link) }
            }
        }
    }
}
