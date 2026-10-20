import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Placeholder shell from T0. Real pages and Theme.qml arrive in T8.
ApplicationWindow {
    id: window

    required property QtObject appInfo

    readonly property var tabTitles: [qsTr("冰箱"), qsTr("菜谱"), qsTr("清冰箱"), qsTr("待买"), qsTr("我的")]
    readonly property int centerTab: 2
    property int currentTab: 0

    width: 390
    height: 844
    visible: true
    title: qsTr("趁鲜")
    color: "#f6f3ec"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: window.tabTitles[window.currentTab]
            font.pixelSize: 28
            color: "#2f3a2f"
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            visible: window.currentTab === 4
            text: qsTr("版本 %1 · %2").arg(window.appInfo.version).arg(window.appInfo.platform)
            color: "#6b6b6b"
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            visible: window.currentTab === 4
            text: window.appInfo.recognitionConfigured ? qsTr("识别服务：已配置") : qsTr("识别服务：未配置")
            color: "#6b6b6b"
        }
    }

    footer: Rectangle {
        implicitHeight: 64
        color: "#ffffff"

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: "#e2ddd2"
        }

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Repeater {
                model: window.tabTitles

                delegate: Item {
                    id: tab

                    required property int index
                    required property string modelData
                    readonly property bool selected: window.currentTab === index

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Label {
                        anchors.centerIn: parent
                        visible: tab.index !== window.centerTab
                        text: tab.modelData
                        font.pixelSize: 14
                        font.bold: tab.selected
                        color: tab.selected ? "#3d7a46" : "#777777"
                    }

                    Rectangle {
                        visible: tab.index === window.centerTab
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.verticalCenter
                        anchors.bottomMargin: -24
                        width: 68
                        height: 68
                        radius: width / 2
                        color: tab.selected ? "#2f6b38" : "#3d7a46"
                        border.color: "#ffffff"
                        border.width: 4

                        Label {
                            anchors.centerIn: parent
                            text: tab.modelData
                            font.pixelSize: 14
                            font.bold: true
                            color: "#ffffff"
                        }
                    }

                    TapHandler {
                        onTapped: window.currentTab = tab.index
                    }
                }
            }
        }
    }
}
