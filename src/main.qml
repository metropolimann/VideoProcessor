import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    visible: true
    width: 640
    height: 480
    title: qsTr("Video Processor GUI")

    // Center the content
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 20

        Text {
            text: "Welcome to Video Processor"
            font.pixelSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            text: "Extract Frames"
            Layout.alignment: Qt.AlignHCenter
            onClicked: {
                console.log("Button clicked! We will call C++ here next.")
                // Later, we will call your C++ function here
            }
        }
        
        Text {
            text: "Status: Ready"
            Layout.alignment: Qt.AlignHCenter
        }
    }
}