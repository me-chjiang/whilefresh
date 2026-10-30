import QtQuick
import WhileFresh

Item {
    id: page
    signal backRequested()
    readonly property var recipe: App.web.detail
    PageHeader {
        id: header
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        heading: "菜谱卡"
        actionText: "返回"
        onActionTriggered: page.backRequested()
    }
    Text {
        anchors.top: header.bottom
        x: Theme.gap
        visible: App.web.detailState === "busy" || App.web.detailState === "error"
        text: App.web.detailState === "busy" ? "读取中…" : App.web.detailError
        font.pixelSize: Theme.textSubtitle
        color: App.web.detailState === "error" ? Theme.nearing : Theme.muted
    }
    Flickable {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        visible: App.web.detailState === "done"
        contentHeight: content.height + Theme.gapLarge
        clip: true
        Column {
            id: content
            x: Theme.gap
            width: parent.width - Theme.gap * 2
            spacing: Theme.gap
            Text {
                width: parent.width
                text: page.recipe.name || ""
                wrapMode: Text.Wrap
                font.family: Theme.serif
                font.pixelSize: Theme.textSheetTitle
                font.weight: Theme.weightBlack
                color: Theme.ink
            }
            Text {
                text: "来源：" + ((page.recipe.attribution || {}).source || "") + (page.recipe.partial ? " · 内容不全" : "")
                font.pixelSize: Theme.textSubtitle
                color: Theme.muted
            }
            InkButton {
                text: "查看原菜谱"
                iconName: "book-2"
                outline: true
                width: content.width
                onClicked: Qt.openUrlExternally((page.recipe.attribution || {}).url)
            }
            Text { text: "用料"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Repeater {
                model: page.recipe.ingredients || []
                delegate: Text {
                    required property var modelData
                    width: content.width
                    text: modelData.raw
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.textControl
                    color: Theme.ink
                }
            }
            Text { text: "步骤"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Repeater {
                model: page.recipe.steps || []
                delegate: Text {
                    required property var modelData
                    width: content.width
                    text: modelData.order + ". " + modelData.text
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.textControl
                    color: Theme.ink
                    lineHeight: 1.35
                }
            }
            Repeater {
                model: page.recipe.tips || []
                delegate: Text {
                    required property string modelData
                    width: content.width
                    text: "小贴士：" + modelData
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.textControl
                    color: Theme.muted
                }
            }
        }
    }
}
