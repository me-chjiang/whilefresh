pragma Singleton
import QtQuick

QtObject {
    // B「厨房手账」: values follow design/README.md. Added category colors below reuse nearby palette hues.
    readonly property color paper: "#F6EBD5"
    readonly property color card: "#FFFBF0"
    readonly property color note: "#FFF3B8"
    readonly property color selectedPaper: "#FFFDF2"
    readonly property color navPaper: "#FFF8E6"
    readonly property color ink: "#3B2D26"
    readonly property color muted: "#8A7767"
    readonly property color mutedTab: "#9C8A76"
    readonly property color noteMuted: "#8A7A44"
    readonly property color rule: "#DFCFAE"
    readonly property color outline: "#BFA97F"
    readonly property color navRule: "#D6C196"
    readonly property color chipOutline: "#C9B77A"
    readonly property color chipMuted: "#9A8B5C"
    readonly property color tomato: "#D5432B"
    readonly property color green: "#3E7F69"
    readonly property color mustard: "#EDBE4C"
    readonly property color none: "#A58C6B"
    readonly property color tapeSage: "#A8D0BF"
    readonly property color tapeBlue: "#A9C9E2"
    readonly property color tapeSand: "#F2CE74"
    readonly property color textOnTomato: "#FFF6E4"
    readonly property color textOnGreen: "#FFF8E6"
    readonly property color mask: "#803B2D26"
    readonly property color shadowColor: "#3C2D23"
    readonly property color fresh: "#2F7A55"
    readonly property color nearing: "#C8581A"
    readonly property color overdue: "#B02A1D"
    readonly property color emptyStar: "#D9C8A6"
    readonly property color vegetable: "#5E9F4B"
    readonly property color fruit: "#E39B53"      // added: warm sand adjacent to staple
    readonly property color meat: "#D8452F"
    readonly property color seafood: "#3F94CF"
    readonly property color eggDairy: "#EFB423"
    readonly property color soy: "#84A65E"        // added: softer green than vegetable
    readonly property color staple: "#BF935A"
    readonly property color seasoning: "#B28A62"  // added: warm brown
    readonly property color processed: "#C17A6A"  // added: muted tomato
    readonly property color other: "#9E9988"      // added: quiet neutral

    function categoryColor(code) {
        const colors = { vegetable: vegetable, fruit: fruit, meat: meat,
                         seafood: seafood, egg_dairy: eggDairy, soy: soy,
                         staple: staple, seasoning: seasoning, processed: processed, other: other }
        return colors[code] || other
    }
    function freshnessColor(status) {
        return status === "overdue" ? overdue : status === "nearing" ? nearing : fresh
    }
    function freshnessLabel(status) {
        return status === "overdue" ? "超出估计期限" : status === "nearing" ? "临期" : "新鲜"
    }
    function sourceLabel(source) {
        return ({ package: "包装标注", user: "你的设置", reference: "参考表", ai: "AI 估计" })[source] || "参考表"
    }

    property FontLoader titleFace: FontLoader {
        source: Qt.resolvedUrl("../assets/fonts/NotoSerifSC-Black-subset.otf")
    }
    readonly property string serif: titleFace.name || "Songti SC"

    readonly property int textTitle: 26
    readonly property int textSheetTitle: 23
    readonly property int textRecipe: 18
    readonly property int textPanel: 17
    readonly property int textGroup: 16
    readonly property int textTape: 15
    readonly property int textBody: 17
    readonly property int textQuantity: 15
    readonly property int textControl: 14
    readonly property int textSubtitle: 13
    readonly property int textMeta: 12
    readonly property int textSmall: 11
    readonly property int weightNormal: 400
    readonly property int weightBold: 700
    readonly property int weightStrong: 800
    readonly property int weightBlack: 900

    readonly property int radiusCard: 8
    readonly property int radiusRow: 6
    readonly property int radiusTag: 4
    readonly property real shadowOneOpacity: 0.14
    readonly property real shadowOneBlur: 0.18
    readonly property real shadowTwoOpacity: 0.20
    readonly property real shadowTwoBlur: 0.30
    readonly property int shadowOneOffset: 1
    readonly property int shadowTwoOffset: 4
    readonly property int gapTiny: 4
    readonly property int gapSmall: 8
    readonly property int gap: 12
    readonly property int gapLarge: 20
    readonly property int touch: 48
    readonly property int topHeight: 58
    readonly property int navHeight: 80
    readonly property int sectionGap: 20
    readonly property int sectionPadding: 12
    readonly property int rowHeight: 54
    readonly property int itemDot: 17
    readonly property int stampHeight: 22
    readonly property int badgeHeight: 19
    readonly property int tapeHeight: 26
    readonly property int recipeHeight: 68
    readonly property int stampDiameter: 84
}
