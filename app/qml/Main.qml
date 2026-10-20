import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import WhileFresh

ApplicationWindow {
    id: window
    required property QtObject appInfo
    required property QtObject platform
    required property bool debugTools
    property int currentTab: 0
    property bool debugPanelVisible: false
    width: 412
    height: 915
    visible: true
    title: "趁鲜 · 演示界面"
    color: Theme.paper

    function showTab(index) {
        if (importSheet.opened) importSheet.close()
        currentTab = index
        stack.clear(StackView.Immediate)
        stack.push([fridgeComponent, recipesComponent, clearComponent, shoppingComponent, profileComponent][index], {}, StackView.Immediate)
    }
    function showImport(shared) {
        importSheet.open()
        if (shared) Qt.callLater(function() { if (importSheet.bodyItem) importSheet.bodyItem.sharedImport() })
        else if (importSheet.bodyItem) importSheet.bodyItem.stage = "choose"
    }
    function goBack() {
        if (importSheet.opened) { importSheet.close(); return }
        if (stack.depth > 1) { stack.pop(); return }
        if (currentTab !== 0) showTab(0)
        else close()
    }

    PaperBackground { anchors.fill: parent }
    Rectangle {
        id: previewBadge
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 26
        color: Theme.tapeSage
        Text {
            anchors.centerIn: parent
            text: MockData.previewLabel
            color: Theme.ink
            font.pixelSize: Theme.textMeta
            font.weight: Theme.weightBold
        }
    }
    StackView {
        id: stack
        anchors.top: previewBadge.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.navHeight
        initialItem: fridgeComponent
        clip: true
        pushEnter: null
        pushExit: null
        popEnter: null
        popExit: null
        replaceEnter: null
        replaceExit: null
    }
    BottomNav {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        currentTab: window.currentTab
        nearCount: MockData.items.filter(function(item) { return item.freshness === "nearing" }).length
        onTabRequested: function(index) { window.showTab(index) }
    }
    BottomSheet {
        id: importSheet
        heading: "确认入库"
        tapeText: "识别结果"
        sheetHeight: 744
        bodyComponent: ImportFlowPage {
            platform: window.platform
            onFinished: { importSheet.close(); window.showTab(0); window.showNotice("已加入模拟食材") }
            onNotice: function(message) { window.showNotice(message) }
        }
    }
    Rectangle {
        id: toast
        visible: toastTimer.running
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.navHeight + Theme.gap
        width: Math.min(parent.width - Theme.gapLarge * 2, toastText.implicitWidth + Theme.gapLarge * 2)
        height: Theme.touch
        radius: Theme.radiusRow
        color: Theme.ink
        z: 100
        Text { id: toastText; anchors.centerIn: parent; color: Theme.textOnTomato; font.pixelSize: Theme.textSubtitle }
    }
    Timer { id: toastTimer; interval: 2600 }
    function showNotice(message) { toastText.text = message; toastTimer.restart() }
    Loader {
        id: debugLoader
        active: window.debugTools
        visible: window.currentTab === 4 && window.debugPanelVisible
        anchors.centerIn: parent
        z: 60
        sourceComponent: ImportDebugPanel { platform: window.platform }
    }
    Connections {
        target: window.platform
        function onImportReceived(payload) {
            if (!importSheet.opened) window.showImport(true)
            else if (importSheet.bodyItem) importSheet.bodyItem.picked()
        }
        function onImportCancelled(origin) {
            if (importSheet.bodyItem) importSheet.bodyItem.cancelled()
        }
        function onImportFailed(origin, message) {
            if (importSheet.bodyItem) importSheet.bodyItem.stage = "failed"
            window.showNotice(message)
        }
    }
    Shortcut { sequences: ["Back", "Escape"]; onActivated: window.goBack() }
    Component {
        id: fridgeComponent
        FridgePage {
            onImportRequested: window.showImport(false)
            onEditRequested: function(item) { stack.push(editComponent, { ingredient: item }) }
            onNotice: function(message) { window.showNotice(message) }
        }
    }
    Component {
        id: recipesComponent
        RecipesPage { onRecipeRequested: function(recipe) { stack.push(detailComponent, { recipe: recipe }) } }
    }
    Component {
        id: clearComponent
        ClearFridgePage { onRecipeRequested: function(recipe) { stack.push(detailComponent, { recipe: recipe }) } }
    }
    Component {
        id: shoppingComponent
        ShoppingPage { platform: window.platform; onNotice: function(message) { window.showNotice(message) } }
    }
    Component {
        id: profileComponent
        ProfilePage {
            debugTools: window.debugTools
            onDebugRequested: window.debugPanelVisible = !window.debugPanelVisible
            onNotice: function(message) { window.showNotice(message) }
        }
    }
    Component {
        id: detailComponent
        RecipeDetailPage {
            onBackRequested: stack.pop()
            onShoppingRequested: window.showTab(3)
        }
    }
    Component {
        id: editComponent
        ItemEditPage { onBackRequested: stack.pop() }
    }
}
