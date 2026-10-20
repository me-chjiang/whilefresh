import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Debug builds only: exercises PlatformServices until the real import screens (T8) exist.
ColumnLayout {
    id: panel

    required property QtObject platform

    readonly property var originNames: ["gallery", "share", "camera"]
    readonly property string sampleText: qsTr("待买清单（调试）\n蔬菜：西红柿、葱\n蛋奶：鸡蛋")
    property string lastEvent: qsTr("尚未收到导入")

    spacing: 8

    Label {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("调试：平台能力")
        font.bold: true
        color: "#2f3a2f"
    }

    GridLayout {
        Layout.alignment: Qt.AlignHCenter
        columns: 2

        Button {
            text: qsTr("选图")
            onClicked: panel.platform.pickImages()
        }
        Button {
            text: qsTr("拍小票")
            onClicked: panel.platform.captureReceipt()
        }
        Button {
            text: qsTr("分享测试文字")
            onClicked: panel.platform.shareText(panel.sampleText)
        }
        Button {
            text: qsTr("复制测试文字")
            onClicked: {
                panel.platform.copyText(panel.sampleText)
                panel.lastEvent = qsTr("已复制测试文字")
            }
        }
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 340
        text: panel.lastEvent
        wrapMode: Text.WrapAnywhere
        font.pixelSize: 12
        color: "#444444"
    }

    Connections {
        target: panel.platform

        function onImportReceived(payload) {
            panel.lastEvent = payload.summary
        }
        function onImportCancelled(origin) {
            panel.lastEvent = qsTr("已取消 · %1").arg(panel.originNames[origin])
        }
        function onImportFailed(origin, message) {
            panel.lastEvent = qsTr("失败 · %1：%2").arg(panel.originNames[origin]).arg(message)
        }
    }
}
