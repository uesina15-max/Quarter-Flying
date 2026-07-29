import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: inspectorRoot
    color: "#1E1E1E"
    
    property var selectedObject: null
    property string objectName: "No Selection"
    property string objectType: ""
    
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
                    text: "🔧 INSPECTOR"
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.bold: true
                    Layout.fillWidth: true
                }
                
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    color: "#3D3D3D"
                    radius: 4
                    Text {
                        anchors.centerIn: parent
                        text: "↺"
                        color: "#B0B0B0"
                        font.pixelSize: 16
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClick: console.log("Reset inspector")
                    }
                }
            }
        }
        
        // Object Name Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: "#252525"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 4
                
                Text {
                    text: objectName
                    color: "#FFFFFF"
                    font.pixelSize: 16
                    font.bold: true
                    Layout.fillWidth: true
                }
                
                Text {
                    text: objectType
                    color: "#6200EE"
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }
        
        // Scrollable Content
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            Column {
                width: parent.width
                spacing: 16
                padding: 16
                
                // Transform Section
                InspectorSection {
                    title: "Transform"
                    icon: "📐"
                    
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        
                        InspectorProperty {
                            label: "Position"
                            value: "0.0, 0.0, 0.0"
                            width: parent.width - 80
                        }
                    }
                    
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        
                        InspectorProperty {
                            label: "Rotation"
                            value: "0.0, 0.0, 0.0"
                            width: parent.width - 80
                        }
                    }
                    
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        
                        InspectorProperty {
                            label: "Scale"
                            value: "1.0, 1.0, 1.0"
                            width: parent.width - 80
                        }
                    }
                }
                
                // Material Section
                InspectorSection {
                    title: "Material"
                    icon: "🎨"
                    
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        
                        InspectorProperty {
                            label: "Color"
                            value: "#FF5722"
                            isColor: true
                            width: parent.width - 80
                        }
                    }
                    
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        
                        InspectorProperty {
                            label: "Roughness"
                            value: "0.5"
                            isSlider: true
                            sliderValue: 0.5
                            width: parent.width - 80
                        }
                    }
                    
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        
                        InspectorProperty {
                            label: "Metallic"
                            value: "0.0"
                            isSlider: true
                            sliderValue: 0.0
                            width: parent.width - 80
                        }
                    }
                }
                
                // Lighting Section
                InspectorSection {
                    title: "Lighting"
                    icon: "💡"
                    
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        
                        InspectorProperty {
                            label: "Intensity"
                            value: "1.0"
                            isSlider: true
                            sliderValue: 1.0
                            width: parent.width - 80
                        }
                    }
                    
                    Row {
                        spacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        
                        InspectorProperty {
                            label: "Color"
                            value: "#FFFFFF"
                            isColor: true
                            width: parent.width - 80
                        }
                    }
                }
                
                // Tags Section
                InspectorSection {
                    title: "Tags"
                    icon: "🏷️"
                    
                    Flow {
                        width: parent.width
                        spacing: 8
                        
                        Repeater {
                            model: ["Static", "Cast Shadow", "Receive Shadow"]
                            
                            Rectangle {
                                width: 80
                                height: 28
                                color: "#6200EE"
                                radius: 14
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: "#FFFFFF"
                                    font.pixelSize: 11
                                }
                                
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: console.log("Tag clicked:", modelData)
                                }
                            }
                        }
                        
                        Rectangle {
                            width: 28
                            height: 28
                            color: "#3D3D3D"
                            radius: 14
                            
                            Text {
                                anchors.centerIn: parent
                                text: "+"
                                color: "#FFFFFF"
                                font.pixelSize: 16
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: console.log("Add tag")
                            }
                        }
                    }
                }
                
                // Components Section
                InspectorSection {
                    title: "Components"
                    icon: "⚙️"
                    
                    Column {
                        width: parent.width
                        spacing: 8
                        
                        Repeater {
                            model: ["Mesh Renderer", "Box Collider", "Rigidbody"]
                            
                            Rectangle {
                                width: parent.width
                                height: 36
                                color: "#2D2D2D"
                                radius: 4
                                
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8
                                    
                                    Text {
                                        text: modelData
                                        color: "#FFFFFF"
                                        font.pixelSize: 12
                                        Layout.fillWidth: true
                                    }
                                    
                                    Rectangle {
                                        Layout.preferredWidth: 24
                                        Layout.preferredHeight: 24
                                        color: "#3D3D3D"
                                        radius: 4
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: "#B0B0B0"
                                            font.pixelSize: 14
                                        }
                                        
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: console.log("Remove component:", modelData)
                                        }
                                    }
                                }
                            }
                        }
                        
                        Rectangle {
                            width: parent.width
                            height: 36
                            color: "#6200EE"
                            radius: 4
                            
                            Text {
                                anchors.centerIn: parent
                                text: "+ Add Component"
                                color: "#FFFFFF"
                                font.pixelSize: 12
                            }
                            
                            MouseArea {
                                anchors.fill: parent
                                onClicked: console.log("Add component")
                            }
                        }
                    }
                }
            }
        }
    }
}

// Component: Inspector Section
Component {
    id: inspectorSection
    property string title: ""
    property string icon: ""
    
    Rectangle {
        width: parent.width
        height: sectionColumn.height + 16
        color: "#252525"
        radius: 8
        
        Column {
            id: sectionColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 12
            
            // Section Header
            RowLayout {
                spacing: 8
                
                Text {
                    text: icon
                    font.pixelSize: 16
                }
                
                Text {
                    text: title
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    font.bold: true
                    Layout.fillWidth: true
                }
                
                Rectangle {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    color: "#3D3D3D"
                    radius: 4
                    
                    Text {
                        anchors.centerIn: parent
                        text: collapsed ? "▶" : "▼"
                        color: "#B0B0B0"
                        font.pixelSize: 10
                    }
                    
                    property bool collapsed: false
                    
                    MouseArea {
                        anchors.fill: parent
                        onClicked: collapsed = !collapsed
                    }
                }
            }
            
            // Section Content
            Item {
                id: contentArea
                width: parent.width
                height: childrenRect.height
                default property alias children: contentData.children
            }
        }
    }
}

// Component: Inspector Property
Component {
    id: inspectorProperty
    property string label: ""
    property string value: ""
    property bool isColor: false
    property bool isSlider: false
    property real sliderValue: 0.0
    property int width: 200
    
    RowLayout {
        spacing: 8
        
        Text {
            text: label
            color: "#B0B0B0"
            font.pixelSize: 11
            Layout.preferredWidth: 70
        }
        
        Rectangle {
            Layout.preferredWidth: width - 78
            Layout.preferredHeight: 24
            color: isColor ? value : "#1E1E1E"
            radius: 4
            border.color: "#3D3D3D"
            border.width: 1
            
            if (!isColor && !isSlider) {
                Text {
                    anchors.centerIn: parent
                    text: value
                    color: "#FFFFFF"
                    font.pixelSize: 11
                }
            }
            
            if (isSlider) {
                Slider {
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    from: 0.0
                    to: 1.0
                    value: sliderValue
                    
                    background: Rectangle {
                        color: "#3D3D3D"
                        radius: 2
                    }
                    
                    contentItem: Rectangle {
                        color: "#6200EE"
                        radius: 2
                    }
                }
            }
            
            MouseArea {
                anchors.fill: parent
                onClicked: console.log("Property clicked:", label)
            }
        }
    }
}
