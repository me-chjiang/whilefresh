import QtQuick
import QtQuick.Layouts
import WhileFresh

Rectangle {
    id: banner
    property string message: "可能有遗漏，请检查"
    property bool warning: true
    property string actionText: ""
    signal actionTriggered()
    implicitHeight: Math.max(Theme.touch, note.implicitHeight + Theme.gapLarge)
    radius: Theme.radiusRow
    color: warning ? Theme.note : Theme.tapeSage
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.gap
        anchors.rightMargin: Theme.gapSmall
        spacing: Theme.gapSmall
        IconGlyph { name: banner.warning ? "flag-2" : "check"; tint: Theme.ink; width: 20; height: 20 }
        Text { id: note; text: banner.message; Layout.fillWidth: true; font.pixelSize: Theme.textSubtitle; color: Theme.ink; wrapMode: Text.Wrap }
        Text { visible: banner.actionText !== ""; text: banner.actionText; font.pixelSize: Theme.textSubtitle; font.weight: Theme.weightBold; color: Theme.tomato }
    }
    TapHandler { enabled: banner.actionText !== ""; onTapped: banner.actionTriggered() }
}
