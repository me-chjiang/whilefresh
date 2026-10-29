import QtQuick
import WhileFresh

Item {
    id: page
    readonly property var uncovered: App.clearResultsSummary.uncovered || []
    signal backRequested()
    signal recipeRequested(var recipe)

    PageHeader {
        id: header
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        heading: "清冰箱结果"
        backVisible: true
        onBackRequested: page.backRequested()
    }
    Column {
        id: summary
        x: Theme.gap
        y: header.height + Theme.gap
        width: parent.width - Theme.gap * 2
        spacing: Theme.gapSmall
        Text {
            width: parent.width
            text: "清理目标 " + App.clearResultsSummary.selectedCount + " 种食材 · " + App.clearResultsSummary.candidateCount + " 道候选菜"
            wrapMode: Text.Wrap
            font.family: Theme.serif
            font.pixelSize: Theme.textPanel
            font.weight: Theme.weightBlack
            color: Theme.ink
        }
        Text {
            width: parent.width
            text: App.clearResultsSummary.completeCount > 0
                  ? App.clearResultsSummary.completeCount + " 道菜用到全部已选食材"
                  : App.clearResultsSummary.candidateCount > 0
                    ? "没有菜谱能用到全部已选食材，可从下面的候选菜挑选"
                    : "这组食材暂时没有对应菜谱"
            wrapMode: Text.Wrap
            font.pixelSize: Theme.textSubtitle
            color: App.clearResultsSummary.completeCount > 0 ? Theme.fresh : Theme.nearing
        }
        Text {
            visible: page.uncovered.length > 0
            width: parent.width
            text: "暂未覆盖：" + page.uncovered.join("、")
            wrapMode: Text.Wrap
            font.pixelSize: Theme.textMeta
            color: Theme.muted
        }
    }
    EmptyState {
        visible: App.clearResultsReady && App.clearRecipes.length === 0
        anchors.top: summary.bottom
        anchors.topMargin: Theme.gapLarge
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        iconName: "chef-hat"
        heading: "没有找到可用菜谱"
        detail: "返回选料页调整食材后再试"
    }
    ListView {
        id: results
        anchors.top: summary.bottom
        anchors.topMargin: Theme.gapLarge
        anchors.bottom: parent.bottom
        x: Theme.gap
        width: parent.width - Theme.gap * 2
        visible: App.clearResultsReady && App.clearRecipes.length > 0
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: Theme.gapSmall
        reuseItems: true
        model: App.clearRecipes
        delegate: RecipeCard {
            required property var modelData
            width: results.width
            recipe: modelData
            onOpened: function(recipe) { page.recipeRequested(recipe) }
        }
    }
}
