import QtQuick
import QtQuick.Controls.Basic
import WhileFresh

Item {
    id: page
    signal webRecipeRequested(string id)
    property string query: ""
    PageHeader {
        id: header
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        heading: "菜谱"
        subtitle: "下厨房"
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
                    onAccepted: if (page.query !== "") App.searchRecipes(page.query)
                }
            }
        }
        InkButton {
            width: parent.width
            text: App.web.searchState === "busy" ? "取消搜索" : "搜下厨房"
            iconName: "notes"
            outline: App.web.searchState === "busy"
            enabled: App.web.searchState === "busy" || page.query !== ""
            onClicked: App.web.searchState === "busy" ? App.cancelRecipeRequest() : App.searchRecipes(page.query)
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
                Text {
                    visible: App.web.searchState === "busy" || App.web.searchState === "error"
                    text: App.web.searchState === "busy" ? "搜索中…" : App.web.searchError
                    font.pixelSize: Theme.textSubtitle
                    color: App.web.searchState === "error" ? Theme.nearing : Theme.muted
                }
                Repeater {
                    model: App.web.results
                    delegate: Rectangle {
                        required property var modelData
                        width: list.width
                        height: Theme.touch + Theme.gapSmall
                        radius: Theme.radiusRow
                        color: Theme.card
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.gap
                            spacing: Theme.gapSmall
                            Text {
                                width: parent.width - 60
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name
                                elide: Text.ElideRight
                                font.pixelSize: Theme.textRecipe
                                color: Theme.ink
                            }
                            Text { anchors.verticalCenter: parent.verticalCenter; text: "下厨房"; font.pixelSize: Theme.textMeta; color: Theme.muted }
                        }
                        MouseArea { anchors.fill: parent; onClicked: page.webRecipeRequested(modelData.id) }
                    }
                }
                EmptyState {
                    visible: App.web.searchState === "done" && App.web.results.length === 0
                    width: parent.width
                    height: visible ? implicitHeight + Theme.gapLarge * 2 : 0
                    iconName: "book-2"
                    heading: "没有找到菜谱"
                    detail: ""
                }
            }
        }
    }
}
