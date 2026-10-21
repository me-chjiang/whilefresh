import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import WhileFresh

Item {
    id: flow
    property string stage: "choose"
    property string method: "gallery"
    property bool ignoredExpanded: false
    readonly property string view: App.importState !== "idle" ? App.importState : stage
    readonly property var foods: App.draftItems.filter(function(c) { return c.group === "food" })
    readonly property var seasonings: App.draftItems.filter(function(c) { return c.group === "seasoning" })
    readonly property var ignored: App.draftItems.filter(function(c) { return c.group === "ignored" })
    signal finished()
    signal notice(string message)
    function reset() { stage = "choose"; ignoredExpanded = false }
    function cancelled() { stage = "choose" }
    Connections { target: App; function onImportCommitted() { flow.finished() } }
    Column {
        anchors.fill: parent
        anchors.leftMargin: Theme.gap
        anchors.rightMargin: Theme.gap
        spacing: Theme.gapSmall
        Text {
            width: parent.width
            text: flow.view === "choose" ? "选择入库方式" : flow.view === "privacy" || flow.view === "prepared" ? "上传前请确认" : flow.view === "recognizing" ? "识别中" : flow.view === "duplicate" ? "疑似重复导入" : flow.view === "failed" ? "没有识别出食材" : flow.view === "manual" ? "手动添加食材" : "请检查并修改"
            font.family: Theme.serif
            font.pixelSize: Theme.textPanel
            font.weight: Theme.weightBlack
            color: Theme.ink
        }
        NoticeBanner {
            width: parent.width
            visible: App.demoMode
            height: visible ? implicitHeight : 0
            message: "演示模式：使用预设识别结果，不会发送任何内容给 DeepSeek"
            warning: false
        }
        Column {
            visible: flow.view === "choose"
            width: parent.width
            spacing: Theme.gapSmall
            InkButton { width: parent.width; text: "相册截图 · 可多选"; iconName: "notes"; onClicked: { flow.method = "gallery"; flow.stage = "privacy" } }
            InkButton { width: parent.width; text: "拍购物小票"; iconName: "pencil"; outline: true; onClicked: { flow.method = "camera"; flow.stage = "privacy" } }
            InkButton { width: parent.width; text: "粘贴订单文字"; iconName: "notes"; outline: true; onClicked: { flow.method = "text"; flow.stage = "privacy" } }
            InkButton { width: parent.width; text: "手动添加"; iconName: "plus"; outline: true; onClicked: flow.stage = "manual" }
        }
        Column {
            visible: flow.view === "privacy"
            width: parent.width
            spacing: Theme.gap
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: App.demoMode ? "演示模式不会上传任何内容，将返回预设的识别结果。" : App.privacyNotice
                font.pixelSize: Theme.textControl
                color: Theme.ink
            }
            NoticeBanner {
                visible: !App.demoMode && !App.recognitionConfigured
                width: parent.width
                height: visible ? implicitHeight : 0
                message: "未配置识别服务密钥，无法真实识别。可在「我的」开启演示模式，或手动添加"
            }
            TextArea {
                id: pastedText
                visible: flow.method === "text"
                width: parent.width
                height: 120
                placeholderText: "在此粘贴订单文字"
                wrapMode: TextEdit.Wrap
                background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusRow }
            }
            InkButton {
                width: parent.width
                text: flow.method === "text" ? "同意并识别" : "同意并选择图片"
                onClicked: {
                    if (flow.method === "text") App.beginText(pastedText.text)
                    else App.beginPick(flow.method === "camera")
                }
            }
            InkButton { width: parent.width; text: "返回"; outline: true; onClicked: flow.stage = "choose" }
        }
        Column {
            visible: flow.view === "prepared"
            width: parent.width
            spacing: Theme.gap
            Text { width: parent.width; wrapMode: Text.Wrap; text: App.demoMode ? "演示模式不会上传任何内容。" : App.privacyNotice; font.pixelSize: Theme.textControl; color: Theme.ink }
            InkButton { width: parent.width; text: "同意并识别"; onClicked: App.startImport(false) }
            InkButton { width: parent.width; text: "放弃"; outline: true; onClicked: { App.cancelImport(); flow.finished() } }
        }
        Column {
            visible: flow.view === "duplicate"
            width: parent.width
            spacing: Theme.gap
            NoticeBanner { width: parent.width; message: "这些图片或文字与之前的导入相同。仍要识别并入库吗？" }
            InkButton { width: parent.width; text: "仍然识别"; onClicked: App.startImport(true) }
            InkButton { width: parent.width; text: "放弃"; outline: true; onClicked: { App.cancelImport(); flow.finished() } }
        }
        Column {
            visible: flow.view === "recognizing"
            width: parent.width
            spacing: Theme.gap
            BusyIndicator { running: parent.visible; anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: App.demoMode ? "正在读取预设识别结果" : "正在识别，请稍候"; color: Theme.muted; font.pixelSize: Theme.textSubtitle; anchors.horizontalCenter: parent.horizontalCenter }
            InkButton { text: "取消"; outline: true; anchors.horizontalCenter: parent.horizontalCenter; onClicked: { App.cancelImport(); flow.finished() } }
        }
        Column {
            visible: flow.view === "failed"
            width: parent.width
            spacing: Theme.gap
            NoticeBanner { width: parent.width; message: (App.importError || "识别失败") + "。可以重试，或改为手动添加" }
            InkButton { width: parent.width; text: "重试"; onClicked: App.startImport(false) }
            InkButton { width: parent.width; text: "改为手动添加"; outline: true; onClicked: { App.cancelImport(); flow.stage = "manual" } }
        }
        Column {
            visible: flow.view === "manual"
            width: parent.width
            spacing: Theme.gapSmall
            TextField { id: manualName; width: parent.width; height: Theme.touch; placeholderText: "食材名称"; background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusRow } }
            TextField { id: manualQuantity; width: parent.width; height: Theme.touch; placeholderText: "数量原值，如 500g"; background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusRow } }
            ComboBox { id: manualLocation; width: parent.width; model: ["冷藏", "冷冻", "常温"] }
            InkButton {
                width: parent.width
                text: "添加食材"
                onClicked: {
                    const name = manualName.text.trim()
                    if (!name) { flow.notice("请先填写食材名称"); return }
                    const result = App.addManual(name, manualQuantity.text.trim(), manualLocation.currentText)
                    if (result.ok) flow.finished()
                    else flow.notice(result.error || "添加失败")
                }
            }
        }
        Item {
            visible: flow.view === "draft"
            width: parent.width
            height: parent.height - y
            Column {
                anchors.fill: parent
                spacing: Theme.gapSmall
                Text { text: "识别出 " + flow.foods.length + " 项食材 · 核对后入库"; font.pixelSize: Theme.textSubtitle; color: Theme.muted }
                NoticeBanner { visible: App.draftWarning !== ""; width: parent.width; height: visible ? implicitHeight : 0; message: App.draftWarning }
                NoticeBanner { visible: App.draftDuplicate; width: parent.width; height: visible ? implicitHeight : 0; message: "疑似重复导入：确认入库将再次添加这些食材" }
                Flickable {
                    id: confirmScroll
                    width: parent.width
                    height: parent.height - y - Theme.touch - Theme.gap
                    clip: true
                    contentHeight: groups.height + Theme.gapLarge
                    Column {
                        id: groups
                        width: parent.width
                        spacing: Theme.gapSmall
                        Text { text: "食材  ·  " + flow.foods.length + " 项"; font.family: Theme.serif; font.pixelSize: Theme.textGroup; font.weight: Theme.weightBlack; color: Theme.ink }
                        Repeater {
                            model: flow.foods
                            delegate: Rectangle {
                                id: ingredient
                                required property var modelData
                                width: groups.width
                                height: editColumn.height + Theme.gap * 2
                                radius: Theme.radiusRow
                                color: Theme.card
                                Column {
                                    id: editColumn
                                    x: Theme.gapSmall
                                    y: Theme.gapSmall
                                    width: parent.width - Theme.gapSmall * 2
                                    spacing: Theme.gapTiny
                                    RowLayout {
                                        width: parent.width
                                        CategoryDot { category: ingredient.modelData.category }
                                        TextField {
                                            id: foodName
                                            Layout.fillWidth: true
                                            text: ingredient.modelData.name
                                            font.pixelSize: Theme.textQuantity
                                            font.weight: Theme.weightBold
                                            color: Theme.ink
                                            background: Item {}
                                            onEditingFinished: App.editDraft(ingredient.modelData.index, { name: text.trim() })
                                        }
                                        TextField {
                                            Layout.preferredWidth: 76
                                            text: ingredient.modelData.quantity
                                            font.pixelSize: Theme.textControl
                                            color: Theme.ink
                                            background: Rectangle { radius: Theme.radiusTag; color: Theme.note }
                                            onEditingFinished: App.editDraft(ingredient.modelData.index, { quantity: text.trim() })
                                        }
                                        ComboBox {
                                            id: location
                                            Layout.preferredWidth: 100
                                            model: ["冷藏", "冷冻", "常温"]
                                            currentIndex: model.indexOf(ingredient.modelData.location)
                                            onActivated: App.editDraft(ingredient.modelData.index, { location: currentText })
                                        }
                                    }
                                    Text {
                                        text: (ingredient.modelData.source === "package" ? "" : "估计 · ") + Theme.sourceLabel(ingredient.modelData.source) + (ingredient.modelData.shelfDays ? " · 保质 " + ingredient.modelData.shelfDays + " 天" : "")
                                        font.pixelSize: Theme.textMeta
                                        color: ingredient.modelData.source === "package" ? Theme.fresh : Theme.nearing
                                    }
                                    Text { visible: ingredient.modelData.needsDate; text: "日期写法需要拍摄日期才能换算，请在下方填写具体日期"; font.pixelSize: Theme.textMeta; color: Theme.overdue; width: parent.width; wrapMode: Text.Wrap }
                                    TextField {
                                        width: parent.width
                                        height: 34
                                        text: ingredient.modelData.shelfDays || ""
                                        placeholderText: "保质天数（包装标注可留空）"
                                        inputMethodHints: Qt.ImhDigitsOnly
                                        font.pixelSize: Theme.textMeta
                                        background: Rectangle { radius: Theme.radiusTag; color: Theme.selectedPaper; border.color: Theme.rule }
                                        onEditingFinished: App.editDraft(ingredient.modelData.index, { shelfDays: text.trim() })
                                    }
                                    RowLayout {
                                        width: parent.width
                                        spacing: Theme.gapTiny
                                        Repeater {
                                            model: [ { key: "purchasedAt", title: "购买" }, { key: "storedAt", title: "入库" }, { key: "producedAt", title: "生产" }, { key: "packageExpiresAt", title: "包装到期" } ]
                                            delegate: Column {
                                                required property var modelData
                                                Layout.fillWidth: true
                                                Text { text: modelData.title; font.pixelSize: Theme.textSmall; color: Theme.muted }
                                                TextField {
                                                    width: parent.width
                                                    height: 34
                                                    text: ingredient.modelData[modelData.key] || ""
                                                    placeholderText: "日期"
                                                    font.pixelSize: Theme.textSmall
                                                    background: Rectangle { color: Theme.selectedPaper; radius: Theme.radiusTag; border.color: Theme.rule }
                                                    onEditingFinished: {
                                                        const patch = {}
                                                        patch[modelData.key] = text.trim()
                                                        App.editDraft(ingredient.modelData.index, patch)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Text { text: "调料  ·  更新常备调料"; font.family: Theme.serif; font.pixelSize: Theme.textGroup; font.weight: Theme.weightBlack; color: Theme.ink }
                        Flow {
                            width: parent.width
                            height: childrenRect.height
                            spacing: Theme.gapSmall
                            Repeater {
                                model: flow.seasonings
                                delegate: Item {
                                    id: seasoningChip
                                    required property var modelData
                                    width: label.implicitWidth + Theme.gapLarge * 2
                                    height: Theme.touch
                                    Rectangle { anchors.fill: parent; anchors.margins: Theme.gapTiny; radius: Theme.radiusRow; color: Theme.selectedPaper; border.color: Theme.ink }
                                    Text { id: label; anchors.centerIn: parent; text: seasoningChip.modelData.name; color: Theme.ink; font.pixelSize: Theme.textControl }
                                    TapHandler { onTapped: App.dropDraft(seasoningChip.modelData.index) }
                                }
                            }
                        }
                        Rectangle {
                            width: parent.width
                            height: Theme.touch
                            radius: Theme.radiusRow
                            color: Theme.selectedPaper
                            border.color: Theme.outline
                            Text { x: Theme.gap; anchors.verticalCenter: parent.verticalCenter; text: "已忽略 " + flow.ignored.length + " 项 · 点击找回"; font.pixelSize: Theme.textControl; color: Theme.muted }
                            TapHandler { onTapped: flow.ignoredExpanded = !flow.ignoredExpanded }
                        }
                        Repeater {
                            model: flow.ignoredExpanded ? flow.ignored : []
                            delegate: InkButton {
                                required property var modelData
                                width: groups.width
                                text: modelData.name + " · 找回为食材"
                                outline: true
                                onClicked: App.retrieveDraft(modelData.index)
                            }
                        }
                    }
                }
                InkButton { width: parent.width; text: (App.draftDuplicate ? "仍然入库 · " : "确认入库 · ") + flow.foods.length + " 项食材"; titleStyle: true; onClicked: App.confirmImport() }
            }
        }
    }
}
