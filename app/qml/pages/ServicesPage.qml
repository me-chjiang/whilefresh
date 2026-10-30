import QtQuick
import QtQuick.Controls.Basic
import WhileFresh

Item {
    id: page
    signal backRequested()
    PageHeader { id: header; x: Theme.gap; width: parent.width - Theme.gap * 2; heading: "服务"; actionText: "返回"; onActionTriggered: page.backRequested() }
    Flickable {
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        contentHeight: content.height + Theme.gapLarge * 2
        Column {
            id: content
            x: Theme.gap
            width: parent.width - Theme.gap * 2
            spacing: Theme.gap
            KeyField { label: "DeepSeek 密钥"; field: "deepseek"; configured: App.settings.deepSeekConfigured }
            KeyField { label: "Lexmount 密钥"; field: "lexmount"; configured: App.settings.lexmountConfigured }
            KeyField { label: "Lexmount 项目 ID"; field: "lexmountProject"; configured: App.settings.lexmountProjectConfigured }
        }
    }
}
