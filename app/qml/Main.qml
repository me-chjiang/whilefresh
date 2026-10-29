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
    property var tabHistory: []
    property int pageCount: 5
    property double navigationStarted: 0
    property string navigationAction: ""
    readonly property var rootPages: [fridgeComponent, recipesComponent, clearComponent, shoppingComponent, profileComponent]
    readonly property var activeStack: tabRepeater.itemAt(currentTab) ? tabRepeater.itemAt(currentTab).item : null
    width: 412
    height: 915
    visible: true
    title: "趁鲜"
    color: Theme.paper

    function traceNavigation(action) {
        if (!debugTools) return
        navigationStarted = Date.now()
        navigationAction = action
    }
    onFrameSwapped: {
        if (navigationAction) {
            console.debug("WhileFresh navigation: " + navigationAction + " first-frame-ms=" + (Date.now() - navigationStarted))
            navigationAction = ""
        }
    }
    function showTab(index) {
        if (importSheet.opened) importSheet.close()
        if (index === currentTab) return
        traceNavigation("tab=" + index + " cached=" + !!tabRepeater.itemAt(index).item)
        tabHistory = tabHistory.concat([currentTab])
        tabRepeater.itemAt(index).visited = true
        currentTab = index
    }
    function resetPages() {
        tabHistory = []
        currentTab = 0
        pageCount = 0
        pageCount = 5
    }
    function openRecipe(recipe, fromClear) {
        traceNavigation("recipe-detail")
        activeStack.push(detailComponent, {
            recipe: App.recipeDetail(recipe.id),
            selectionContext: fromClear ? { usedSelected: recipe.usedSelected, unusedSelected: recipe.unusedSelected } : ({})
        })
    }
    function showImport() {
        if (importSheet.bodyItem) importSheet.bodyItem.reset()
        importSheet.open()
    }
    function goBack() {
        if (importSheet.opened) { importSheet.close(); return }
        if (activeStack && activeStack.depth > 1) {
            traceNavigation("back-page")
            activeStack.pop()
            return
        }
        if (tabHistory.length > 0) {
            traceNavigation("back-tab")
            currentTab = tabHistory[tabHistory.length - 1]
            tabHistory = tabHistory.slice(0, -1)
        } else close()
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
    StackLayout {
        anchors.top: previewBadge.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.navHeight
        currentIndex: window.currentTab
        Repeater {
            id: tabRepeater
            model: window.pageCount
            delegate: Loader {
                id: tabLoader
                required property int index
                property bool visited: index === 0
                active: visited
                sourceComponent: Component {
                    StackView {
                        initialItem: window.rootPages[tabLoader.index]
                        clip: true
                        pushEnter: null
                        pushExit: null
                        popEnter: null
                        popExit: null
                        replaceEnter: null
                        replaceExit: null
                    }
                }
            }
        }
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
        function onDemoModeChanged() { window.resetPages() }
        function onClearResultsChanged() {
            const loader = tabRepeater.itemAt(2)
            if (!App.clearResultsReady && loader && loader.item && loader.item.depth > 1)
                loader.item.pop(null, StackView.Immediate)
        }
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
            canGoBack: window.tabHistory.length > 0
            onBackRequested: window.goBack()
            onImportRequested: window.showImport()
            onEditRequested: function(item) { window.activeStack.push(editComponent, { ingredient: item }) }
            onNotice: function(message) { window.showNotice(message) }
        }
    }
    Component {
        id: recipesComponent
        RecipesPage {
            canGoBack: window.tabHistory.length > 0
            onBackRequested: window.goBack()
            onRecipeRequested: function(recipe) { window.openRecipe(recipe, false) }
        }
    }
    Component {
        id: clearComponent
        ClearFridgePage {
            canGoBack: window.tabHistory.length > 0
            onBackRequested: window.goBack()
            onResultsRequested: window.activeStack.push(clearResultsComponent)
        }
    }
    Component {
        id: shoppingComponent
        ShoppingPage {
            platform: window.platform
            canGoBack: window.tabHistory.length > 0
            onBackRequested: window.goBack()
            onNotice: function(message) { window.showNotice(message) }
        }
    }
    Component {
        id: profileComponent
        ProfilePage {
            canGoBack: window.tabHistory.length > 0
            onBackRequested: window.goBack()
            debugTools: window.debugTools
            onDebugRequested: window.debugPanelVisible = !window.debugPanelVisible
            onNotice: function(message) { window.showNotice(message) }
        }
    }
    Component {
        id: clearResultsComponent
        ClearResultsPage {
            onBackRequested: window.goBack()
            onRecipeRequested: function(recipe) { window.openRecipe(recipe, true) }
        }
    }
    Component {
        id: detailComponent
        RecipeDetailPage {
            onBackRequested: window.goBack()
            onShoppingRequested: window.showTab(3)
        }
    }
    Component {
        id: editComponent
        ItemEditPage { onBackRequested: window.goBack() }
    }
}
