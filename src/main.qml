import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    visible: true
    width: 900
    height: 700
    title: qsTr("Video Processor")

    FileDialog {
        id: videoFileDialog
        title: "Select Video File"
        nameFilters: ["Video files (*.mp4 *.avi *.mkv *.mov)", "All files (*)"]
        onAccepted: videoPathField.text = selectedFile.toString().replace("file://", "")
    }

    FileDialog {
        id: outputDirDialog
        title: "Select Output Directory"
        selectFolder: true
        onAccepted: outputDirField.text = selectedFile.toString().replace("file://", "")
    }

    ScrollView {
        anchors.fill: parent

        ColumnLayout {
            anchors.margins: 20
            spacing: 15

            Text {
                text: "Video Processor"
                font.pixelSize: 28
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            // Video input
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Video File:"; Layout.preferredWidth: 120 }
                TextField {
                    id: videoPathField
                    Layout.fillWidth: true
                    placeholderText: "Select a video file..."
                }
                Button { text: "Browse..."; onClicked: videoFileDialog.open() }
            }

            // Output directory
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Output Dir:"; Layout.preferredWidth: 120 }
                TextField {
                    id: outputDirField
                    Layout.fillWidth: true
                    placeholderText: "Select output directory..."
                }
                Button { text: "Browse..."; onClicked: outputDirDialog.open() }
            }

            // Time range
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Start Time:"; Layout.preferredWidth: 120 }
                TextField {
                    id: startTimeField
                    Layout.fillWidth: true
                    placeholderText: "HH:MM:SS (e.g., 00:01:00)"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "End Time:"; Layout.preferredWidth: 120 }
                TextField {
                    id: endTimeField
                    Layout.fillWidth: true
                    placeholderText: "HH:MM:SS (e.g., 00:02:00)"
                }
            }

            // FPS
            RowLayout {
                Layout.fillWidth: true
                Text { text: "FPS:"; Layout.preferredWidth: 120 }
                SpinBox {
                    id: fpsSpinBox
                    from: 1
                    to: 120
                    value: 5
                    Layout.fillWidth: true
                }
            }

            // Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    text: "Extract Frames"
                    Layout.fillWidth: true
                    onClicked: {
                        backend.extractFrames(videoPathField.text, outputDirField.text,
                                              fpsSpinBox.value, startTimeField.text,
                                              endTimeField.text)
                    }
                }

                Button {
                    text: "Fire Crop"
                    Layout.fillWidth: true
                    onClicked: {
                        backend.extractFireCrop(videoPathField.text, outputDirField.text,
                                                fpsSpinBox.value, startTimeField.text,
                                                endTimeField.text)
                    }
                }
            }

            // Variance analysis section
            GroupBox {
                title: "Variance Analysis"
                Layout.fillWidth: true

                ColumnLayout {
                    RowLayout {
                        Text { text: "Samples (n):"; Layout.preferredWidth: 120 }
                        SpinBox { id: nSpinBox; from: 1; to: 1000; value: 10; Layout.fillWidth: true }
                    }
                    RowLayout {
                        Text { text: "Frame Distance (m):"; Layout.preferredWidth: 120 }
                        SpinBox { id: mSpinBox; from: 0; to: 100; value: 5; Layout.fillWidth: true }
                    }
                    Button {
                        text: "Analyze Variance"
                        Layout.fillWidth: true
                        onClicked: {
                            backend.analyzeVariance(videoPathField.text, startTimeField.text,
                                                     endTimeField.text, nSpinBox.value,
                                                     mSpinBox.value)
                        }
                    }
                }
            }

            // Status
            TextArea {
                id: statusArea
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                readOnly: true
                text: backend.status
                wrapMode: TextArea.Wrap
            }
        }
    }
}