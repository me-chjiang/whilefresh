import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import WhileFresh

Item {
    id: flow
    property QtObject platform
    property string stage: "choose"
    property string method: "gallery"
    property bool ignoredExpanded: false
    signal finished()
    signal notice(string message)
    function sharedImport() { method = "share"; stage = "recognizing"; recognitionTimer.restart() }
    function picked() { stage = "recognizing"; recognitionTimer.restart() }
    function cancelled() { stage = "choose" }
    Timer { id: recognitionTimer; interval: 1200; onTriggered: flow.stage = "confirm" }
    Column {
        anchors.fill: parent
        anchors.leftMargin: Theme.gap
        anchors.rightMargin: Theme.gap
        spacing: Theme.gapSmall
        Text {
            width: parent.width
            text: flow.stage === "choose" ? "选择入库方式" : flow.stage === "privacy" ? "上传前请确认" : flow.stage === "recognizing" ? "识别中 · 模拟" : flow.stage === "failed" ? "没有识别出食材" : flow.stage === "manual" ? "手动添加食材" : "请检查并修改"
            font.family: Theme.serif
            font.pixelSize: Theme.textPanel
            font.weight: Theme.weightBlack
            color: Theme.ink
        }
        NoticeBanner {
            width: parent.width
            message: "演示 · 界面预览。这里仅显示模拟识别结果，不会发送任何内容给 DeepSeek"
            warning: false
        }
        Column {
            visible: flow.stage === "choose"
            width: parent.width
            spacing: Theme.gapSmall
            InkButton { width: parent.width; text: "相册截图 · 可多选"; iconName: "notes"; onClicked: { flow.method = "gallery"; flow.stage = "privacy" } }
            InkButton { width: parent.width; text: "拍购物小票"; iconName: "pencil"; outline: true; onClicked: { flow.method = "camera"; flow.stage = "privacy" } }
            InkButton { width: parent.width; text: "粘贴订单文字"; iconName: "notes"; outline: true; onClicked: { flow.method = "text"; flow.stage = "privacy" } }
            InkButton { width: parent.width; text: "手动添加"; iconName: "plus"; outline: true; onClicked: flow.stage = "manual" }
        }
        Column {
            visible: flow.stage === "privacy"
            width: parent.width
            spacing: Theme.gap
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: "真实识别将发送截图或文字给 DeepSeek；截图可能包含地址等个人信息。请先检查内容。当前仅模拟，不上传。"
                font.pixelSize: Theme.textControl
                color: Theme.ink
            }
            TextArea {
                id: pastedText
                visible: flow.method === "text"
                width: parent.width
                height: 120
                placeholderText: "在此粘贴订单文字（仅在本机用于界面预览）"
                wrapMode: TextEdit.Wrap
                background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusRow }
            }
            InkButton {
                width: parent.width
                text: flow.method === "text" ? "开始模拟识别" : "继续选择，模拟识别"
                onClicked: {
                    if (flow.method === "gallery" && flow.platform) flow.platform.pickImages()
                    else if (flow.method === "camera" && flow.platform) flow.platform.captureReceipt()
                    else { pastedText.text = ""; flow.picked() }
                }
            }
            InkButton { width: parent.width; text: "返回"; outline: true; onClicked: flow.stage = "choose" }
        }
        Column {
            visible: flow.stage === "recognizing"
            width: parent.width
            spacing: Theme.gap
            BusyIndicator { running: parent.visible; anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: "正在整理模拟食材，未调用识别服务"; color: Theme.muted; font.pixelSize: Theme.textSubtitle; anchors.horizontalCenter: parent.horizontalCenter }
            InkButton { text: "预览失败状态"; outline: true; anchors.horizontalCenter: parent.horizontalCenter; onClicked: { recognitionTimer.stop(); flow.stage = "failed" } }
        }
        Column {
            visible: flow.stage === "failed"
            width: parent.width
            spacing: Theme.gap
            NoticeBanner { width: parent.width; message: "识别失败。可以重试，或改为手动添加" }
            InkButton { width: parent.width; text: "重试模拟识别"; onClicked: flow.picked() }
            InkButton { width: parent.width; text: "改为手动添加"; outline: true; onClicked: flow.stage = "manual" }
        }
        Column {
            visible: flow.stage === "manual"
            width: parent.width
            spacing: Theme.gapSmall
            TextField { id: manualName; width: parent.width; height: Theme.touch; placeholderText: "食材名称"; background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusRow } }
            TextField { id: manualQuantity; width: parent.width; height: Theme.touch; placeholderText: "数量原值，如 500g"; background: Rectangle { color: Theme.selectedPaper; border.color: Theme.rule; radius: Theme.radiusRow } }
            ComboBox { id: manualLocation; width: parent.width; model: ["冷藏", "冷冻", "常温"] }
            InkButton {
                width: parent.width
                text: "添加模拟食材"
                onClicked: {
                    const name = manualName.text.trim()
                    if (!name) { flow.notice("请先填写食材名称"); return }
                    MockData.addItems([{ name: name, quantity: manualQuantity.text.trim(), category: "other", location: manualLocation.currentText, freshness: "fresh", source: "user", days: "请查看后决定", first: false, purchasedAt: "", storedAt: "", producedAt: "", packageExpiresAt: "", shelfDays: "" }])
                    flow.finished()
                }
            }
        }
        Item {
            visible: flow.stage === "confirm"
            width: parent.width
            height: parent.height - y
            Column {
                anchors.fill: parent
                spacing: Theme.gapSmall
                Text { text: "从模拟截图识别出 15 项 · 核对后入库"; font.pixelSize: Theme.textSubtitle; color: Theme.muted }
                NoticeBanner { width: parent.width; message: "可能有遗漏，请检查" }
                NoticeBanner { visible: MockData.items.length > 0; width: parent.width; height: visible ? implicitHeight : 0; message: "疑似重复导入：请检查冰箱内是否已有同名食材" }
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
                        Text { text: "食材  ·  " + MockData.candidates.filter(function(c) { return c.group === "food" }).length + " 项"; font.family: Theme.serif; font.pixelSize: Theme.textGroup; font.weight: Theme.weightBlack; color: Theme.ink }
                        Repeater {
                            model: MockData.candidates.map(function(c, i) { return Object.assign({}, c, { index: i }) }).filter(function(c) { return c.group === "food" })
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
                                            onEditingFinished: MockData.updateCandidate(ingredient.modelData.index, { name: text.trim() })
                                        }
                                        TextField {
                                            Layout.preferredWidth: 76
                                            text: ingredient.modelData.quantity
                                            font.pixelSize: Theme.textControl
                                            color: Theme.ink
                                            background: Rectangle { radius: Theme.radiusTag; color: Theme.note }
                                            onEditingFinished: MockData.updateCandidate(ingredient.modelData.index, { quantity: text.trim() })
                                        }
                                        ComboBox {
                                            id: location
                                            Layout.preferredWidth: 100
                                            model: ["冷藏", "冷冻", "常温"]
                                            currentIndex: model.indexOf(ingredient.modelData.location)
                                            onActivated: MockData.updateCandidate(ingredient.modelData.index, { location: currentText })
                                        }
                                    }
                                    Text {
                                        text: (ingredient.modelData.source === "package" ? "" : "估计 · ") + Theme.sourceLabel(ingredient.modelData.source) + (ingredient.modelData.shelfDays ? " · 保质 " + ingredient.modelData.shelfDays + " 天" : "")
                                        font.pixelSize: Theme.textMeta
                                        color: ingredient.modelData.source === "package" ? Theme.fresh : Theme.nearing
                                    }
                                    TextField {
                                        width: parent.width
                                        height: 34
                                        text: ingredient.modelData.shelfDays || ""
                                        placeholderText: "保质天数（包装标注可留空）"
                                        inputMethodHints: Qt.ImhDigitsOnly
                                        font.pixelSize: Theme.textMeta
                                        background: Rectangle { radius: Theme.radiusTag; color: Theme.selectedPaper; border.color: Theme.rule }
                                        onEditingFinished: MockData.updateCandidate(ingredient.modelData.index, { shelfDays: text.trim() })
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
                                                        MockData.updateCandidate(ingredient.modelData.index, patch)
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
                                model: MockData.candidates.map(function(c, i) { return Object.assign({}, c, { index: i }) }).filter(function(c) { return c.group === "seasoning" })
                                delegate: Item {
                                    id: seasoningChip
                                    required property var modelData
                                    width: label.implicitWidth + Theme.gapLarge * 2
                                    height: Theme.touch
                                    Rectangle { anchors.fill: parent; anchors.margins: Theme.gapTiny; radius: Theme.radiusRow; color: Theme.selectedPaper; border.color: Theme.ink }
                                    Text { id: label; anchors.centerIn: parent; text: seasoningChip.modelData.name; color: Theme.ink; font.pixelSize: Theme.textControl }
                                    TapHandler { onTapped: MockData.updateCandidate(seasoningChip.modelData.index, { group: "ignored" }) }
                                }
                            }
                        }
                        Rectangle {
                            width: parent.width
                            height: Theme.touch
                            radius: Theme.radiusRow
                            color: Theme.selectedPaper
                            border.color: Theme.outline
                            Text { x: Theme.gap; anchors.verticalCenter: parent.verticalCenter; text: "已忽略 " + MockData.candidates.filter(function(c) { return c.group === "ignored" }).length + " 项 · 点击找回"; font.pixelSize: Theme.textControl; color: Theme.muted }
                            TapHandler { onTapped: flow.ignoredExpanded = !flow.ignoredExpanded }
                        }
                        Repeater {
                            model: flow.ignoredExpanded ? MockData.candidates.map(function(c, i) { return Object.assign({}, c, { index: i }) }).filter(function(c) { return c.group === "ignored" }) : []
                            delegate: InkButton {
                                required property var modelData
                                width: groups.width
                                text: modelData.name + " · 找回为食材"
                                outline: true
                                onClicked: MockData.updateCandidate(modelData.index, { group: "food", category: "other", quantity: "", location: "冷藏", source: "ai", shelfDays: "", purchasedAt: "", storedAt: "", producedAt: "", packageExpiresAt: "" })
                            }
                        }
                    }
                }
                InkButton { width: parent.width; text: "确认入库 · " + MockData.candidates.filter(function(c) { return c.group === "food" }).length + " 项食材"; titleStyle: true; onClicked: { MockData.confirmCandidates(); flow.finished() } }
            }
        }
    }
}
