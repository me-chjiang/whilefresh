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
    title: "趁鲜"
    color: Theme.paper

    function showTab(index) {
        if (importSheet.opened) importSheet.close()
        currentTab = index
        stack.clear(StackView.Immediate)
        stack.push([fridgeComponent, recipesComponent, clearComponent, shoppingComponent, profileComponent][index], {}, StackView.Immediate)
    }
    function showImport() {
        if (importSheet.bodyItem) importSheet.bodyItem.reset()
        importSheet.open()
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
        height: App.demoMode ? 26 : 0
        visible: App.demoMode
        color: Theme.tapeSage
        Text {
            anchors.centerIn: parent
            text: "演示模式 · 示例数据，识别为预设结果，不联网"
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
        nearCount: App.nearCount
        onTabRequested: function(index) { window.showTab(index) }
    }
    BottomSheet {
        id: importSheet
        heading: App.importState === "draft" ? "确认入库" : "入库"
        tapeText: "识别结果"
        sheetHeight: 744
        onClosed: if (App.importState !== "idle") App.cancelImport()
        bodyComponent: ImportFlowPage {
            onFinished: { importSheet.close(); window.showTab(0); window.showNotice("已加入冰箱") }
            onDismissed: importSheet.close()
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
        target: App
        function onNotice(message) { window.showNotice(message) }
        function onPickCancelled() { if (importSheet.bodyItem) importSheet.bodyItem.cancelled() }
        function onImportChanged() { if (App.importState !== "idle" && !importSheet.opened) importSheet.open() }
    }
    Component.onCompleted: {
        if (App.startupNotice) showNotice(App.startupNotice)
        else if (!App.recognitionConfigured && !App.demoMode) showNotice("未配置识别服务密钥，可在「我的」开启演示模式")
    }
    Shortcut { sequences: ["Back", "Escape"]; onActivated: window.goBack() }
    Component {
        id: fridgeComponent
        FridgePage {
            onImportRequested: window.showImport()
            onEditRequested: function(item) { stack.push(editComponent, { ingredient: item }) }
            onNotice: function(message) { window.showNotice(message) }
        }
    }
    Component {
        id: recipesComponent
        RecipesPage {
            onWebRecipeRequested: function(id) { App.requestRecipeDetail(id); stack.push(webDetailComponent) }
        }
    }
    Component {
        id: clearComponent
        ClearFridgePage { onSearchRequested: function(keyword) { App.searchRecipes(keyword); window.showTab(1) } }
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
            onServicesRequested: stack.push(servicesComponent)
            onPreferencesRequested: stack.push(preferencesComponent)
            onNotice: function(message) { window.showNotice(message) }
        }
    }
    Component {
        id: servicesComponent
        ServicesPage { onBackRequested: stack.pop() }
    }
    Component {
        id: preferencesComponent
        PreferencesPage { onBackRequested: stack.pop() }
    }
    Component {
        id: webDetailComponent
        WebRecipeDetailPage { onBackRequested: { App.cancelRecipeRequest(); stack.pop() } }
    }
    Component {
        id: editComponent
        ItemEditPage { onBackRequested: stack.pop() }
    }
}
