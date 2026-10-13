// Main.qml — Veltrix Studio main window (interface lives ONLY under
// Interface/ per project rules; this is the single entry QML file).
//
// Layout: media open button, position scrubber, decode button, preview
// image area, status/engine labels. Everything drives previewController
// (C++ bridge); zero decode logic in QML.

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    title: qsTr("Veltrix Studio")
    width: 1100
    height: 700
    visible: true

    Material.theme: Material.Dark
    Material.primary: "#3F51B5"
    Material.accent: "#FF4081"

    // The decoded frame; bumped when frameReady fires.
    property string frameUrl: ""

    Connections {
        function onFrameReady(frame, ptsTicks) {
            // QML cannot hold a QImage directly: the image provider serves
            // it; changing the cache-buster URL forces a reload.
            root.frameUrl = "image://veltrix-frame/latest?v=" + ptsTicks
        }
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            MenuItem {
                text: qsTr("&Open media…")
                onTriggered: fileDialog.open()
            }
            MenuItem {
                text: qsTr("E&xit")
                onTriggered: Qt.quit()
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Open a media file")
        nameFilters: [qsTr("Media files (*.mp4 *.mov *.mkv *.avi *.m4a *.mp3 *.wav)"),
                     qsTr("All files (*)")]
        onAccepted: previewController.openMedia(selectedFile.toString().replace("file:///", ""))
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        // ---- preview area ------------------------------------------------
        Rectangle {
            id: previewArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#101014"
            radius: 8
            border.color: "#2A2A30"
            border.width: 1

            Image {
                id: previewImage
                anchors.centerIn: parent
                source: root.frameUrl
                fillMode: Image.PreserveAspectFit
                anchors.margins: 16
                width: parent.width - 32
                height: parent.height - 32
                cache: false
                visible: root.frameUrl !== ""
            }

            Label {
                anchors.centerIn: parent
                visible: root.frameUrl === ""
                text: previewController.mediaPath === ""
                      ? qsTr("Open a media file to begin (File → Open media…)")
                      : qsTr("Use the position slider and Decode button")
                color: "#6A6A72"
                font.pixelSize: 15
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: previewController.decoding
                visible: previewController.decoding
            }
        }

        // ---- position + decode --------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label { text: qsTr("Position (s):") }
            Slider {
                id: positionSlider
                Layout.fillWidth: true
                from: 0.0
                to: 10.0
                value: 0.0
                stepSize: 0.05
                enabled: previewController.mediaPath !== ""
            }
            Label {
                text: positionSlider.value.toFixed(2) + " s"
                font.family: "monospace"
                Layout.preferredWidth: 70
            }
            Button {
                text: qsTr("Decode frame")
                enabled: previewController.mediaPath !== "" && !previewController.decoding
                onClicked: previewController.requestFrameAt(positionSlider.value)
                Material.background: Material.primary
                Material.foreground: "#FFFFFF"
            }
        }

        // ---- status bar -----------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Label {
                text: previewController.mediaPath === ""
                      ? ""
                      : qsTr("Media: ") + previewController.mediaPath
                elide: Text.ElideMiddle
                Layout.fillWidth: true
                color: "#9E9EA6"
            }
            Label {
                text: qsTr("Engine: ") + previewController.engineName
                color: "#6A6A72"
                font.pixelSize: 11
            }
            Label {
                text: previewController.statusMessage
                color: "#FF4081"
                font.pixelSize: 11
            }
        }
    }
}
