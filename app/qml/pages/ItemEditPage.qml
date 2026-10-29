import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import WhileFresh

Item {
    id: page
    required property var ingredient
    signal backRequested()
    property var dateEdits: ({})
    property string saveError: ""
    PageHeader { id: header; x: Theme.gap; width: parent.width - Theme.gap * 2; heading: "编辑食材"; backVisible: true; onBackRequested: page.backRequested() }
    Flickable {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        contentHeight: form.height + Theme.gapLarge * 2
        Column {
            id: form
            x: Theme.gap
            width: parent.width - Theme.gap * 2
            spacing: Theme.gap
            Rectangle {
                width: parent.width
                height: fields.height + Theme.gap * 2
                radius: Theme.radiusCard
                color: Theme.card
                Column {
                    id: fields
                    x: Theme.gap
                    y: Theme.gap
                    width: parent.width - Theme.gap * 2
                    spacing: Theme.gapSmall
                    Text { text: "名称与数量"; font.family: Theme.serif; font.pixelSize: Theme.textPanel; color: Theme.ink }
                    TextField { id: itemName; width: parent.width; height: Theme.touch; text: page.ingredient.name; placeholderText: "名称"; background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusTag } }
                    TextField { id: quantity; width: parent.width; height: Theme.touch; text: page.ingredient.quantity; placeholderText: "数量原值"; background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusTag } }
                    Text { text: "存放位置"; font.pixelSize: Theme.textSubtitle; color: Theme.muted }
                    ComboBox { id: location; width: parent.width; height: Theme.touch; model: ["冷藏", "冷冻", "常温"]; currentIndex: model.indexOf(page.ingredient.location) }
                    Text { text: "保质天数  ·  " + Theme.sourceLabel(page.ingredient.source) + (page.ingredient.source === "package" ? "" : " · 估计"); font.pixelSize: Theme.textSubtitle; color: Theme.muted }
                    TextField { id: shelfDays; width: parent.width; height: Theme.touch; text: page.ingredient.shelfDays || ""; placeholderText: "保质天数"; inputMethodHints: Qt.ImhDigitsOnly; background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusTag } }
                    Repeater {
                        model: [{ key: "purchasedAt", label: "购买日期" }, { key: "storedAt", label: "入库日期" }, { key: "producedAt", label: "生产日期" }, { key: "packageExpiresAt", label: "包装到期日" }]
                        delegate: RowLayout {
                            required property var modelData
                            width: fields.width
                            height: Theme.touch
                            Text { text: modelData.label; font.pixelSize: Theme.textSubtitle; color: Theme.muted; Layout.preferredWidth: 90 }
                            TextField {
                                id: dateField
                                property string fieldKey: modelData.key
                                Layout.fillWidth: true
                                height: Theme.touch
                                text: page.ingredient[fieldKey] || ""
                                placeholderText: "YYYY-MM-DD（可留空）"
                                font.pixelSize: Theme.textControl
                                color: Theme.ink
                                background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusTag }
                                onTextChanged: {
                                    const patch = {}
                                    patch[fieldKey] = text.trim()
                                    page.dateEdits = Object.assign({}, page.dateEdits, patch)
                                }
                            }
                        }
                    }
                }
            }
            NoticeBanner { width: parent.width; message: page.ingredient.freshness === "overdue" ? "超出估计期限，请查看后决定" : "非包装标注的期限仅为估计，请结合食材状态判断" }
            InkButton {
                width: parent.width
                text: "保存修改"
                titleStyle: true
                onClicked: {
                    if (!itemName.text.trim()) return
                    const edits = { name: itemName.text.trim(), quantity: quantity.text.trim(), location: location.currentText, shelfDays: shelfDays.text.trim() }
                    const keys = ["purchasedAt", "storedAt", "producedAt", "packageExpiresAt"]
                    keys.forEach(function(key) { edits[key] = page.dateEdits[key] === undefined ? (page.ingredient[key] || "") : page.dateEdits[key] })
                    const result = App.saveItem(page.ingredient.id, edits)
                    if (!result.ok) { page.saveError = result.error || "保存失败，请检查输入"; return }
                    if (result.question) { rememberDialog.message = result.question; rememberDialog.open() }
                    else page.backRequested()
                }
            }
            NoticeBanner { visible: page.saveError !== ""; width: parent.width; height: visible ? implicitHeight : 0; message: page.saveError }
        }
    }
    ConfirmDialog {
        id: rememberDialog
        message: "以后同类食材都按这个存放位置和保质天数吗？"
        confirmText: "记住这个设置"
        onAcceptedAction: { App.confirmRememberedRules(); page.backRequested() }
        onRejected: { App.dismissRememberedRules(); page.backRequested() }
    }
}
