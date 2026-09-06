import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import lcmp.gui 1.0

ApplicationWindow {
    id: window
    width: 900
    height: 600
    visible: true
    title: qsTr("Litematica Material Comparator")

    readonly property color md3DarkBackground: "#141218"
    readonly property color md3DarkSurfaceVariant: "#211F26"
    readonly property color md3DarkOnSurface: "#E6E1E5"
    readonly property color md3DarkOutline: "#49454F"

    property string basePath: ""
    property string modifiedPath: ""

    background: Rectangle {
        color: window.md3DarkBackground
    }

    LcmpBridge {
        id: bridge
    }

    function triggerComparison() {
        if (basePath !== "" && modifiedPath !== "") {
            let res = bridge.compareFiles(basePath, modifiedPath)
            if (res.success) {
                resultDialog.showResult(res)
            } else {
                errorDialog.errorMessage = res.error
                errorDialog.open()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        DropArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            keys: ["text/uri-list"]

            onDropped: (drop) => {
                if (drop.hasUrls && drop.urls.length > 0) {
                    window.basePath = drop.urls[0]
                    window.triggerComparison()
                }
            }

            Rectangle {
                anchors.fill: parent
                color: "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12

                    Text {
                        text: qsTr("Base Schematic")
                        color: window.md3DarkOnSurface
                        font.pixelSize: 18
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: window.basePath !== "" ? window.basePath : qsTr("Drop original .litematic here")
                        color: window.md3DarkOnSurface
                        opacity: 0.7
                        font.pixelSize: 13
                        wrapMode: Text.WrapAnywhere
                        Layout.maximumWidth: 320
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: window.md3DarkOutline
        }

        DropArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            keys: ["text/uri-list"]

            onDropped: (drop) => {
                if (drop.hasUrls && drop.urls.length > 0) {
                    window.modifiedPath = drop.urls[0]
                    window.triggerComparison()
                }
            }

            Rectangle {
                anchors.fill: parent
                color: "transparent"

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12

                    Text {
                        text: qsTr("Modified Schematic")
                        color: window.md3DarkOnSurface
                        font.pixelSize: 18
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: window.modifiedPath !== "" ? window.modifiedPath : qsTr("Drop modified .litematic here")
                        color: window.md3DarkOnSurface
                        opacity: 0.7
                        font.pixelSize: 13
                        wrapMode: Text.WrapAnywhere
                        Layout.maximumWidth: 320
                        Layout.alignment: Qt.AlignHCenter
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }

    // 结果对比弹窗
    Dialog {
        id: resultDialog
        title: qsTr("Comparison Result")
        anchors.centerIn: parent
        width: 550
        height: 420
        modal: true
        standardButtons: Dialog.Ok

        property var resultData: null

        function showResult(data) {
            resultData = data
            open()
        }

        background: Rectangle {
            color: window.md3DarkSurfaceVariant
            border.color: window.md3DarkOutline
            radius: 12
        }

        header: Text {
            text: resultDialog.title
            color: window.md3DarkOnSurface
            font.pixelSize: 20
            font.bold: true
            padding: 16
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: resultDialog.resultData ?
                      qsTr("Total Delta: %1 blocks (%2 types changed)")
                      .arg(resultDialog.resultData.totalDelta)
                      .arg(resultDialog.resultData.changedTypes) : ""
                color: window.md3DarkOnSurface
                font.pixelSize: 14
                font.bold: true
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: resultDialog.resultData ? resultDialog.resultData.changed : []

                delegate: Rectangle {
                    required property var modelData
                    required property int index

                    width: ListView.view.width
                    height: 36
                    color: index % 2 === 0 ? "transparent" : "#2B2930"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Text {
                            text: parent.parent.modelData.name
                            color: window.md3DarkOnSurface
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        Text {
                            text: `${parent.parent.modelData.baseCount} → ${parent.parent.modelData.newCount}`
                            color: window.md3DarkOnSurface
                            opacity: 0.7
                        }

                        Text {
                            text: (parent.parent.modelData.delta > 0 ? "+" : "") + parent.parent.modelData.delta
                            color: parent.parent.modelData.delta > 0 ? "#81C784" : "#E57373"
                            font.bold: true
                            Layout.preferredWidth: 60
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: errorDialog
        title: qsTr("Parsing Error")
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok
        property string errorMessage: ""

        background: Rectangle {
            color: window.md3DarkSurfaceVariant
            border.color: window.md3DarkOutline
            radius: 12
        }

        header: Text {
            text: errorDialog.title
            color: "#FFB4AB"
            font.pixelSize: 18
            font.bold: true
            padding: 16
        }

        contentItem: Text {
            text: errorDialog.errorMessage
            color: window.md3DarkOnSurface
            wrapMode: Text.Wrap
        }
    }
}