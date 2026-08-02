import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

import "."  // Import local components

ApplicationWindow {
    id: root
    visible: true
    width: 1600
    height: 900
    minimumWidth: 1200
    minimumHeight: 700
    title: "Quarter Flying Editor - Modern"
    
    // Modern Material Design 3 Theme
    color: "#121212"
    
    // Main Layout
    RowLayout {
        id: mainLayout
        anchors.fill: parent
        spacing: 0
        
        // Left Panel - Scene Hierarchy
        Rectangle {
            id: leftPanel
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            color: "#1E1E1E"
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                
                // Panel Header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    color: "#2D2D2D"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 8
                        
                        Text {
                            text: "🌍 SCENE HIERARCHY"
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        
                        Button {
                            text: "＋"
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            background: Rectangle {
                                color: "#6200EE"
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: 18
                            }
                        }
                    }
                }
                
                // Scene Tree
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                        // ListView {
                        id: sceneTree
                        model: engineBridge.sceneModel
                        delegate: SceneTreeItem {
                            name: model.name
                            type: model.type
                            depth: model.depth
                            
                            onClicked: {
                                engineBridge.selectObject(model.name, model.type)
                                inspectorModel.selectObject(model.name, model.type)
                            }
                        }
                        background: Rectangle {
                            color: "#1E1E1E"
                        }
                    }
                }
            }
        }
        
        // Center Panel - Viewport
        Rectangle {
            id: centerPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#000000"
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                
                // Toolbar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    color: "#2D2D2D"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 8
                        
                        // Mode Buttons
                        Button {
                            text: "🎮 Scene"
                            checked: true
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 36
                            background: Rectangle {
                                color: parent.checked ? "#6200EE" : "#3D3D3D"
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        
                        Button {
                            text: "▶ Play"
                            Layout.preferredWidth: 100
                            Layout.preferredHeight: 36
                            background: Rectangle {
                                color: "#03DAC6"
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "black"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        // Scene Name
                        Text {
                            text: "Untitled Scene"
                            color: "#B0B0B0"
                            font.pixelSize: 13
                        }
                    }
                }
                
                // Viewport Area
                ViewportPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
        
        // Right Panel - Inspector
        InspectorPanel {
            id: inspectorPanel
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            
            // Connect to inspector model
            Component.onCompleted: {
                inspectorPanel.objectName = Qt.binding(function() { return inspectorModel.objectName })
                inspectorPanel.objectType = Qt.binding(function() { return inspectorModel.objectType })
            }
        }
    }
    
    // Scene Model (will be connected to C++)
    ListModel {
        id: sceneModel
        ListElement { name: "World Root"; type: "root"; depth: 0 }
        ListElement { name: "Main Camera"; type: "camera"; depth: 1 }
        ListElement { name: "Directional Light"; type: "light"; depth: 1 }
        ListElement { name: "Static Meshes"; type: "folder"; depth: 1 }
        ListElement { name: "Floor"; type: "mesh"; depth: 2 }
        ListElement { name: "WallNorth"; type: "mesh"; depth: 2 }
        ListElement { name: "Player"; type: "entity"; depth: 1 }
    }
    

}
