import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import WhileFresh

Item {
    id: page
    signal notice(string message)
    signal debugRequested()
    signal backRequested()
    property bool canGoBack: false
    property bool debugTools: false
    property string pendingImportName: ""
    property int pendingImportId: -1
    PageHeader { id: header; x: Theme.gap; width: parent.width - Theme.gap * 2; heading: "我的"; subtitle: "习惯与数据"; backVisible: page.canGoBack; onBackRequested: page.backRequested() }
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
            NoticeBanner { width: content.width; message: App.demoMode ? "演示模式：使用独立示例数据和预设识别结果，不联网，不影响真实数据" : (App.recognitionConfigured ? "识别服务已配置。入库识别前会再次提示隐私说明" : "未配置识别服务密钥，真实识别不可用；可开启演示模式体验流程"); warning: !App.demoMode && !App.recognitionConfigured }
            Rectangle {
                width: content.width
                height: demoRow.height + Theme.gapLarge
                radius: Theme.radiusCard
                color: Theme.card
                RowLayout {
                    id: demoRow
                    x: Theme.gap
                    y: Theme.gapSmall
                    width: parent.width - Theme.gap * 2
                    height: Theme.touch
                    Text { text: "演示模式"; font.pixelSize: Theme.textBody; font.weight: Theme.weightBold; color: Theme.ink; Layout.fillWidth: true }
                    Switch { checked: App.demoMode; onToggled: App.setDemoMode(checked) }
                }
            }
            Text { text: "常备调料 / 常用食材"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Text { text: "清单内默认家里有；存量多的食材用掉一次仍保留。建议更改需确认。"; width: content.width; wrapMode: Text.Wrap; font.pixelSize: Theme.textMeta; color: Theme.muted }
            Rectangle {
                width: content.width
                height: pantryList.height + Theme.gap * 2
                radius: Theme.radiusCard
                color: Theme.card
                Column {
                    id: pantryList
                    x: Theme.gap
                    y: Theme.gap
                    width: parent.width - Theme.gap * 2
                    spacing: Theme.gapTiny
                    Repeater {
                        model: App.pantry
                        delegate: RowLayout {
                            required property var modelData
                            width: pantryList.width
                            height: Theme.touch
                            CategoryDot { category: modelData.category }
                            Text { text: modelData.name; color: Theme.ink; font.pixelSize: Theme.textControl; Layout.fillWidth: true }
                            Text { text: modelData.bulk ? "默认存量多" : "默认有"; color: Theme.muted; font.pixelSize: Theme.textMeta }
                            Item {
                                width: Theme.touch
                                height: Theme.touch
                                IconGlyph { name: "x"; tint: Theme.muted; width: 20; height: 20; anchors.centerIn: parent }
                                TapHandler { onTapped: App.removePantry(modelData.name) }
                            }
                        }
                    }
                    RowLayout {
                        width: pantryList.width
                        height: Theme.touch
                        TextField { id: pantryInput; Layout.fillWidth: true; placeholderText: "添加常备食材"; font.pixelSize: Theme.textControl; background: Rectangle { color: Theme.selectedPaper; radius: Theme.radiusTag; border.color: Theme.rule } }
                        InkButton {
                            text: "添加"
                            onClicked: {
                                const value = pantryInput.text.trim()
                                if (value) {
                                    App.addPantry(value)
                                    pantryInput.text = ""
                                }
                            }
                        }
                    }
                }
            }
            Text { text: "待确认建议"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            NoticeBanner { visible: App.suggestions.length === 0; width: content.width; height: visible ? implicitHeight : 0; message: "暂无待确认建议"; warning: false }
            Repeater {
                model: App.suggestions
                delegate: Rectangle {
                    required property var modelData
                    width: content.width
                    height: suggestColumn.height + Theme.gap * 2
                    color: Theme.note
                    radius: Theme.radiusRow
                    Column {
                        id: suggestColumn
                        x: Theme.gap
                        y: Theme.gap
                        spacing: Theme.gapSmall
                        Text { text: modelData.name + " · " + modelData.note; font.pixelSize: Theme.textSubtitle; color: Theme.ink }
                        Row {
                            spacing: Theme.gapSmall
                            InkButton { text: "确认变更"; onClicked: App.decideSuggestion(modelData.id, true) }
                            InkButton { text: "不变更"; outline: true; onClicked: App.decideSuggestion(modelData.id, false) }
                        }
                    }
                }
            }
            Text { text: "保质期与存放规则"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Repeater {
                model: App.rules
                delegate: Rectangle {
                    required property var modelData
                    width: content.width
                    height: ruleColumn.height + Theme.gap * 2
                    color: Theme.card
                    radius: Theme.radiusRow
                    Column {
                        id: ruleColumn
                        x: Theme.gap
                        y: Theme.gap
                        spacing: Theme.gapSmall
                        Text { text: modelData.name + " · " + modelData.source; color: Theme.ink; font.pixelSize: Theme.textControl; font.weight: Theme.weightBold }
                        RowLayout {
                            width: content.width - Theme.gap * 2
                            height: Theme.touch
                            TextField { id: ruleDays; text: modelData.days; Layout.preferredWidth: 65; placeholderText: "天数"; inputMethodHints: Qt.ImhDigitsOnly; background: Rectangle { color: Theme.note; radius: Theme.radiusTag } }
                            ComboBox { id: rulePlace; model: ["冷藏", "冷冻", "常温"]; currentIndex: model.indexOf(modelData.location); Layout.fillWidth: true }
                            InkButton { text: "保存"; onClicked: if (App.saveRule(modelData.name, modelData.location, rulePlace.currentText, ruleDays.text)) page.notice("已保存规则") }
                            Item {
                                width: Theme.touch
                                height: Theme.touch
                                IconGlyph { anchors.centerIn: parent; name: "x"; tint: Theme.muted; width: 20; height: 20 }
                                TapHandler { onTapped: App.deleteRule(modelData.name, modelData.location, modelData.kind) }
                            }
                        }
                    }
                }
            }
            Text { text: "导入记录与本机数据"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Repeater {
                model: App.imports
                delegate: RowLayout {
                    required property var modelData
                    width: content.width
                    height: Theme.touch
                    Text { text: modelData.name + " · " + modelData.date; Layout.fillWidth: true; font.pixelSize: Theme.textSubtitle; color: Theme.ink; elide: Text.ElideRight }
                    InkButton { text: "删除本次"; outline: true; onClicked: { page.pendingImportId = modelData.id; deleteOne.open() } }
                }
            }
            InkButton { text: "删除全部数据"; accent: Theme.tomato; width: content.width; onClicked: deleteAll.open() }
            InkButton { visible: page.debugTools; width: content.width; text: "调试 · 平台导入面板"; outline: true; onClicked: page.debugRequested() }
            Text { text: "来源与许可"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; font.weight: Theme.weightBlack; color: Theme.ink }
            Rectangle {
                width: content.width
                height: credits.height + Theme.gap * 2
                color: Theme.card
                radius: Theme.radiusRow
                Column {
                    id: credits
                    x: Theme.gap
                    y: Theme.gap
                    width: parent.width - Theme.gap * 2
                    spacing: Theme.gapSmall
                    Text { width: parent.width; wrapMode: Text.Wrap; text: "菜谱：HowToCook · Unlicense（原文及来源链接见菜谱详情）"; font.pixelSize: Theme.textSubtitle; color: Theme.ink }
                    Text { width: parent.width; wrapMode: Text.Wrap; text: "保质期参考：USDA FoodKeeper。仅作估计，请查看食材后决定。"; font.pixelSize: Theme.textSubtitle; color: Theme.ink }
                    Text { width: parent.width; wrapMode: Text.Wrap; text: "图标：Tabler Icons · MIT；标题字：Noto Serif SC Black · SIL OFL 1.1。许可文本随 App 附带。"; font.pixelSize: Theme.textSubtitle; color: Theme.ink }
                    Text { width: parent.width; wrapMode: Text.Wrap; text: "基于 Qt 6.8.2（LGPL v3，动态链接，源码见 qt.io）与 OpenSSL 3（Apache 2.0）构建。"; font.pixelSize: Theme.textSubtitle; color: Theme.ink }
                }
            }
        }
    }
    ConfirmDialog { id: deleteOne; message: "删除这次导入记录及其截图？"; confirmText: "删除本次"; destructive: true; onAcceptedAction: App.deleteImport(page.pendingImportId) }
    ConfirmDialog { id: deleteAll; message: "删除当前模式下的全部食材、待买、规则和导入记录？"; confirmText: "删除全部"; destructive: true; onAcceptedAction: { App.deleteAllData(); page.notice("已清空数据") } }
}
