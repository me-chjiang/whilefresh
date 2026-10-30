import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import WhileFresh

Item {
    id: page
    signal backRequested()
    property string addError: ""
    Component.onCompleted: App.settings.refresh()
    PageHeader { id: header; x: Theme.gap; width: parent.width - Theme.gap * 2; heading: "偏好记忆"; actionText: "返回"; onActionTriggered: page.backRequested() }
    Flickable {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        contentHeight: content.height + Theme.gapLarge * 2
        Column {
            id: content
            x: Theme.gap
            width: parent.width - Theme.gap * 2
            spacing: Theme.gap
            Rectangle {
                width: content.width
                height: prefList.height + Theme.gap * 2
                radius: Theme.radiusCard
                color: Theme.card
                Column {
                    id: prefList
                    x: Theme.gap
                    y: Theme.gap
                    width: parent.width - Theme.gap * 2
                    spacing: Theme.gapTiny
                    Repeater {
                        model: App.settings.preferences
                        delegate: RowLayout {
                            required property var modelData
                            width: prefList.width
                            height: Theme.touch
                            Text { text: modelData.text; color: Theme.ink; font.pixelSize: Theme.textControl; Layout.fillWidth: true; elide: Text.ElideRight }
                            Text { visible: modelData.source === "ai"; text: "AI"; color: Theme.muted; font.pixelSize: Theme.textMeta }
                            Item {
                                width: Theme.touch
                                height: Theme.touch
                                IconGlyph { name: "x"; tint: Theme.muted; width: 20; height: 20; anchors.centerIn: parent }
                                TapHandler { onTapped: App.settings.deletePreference(modelData.id) }
                            }
                        }
                    }
                    RowLayout {
                        width: prefList.width
                        height: Theme.touch
                        TextField { id: prefInput; Layout.fillWidth: true; placeholderText: "添加偏好"; font.pixelSize: Theme.textControl; background: Rectangle { color: Theme.selectedPaper; radius: Theme.radiusTag; border.color: Theme.rule } }
                        InkButton {
                            text: "添加"
                            enabled: prefInput.text.trim() !== ""
                            onClicked: {
                                if (App.settings.addPreference(prefInput.text)) {
                                    prefInput.text = ""
                                    page.addError = ""
                                } else {
                                    page.addError = "添加失败"
                                }
                            }
                        }
                    }
                    Text { visible: page.addError !== ""; text: page.addError; font.pixelSize: Theme.textMeta; color: Theme.tomato }
                }
            }
            Text { text: "摘要"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Rectangle {
                width: content.width
                height: summaryColumn.height + Theme.gap * 2
                radius: Theme.radiusCard
                color: Theme.card
                Column {
                    id: summaryColumn
                    x: Theme.gap
                    y: Theme.gap
                    width: parent.width - Theme.gap * 2
                    spacing: Theme.gapSmall
                    Text { width: parent.width; wrapMode: Text.Wrap; text: App.settings.preferenceSummary !== "" ? App.settings.preferenceSummary : "无"; font.pixelSize: Theme.textControl; color: Theme.ink }
                    Text { visible: App.settings.summaryStale; text: "待整理"; font.pixelSize: Theme.textMeta; color: Theme.muted }
                    Text { visible: App.settings.summaryError !== ""; text: App.settings.summaryError; font.pixelSize: Theme.textMeta; color: Theme.tomato }
                    Text { visible: !App.settings.summarizeAvailable; text: "未配置"; font.pixelSize: Theme.textMeta; color: Theme.muted }
                    InkButton {
                        text: App.settings.summarizing ? "整理中" : "整理"
                        enabled: App.settings.summarizeAvailable && !App.settings.summarizing && App.settings.preferences.length > 0
                        onClicked: App.settings.summarize()
                    }
                }
            }
            RowLayout {
                width: content.width
                Text { text: "已记住的名称"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink; Layout.fillWidth: true }
                InkButton { visible: App.settings.nameMappings.length > 0; text: "全部清除"; outline: true; onClicked: clearAll.open() }
            }
            Rectangle {
                visible: App.settings.nameMappings.length > 0
                width: content.width
                height: visible ? mappingList.height + Theme.gap * 2 : 0
                radius: Theme.radiusCard
                color: Theme.card
                Column {
                    id: mappingList
                    x: Theme.gap
                    y: Theme.gap
                    width: parent.width - Theme.gap * 2
                    spacing: Theme.gapTiny
                    Repeater {
                        model: App.settings.nameMappings
                        delegate: RowLayout {
                            required property var modelData
                            width: mappingList.width
                            height: Theme.touch
                            Text { text: modelData.raw + " → " + modelData.canonical; color: Theme.ink; font.pixelSize: Theme.textControl; Layout.fillWidth: true; elide: Text.ElideRight }
                            Item {
                                width: Theme.touch
                                height: Theme.touch
                                IconGlyph { name: "x"; tint: Theme.muted; width: 20; height: 20; anchors.centerIn: parent }
                                TapHandler { onTapped: App.settings.forgetNameMapping(modelData.raw) }
                            }
                        }
                    }
                }
            }
            Text { visible: App.settings.nameMappings.length === 0; text: "无"; font.pixelSize: Theme.textControl; color: Theme.muted }
        }
    }
    ConfirmDialog { id: clearAll; message: "清除全部已记住的名称？"; confirmText: "全部清除"; destructive: true; onAcceptedAction: App.settings.clearNameMappings() }
}
