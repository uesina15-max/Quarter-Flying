import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: itemRoot
    width: ListView.view.width
    height: 36
    color: ListView.isCurrentItem ? "#2D2D2D" : "#1E1E1E"
    
    property string name: ""
    property string type: ""
    property int depth: 0
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12 + depth * 16
        spacing: 8
        
        // Icon based on type
        Text {
            text: {
                switch(type) {
                    case "root": return "🌍"
                    case "camera": return "🎥"
                    case "light": return "💡"
                    case "folder": return "📁"
                    case "mesh": return "📦"
                    case "entity": return "🧍"
                    default: return "🔲"
                }
            }
            color: "#FFFFFF"
            font.pixelSize: 16
            Layout.preferredWidth: 24
        }
        
        Text {
            text: name
            color: "#FFFFFF"
            font.pixelSize: 13
            Layout.fillWidth: true
        }
        
        // Context menu indicator
        Rectangle {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            color: "transparent"
            radius: 4
            
            Text {
                anchors.centerIn: parent
                text: "⋮"
                color: "#B0B0B0"
                font.pixelSize: 16
            }
            
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    // Show context menu
                    console.log("Context menu for:", name)
                }
            }
        }
    }
    
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            itemRoot.color = "#252525"
        }
        onExited: {
            itemRoot.color = ListView.isCurrentItem ? "#2D2D2D" : "#1E1E1E"
        }
        onClick: {
            ListView.view.currentIndex = index
        }
    }
}
