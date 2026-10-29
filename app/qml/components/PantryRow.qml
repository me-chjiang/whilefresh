import QtQuick
import QtQuick.Layouts
import WhileFresh

Item {
    id: row
    property var ingredient: ({})
    property bool multiSelect: false
    property bool checked: false
    readonly property bool actionReady: !multiSelect && !drag.active && !slideAnimation.running && front.x === -150
    signal editRequested(var item)
    signal selectionRequested(var item)
    signal used(var item)
    signal unavailable(var item)
    signal firstRequested(var item)
    height: Theme.rowHeight + Theme.gapSmall
    clip: true
    onMultiSelectChanged: if (multiSelect) front.x = 0

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        width: 150
        height: Theme.rowHeight
        Rectangle {
            width: parent.width / 2
            height: parent.height
            color: Theme.green
            Column {
                anchors.centerIn: parent
                IconGlyph { name: "check"; tint: Theme.textOnGreen; width: 20; height: 20; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: "用掉"; font.pixelSize: Theme.textSubtitle; font.weight: Theme.weightBold; color: Theme.textOnGreen }
            }
            TapHandler { enabled: row.actionReady; onTapped: { row.used(row.ingredient); front.x = 0 } }
        }
        Rectangle {
            width: parent.width / 2
            height: parent.height
            color: Theme.none
            Column {
                anchors.centerIn: parent
                IconGlyph { name: "ban"; tint: Theme.textOnGreen; width: 20; height: 20; anchors.horizontalCenter: parent.horizontalCenter }
                Text { text: "家里没有"; font.pixelSize: Theme.textSubtitle; font.weight: Theme.weightBold; color: Theme.textOnGreen }
            }
            TapHandler { enabled: row.actionReady; onTapped: { row.unavailable(row.ingredient); front.x = 0 } }
        }
    }
    Rectangle {
        id: front
        width: parent.width
        height: Theme.rowHeight
        radius: Theme.radiusRow
        color: Theme.card
        border.color: row.checked ? Theme.tomato : "transparent"
        border.width: row.checked ? 1 : 0
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.gapSmall
            anchors.rightMargin: Theme.gapSmall
            spacing: Theme.gapSmall
            Rectangle {
                visible: row.multiSelect
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                color: row.checked ? Theme.tomato : Theme.selectedPaper
                radius: Theme.radiusTag
                border.color: Theme.outline
                Text { anchors.centerIn: parent; visible: row.checked; text: "✓"; color: Theme.textOnTomato; font.weight: Theme.weightBold }
            }
            CategoryDot { category: row.ingredient.category || "other" }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gapSmall
                    Text {
                        Layout.fillWidth: true
                        text: row.ingredient.name || ""
                        font.pixelSize: Theme.textBody
                        font.weight: Theme.weightBold
                        color: Theme.ink
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        visible: row.ingredient.first || false
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: Theme.badgeHeight
                        radius: Theme.radiusTag
                        color: Theme.mustard
                        Text { anchors.centerIn: parent; text: "先用"; font.pixelSize: Theme.textSmall; font.weight: Theme.weightStrong; color: Theme.ink }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: (row.ingredient.days || "") + " · " + Theme.sourceLabel(row.ingredient.source) + (row.ingredient.source === "package" ? "" : " · 估计")
                    font.pixelSize: Theme.textMeta
                    color: Theme.muted
                    elide: Text.ElideRight
                }
            }
            Text {
                text: row.ingredient.quantity || ""
                font.pixelSize: Theme.textQuantity
                font.weight: Theme.weightBold
                color: Theme.ink
            }
            FreshnessStamp { status: row.ingredient.freshness || "fresh" }
        }
        DragHandler {
            id: drag
            enabled: !row.multiSelect
            target: front
            xAxis.minimum: -150
            xAxis.maximum: 90
            yAxis.enabled: false
            onActiveChanged: {
                if (!active) {
                    if (front.x > 55) {
                        row.firstRequested(row.ingredient)
                        front.x = 0
                    } else {
                        front.x = front.x < -55 ? -150 : 0
                    }
                }
            }
        }
        TapHandler {
            onTapped: {
                if (front.x < 0) front.x = 0
                else if (row.multiSelect) row.selectionRequested(row.ingredient)
                else row.editRequested(row.ingredient)
            }
            onLongPressed: if (front.x === 0) row.selectionRequested(row.ingredient)
        }
        Behavior on x {
            enabled: !drag.active
            NumberAnimation { id: slideAnimation; duration: 160 }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.rule
    }
}
