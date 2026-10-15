import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Window

ApplicationWindow {
    id: root
    visible: true
    
    width: Screen.width * 0.35
    height: Screen.height * 0.60
    minimumWidth: Screen.width * 0.35
    maximumWidth: Screen.width * 0.35
    minimumHeight: Screen.height * 0.60
    maximumHeight: Screen.height * 0.60
    
    title: qsTr("Video Processor")
    color: "#1e1e1e"

    MessageDialog {
        id: msgDialog
        title: "Notification"
        text: ""
        buttons: MessageDialog.Ok
    }

    Connections {
        target: backend
        function onOperationFinished(success, message) {
            msgDialog.text = message
            msgDialog.title = success ? "Success" : "Error"
            msgDialog.open()
        }
    }

    FileDialog {
        id: videoFileDialog
        title: "Select Video File"
        nameFilters: ["Video files (*.mp4 *.avi *.mkv *.mov)", "All files (*)"]
        options: FileDialog.DontUseNativeDialog
        fileMode: FileDialog.OpenFile
        onAccepted: videoPathField.text = selectedFile.toString().replace("file://", "")
    }

    FileDialog {
        id: outputDirDialog
        title: "Select Output Directory"
        options: FileDialog.DontUseNativeDialog | FileDialog.ShowDirsOnly | FileDialog.CreateDirectory
        fileMode: FileDialog.Directory
        onAccepted: outputDirField.text = selectedFile.toString().replace("file://", "")
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: parent.width
        clip: true

        ColumnLayout {
            width: parent.width - 30
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 15
            spacing: 15

            Text {
                text: "Video Processor"
                font.pixelSize: 24
                font.weight: Font.Bold
                color: "#ffffff"
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "Extract frames, crop fire regions, and analyze variance."
                font.pixelSize: 11
                font.weight: Font.Medium
                color: "#b0b0b0"
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 5
            }

            // --- INPUT SECTION ---
            GroupBox {
                title: "Input Settings"
                Layout.fillWidth: true
                Layout.leftMargin: 5
                Layout.rightMargin: 5
                Layout.topMargin: 5
                Layout.bottomMargin: 5
                label: Text {
                    text: parent.title
                    color: "#3498db"
                    font.weight: Font.Bold
                    font.pixelSize: 13
                    leftPadding: 8 
                }
                background: Rectangle {
                    color: "#252526"
                    border.color: "#3e3e42"
                    border.width: 1
                    radius: 8
                }

                GridLayout {
                    columns: 3
                    columnSpacing: 12
                    rowSpacing: 12
                    anchors.fill: parent
                    anchors.margins: 12

                    Text { text: "Video File:"; font.weight: Font.Bold; color: "#e0e0e0"; font.pixelSize: 11 }
                    TextField {
                        id: videoPathField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 31
                        placeholderText: "Select video..."
                        placeholderTextColor: "#888888"
                        font.pixelSize: 10
                        color: "#ffffff"
                        background: Rectangle {
                            color: "#3c3c3c"; border.color: "#555"; border.width: 1; radius: 5
                        }
                    }
                    Button { 
                        text: "Browse"
                        Layout.preferredWidth: 75
                        Layout.preferredHeight: 31
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        background: Rectangle {
                            color: parent.hovered ? "#4a9eff" : "#3498db"
                            border.color: "#2980b9"; border.width: 1; radius: 5
                        }
                        contentItem: Text {
                            text: parent.text; color: "#ffffff"
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            font.weight: Font.Bold
                        }
                        onClicked: videoFileDialog.open()
                    }

                    Text { text: "Output Dir:"; font.weight: Font.Bold; color: "#e0e0e0"; font.pixelSize: 11 }
                    TextField {
                        id: outputDirField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 31
                        placeholderText: "Select output..."
                        placeholderTextColor: "#888888"
                        font.pixelSize: 10
                        color: "#ffffff"
                        background: Rectangle {
                            color: "#3c3c3c"; border.color: "#555"; border.width: 1; radius: 5
                        }
                    }                    
                    Button { 
                        text: "Browse"
                        Layout.preferredWidth: 75
                        Layout.preferredHeight: 31
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        background: Rectangle {
                            color: parent.hovered ? "#4a9eff" : "#3498db"
                            border.color: "#2980b9"; border.width: 1; radius: 5
                        }
                        contentItem: Text {
                            text: parent.text; color: "#ffffff"
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            font.weight: Font.Bold
                        }
                        onClicked: outputDirDialog.open()
                    }

                    Text { text: "Start Time:"; font.weight: Font.Bold; color: "#e0e0e0"; font.pixelSize: 11 }
                    TextField {
                        id: startTimeField
                        Layout.preferredWidth: 85
                        Layout.preferredHeight: 31
                        inputMask: "99:99:99;_"
                        font.pixelSize: 10
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        background: Rectangle {
                            color: "#3c3c3c"; border.color: "#555"; border.width: 1; radius: 5
                        }
                    }
                    Item { Layout.fillWidth: true }

                    Text { text: "End Time:"; font.weight: Font.Bold; color: "#e0e0e0"; font.pixelSize: 11 }
                    TextField {
                        id: endTimeField
                        Layout.preferredWidth: 85
                        Layout.preferredHeight: 31
                        inputMask: "99:99:99;_"
                        font.pixelSize: 10
                        color: "#ffffff"
                        horizontalAlignment: Text.AlignHCenter
                        background: Rectangle {
                            color: "#3c3c3c"; border.color: "#555"; border.width: 1; radius: 5
                        }
                    }
                    Item { Layout.fillWidth: true }

                    Text { 
                        text: "FPS:"
                        font.weight: Font.Bold; color: "#e0e0e0"; font.pixelSize: 11
                        Layout.bottomMargin: 10
                    }
                    SpinBox {
                        id: fpsSpinBox
                        from: 1; to: 120; value: 5
                        Layout.preferredWidth: 85
                        Layout.preferredHeight: 31
                        Layout.bottomMargin: 10
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        editable: true
                        background: Rectangle {
                            color: "#3c3c3c"; border.color: "#555"; border.width: 1; radius: 5
                        }
                        contentItem: TextInput {
                            text: fpsSpinBox.displayText
                            font.pixelSize: 10; font.weight: Font.Bold; color: "#ffffff"
                            horizontalAlignment: Qt.AlignHCenter; verticalAlignment: Qt.AlignVCenter
                            readOnly: true; validator: fpsSpinBox.validator
                        }
                    }
                    Item { Layout.fillWidth: true; Layout.bottomMargin: 10 }
                }
            }

            // --- ACTION BUTTONS (INCREASED HEIGHT & WIDTH) ---
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 5
                Layout.rightMargin: 5
                spacing: 15

                Button {
                    text: "Extract Frames"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50 // <-- INCREASED HEIGHT
                    font.pixelSize: 14 // <-- INCREASED FONT
                    font.weight: Font.Bold
                    background: Rectangle {
                        color: parent.hovered ? "#2980b9" : "#3498db"
                        border.color: "#2574a9"; border.width: 1; radius: 6
                    }
                    contentItem: Text {
                        text: parent.text; color: "white"
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        font.weight: Font.Bold
                    }
                    onClicked: {
                        backend.extractFrames(videoPathField.text, outputDirField.text,
                                              fpsSpinBox.value, startTimeField.text,
                                              endTimeField.text)
                    }
                }

                Button {
                    text: "Fire Crop"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 50 // <-- INCREASED HEIGHT
                    font.pixelSize: 14 // <-- INCREASED FONT
                    font.weight: Font.Bold
                    background: Rectangle {
                        color: parent.hovered ? "#c0392b" : "#e74c3c"
                        border.color: "#a93226"; border.width: 1; radius: 6
                    }
                    contentItem: Text {
                        text: parent.text; color: "white"
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        font.weight: Font.Bold
                    }
                    onClicked: {
                        backend.extractFireCrop(videoPathField.text, outputDirField.text,
                                                fpsSpinBox.value, startTimeField.text,
                                                endTimeField.text)
                    }
                }
            }

            // --- VARIANCE ANALYSIS SECTION (CENTERED BUTTON) ---
            GroupBox {
                title: "Variance Analysis"
                Layout.fillWidth: true
                Layout.leftMargin: 5
                Layout.rightMargin: 5
                Layout.topMargin: 5
                Layout.bottomMargin: 5
                label: Text {
                    text: parent.title
                    color: "#2ecc71"
                    font.weight: Font.Bold
                    font.pixelSize: 13
                    leftPadding: 8
                }
                background: Rectangle {
                    color: "#252526"; border.color: "#3e3e42"; border.width: 1; radius: 8
                }

                GridLayout {
                    columns: 3
                    columnSpacing: 12
                    rowSpacing: 12
                    anchors.fill: parent
                    anchors.margins: 12

                    Text { text: "Samples (n):"; font.weight: Font.Bold; color: "#e0e0e0"; font.pixelSize: 11 }
                    SpinBox { 
                        id: nSpinBox
                        from: 1; to: 1000; value: 10
                        Layout.preferredWidth: 85
                        Layout.preferredHeight: 31
                        font.pixelSize: 10; font.weight: Font.Bold; editable: true
                        background: Rectangle {
                            color: "#3c3c3c"; border.color: "#555"; border.width: 1; radius: 5
                        }
                        contentItem: TextInput {
                            text: nSpinBox.displayText
                            font.pixelSize: 10; font.weight: Font.Bold; color: "#ffffff"
                            horizontalAlignment: Qt.AlignHCenter; verticalAlignment: Qt.AlignVCenter
                            readOnly: true; validator: nSpinBox.validator
                        }
                    }
                    Item { Layout.fillWidth: true }

                    Text { text: "Frame Dist. (m):"; font.weight: Font.Bold; color: "#e0e0e0"; font.pixelSize: 11 }
                    SpinBox { 
                        id: mSpinBox
                        from: 0; to: 100; value: 5
                        Layout.preferredWidth: 85
                        Layout.preferredHeight: 31
                        font.pixelSize: 10; font.weight: Font.Bold; editable: true
                        background: Rectangle {
                            color: "#3c3c3c"; border.color: "#555"; border.width: 1; radius: 5
                        }
                        contentItem: TextInput {
                            text: mSpinBox.displayText
                            font.pixelSize: 10; font.weight: Font.Bold; color: "#ffffff"
                            horizontalAlignment: Qt.AlignHCenter; verticalAlignment: Qt.AlignVCenter
                            readOnly: true; validator: mSpinBox.validator
                        }
                    }
                    Item { Layout.fillWidth: true }

                    // Centered Analyze Button
                    Item { Layout.columnSpan: 3; Layout.fillWidth: true; Layout.preferredHeight: 10 } // Spacer
                    Button {
                        text: "Analyze Variance"
                        Layout.columnSpan: 3
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 250 // Fixed width for a nice centered look
                        Layout.preferredHeight: 40
                        font.pixelSize: 12
                        font.weight: Font.Bold
                        background: Rectangle {
                            color: parent.hovered ? "#27ae60" : "#2ecc71"
                            border.color: "#26a65b"; border.width: 1; radius: 6
                        }
                        contentItem: Text {
                            text: parent.text; color: "white"
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            font.weight: Font.Bold
                        }
                        onClicked: {
                            backend.analyzeVariance(videoPathField.text, startTimeField.text,
                                                     endTimeField.text, nSpinBox.value,
                                                     mSpinBox.value)
                        }
                    }
                    Item { Layout.columnSpan: 3; Layout.fillWidth: true; Layout.preferredHeight: 10 } // Spacer
                }
            }

            // --- STATUS OUTPUT CONSOLE (FIXED HEIGHT) ---
            GroupBox {
                title: "Console Output"
                Layout.fillWidth: true
                Layout.preferredHeight: 150 // <-- FIXED HEIGHT SO IT DOESN'T COLLAPSE
                Layout.leftMargin: 5
                Layout.rightMargin: 5
                Layout.topMargin: 5
                Layout.bottomMargin: 5
                label: Text {
                    text: parent.title
                    color: "#f39c12"
                    font.weight: Font.Bold
                    font.pixelSize: 13
                    leftPadding: 8
                }
                background: Rectangle {
                    color: "#252526"; border.color: "#3e3e42"; border.width: 1; radius: 8
                }

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 12
                    clip: true

                    TextArea {
                        id: statusArea
                        readOnly: true
                        text: backend.status
                        wrapMode: TextArea.Wrap
                        font.family: "monospace"
                        font.pixelSize: 10
                        font.weight: Font.Normal
                        color: "#ecf0f1"
                        background: Rectangle {
                            color: "#1e1e1e"; radius: 5; border.color: "#333"; border.width: 1
                        }
                        padding: 8
                    }
                }
            }
        }
    }
}
