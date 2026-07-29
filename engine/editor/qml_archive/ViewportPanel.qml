import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: viewportRoot
    color: "#000000"
    
    property bool engineInitialized: false
    property alias engineBridge: engineBridgeConnection
    
    // Engine status overlay
    Rectangle {
        id: statusOverlay
        anchors.fill: parent
        color: "#000000"
        opacity: engineInitialized ? 0 : 1
        visible: opacity > 0
        
        Behavior on opacity {
            NumberAnimation { duration: 300 }
        }
        
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 16
            
            Text {
                text: "🎮"
                font.pixelSize: 64
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
            
            Text {
                text: engineInitialized ? "Engine Running" : "Engine Initializing..."
                color: "#FFFFFF"
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
            
            Rectangle {
                Layout.preferredWidth: 200
                Layout.preferredHeight: 4
                color: "#6200EE"
                Layout.alignment: Qt.AlignHCenter
                
                Rectangle {
                    anchors.fill: parent
                    color: "#03DAC6"
                    width: parent.width * (engineInitialized ? 1 : 0.3)
                    
                    Behavior on width {
                        NumberAnimation { duration: 500 }
                    }
                }
            }
        }
    }
    
    // Engine viewport (will be integrated with C++)
    Rectangle {
        id: engineViewport
        anchors.fill: parent
        color: "#000000"
        visible: engineInitialized
        
        // Engine rendering surface placeholder
        // This will be replaced with actual OpenGL widget
        Text {
            anchors.centerIn: parent
            text: "3D Viewport\n(Qt Quick Integration)"
            color: "#3D3D3D"
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            visible: !engineInitialized
        }
    }
    
    // Viewport controls overlay
    Rectangle {
        id: controlsOverlay
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 16
        width: 200
        height: 120
        color: "#1E1E1E"
        radius: 8
        opacity: 0.8
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8
            
            Text {
                text: "📷 Viewport Controls"
                color: "#FFFFFF"
                font.pixelSize: 12
                font.bold: true
            }
            
            Row {
                spacing: 8
                Layout.fillWidth: true
                
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    color: "#2D2D2D"
                    radius: 4
                    Text {
                        anchors.centerIn: parent
                        text: "🔄"
                        font.pixelSize: 16
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClick: console.log("Reset camera")
                    }
                }
                
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    color: "#2D2D2D"
                    radius: 4
                    Text {
                        anchors.centerIn: parent
                        text: "🔍"
                        font.pixelSize: 16
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClick: console.log("Zoom in")
                    }
                }
                
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    color: "#2D2D2D"
                    radius: 4
                    Text {
                        anchors.centerIn: parent
                        text: "📐"
                        font.pixelSize: 16
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClick: console.log("Toggle grid")
                    }
                }
            }
            
            // FPS counter
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 24
                color: "#2D2D2D"
                radius: 4
                
                Text {
                    anchors.centerIn: parent
                    text: "60 FPS"
                    color: "#03DAC6"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }
    }
    
    // Context menu placeholder
    Rectangle {
        id: contextMenu
        anchors.centerIn: parent
        width: 200
        height: 150
        color: "#2D2D2D"
        radius: 8
        visible: false
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            
            Repeater {
                model: ["Add Entity", "Add Light", "Add Camera", "Refresh"]
                
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    color: mouseArea.containsMouse ? "#3D3D3D" : "#2D2D2D"
                    
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: "#FFFFFF"
                        font.pixelSize: 13
                    }
                    
                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            console.log("Context menu:", modelData)
                            contextMenu.visible = false
                        }
                    }
                }
            }
        }
    }
    
    // Mouse area for context menu
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onRightClicked: {
            contextMenu.visible = true
        }
    }
    
    // Engine bridge connection
    QtObject {
        id: engineBridgeConnection
        
        function onEngineInitialized() {
            engineInitialized = true
            console.log("[Viewport] Engine initialized")
        }
        
        function onEngineError(error) {
            engineInitialized = false
            console.log("[Viewport] Engine error:", error)
        }
    }
}
