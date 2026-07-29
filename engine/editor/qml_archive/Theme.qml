pragma Singleton
import QtQuick 2.15

QtObject {
    // Material Design 3 Color Palette
    readonly property color primary: "#6200EE"
    readonly property color onPrimary: "#FFFFFF"
    readonly property color primaryContainer: "#6200EE"
    readonly property color onPrimaryContainer: "#FFFFFF"
    
    readonly property color secondary: "#03DAC6"
    readonly property color onSecondary: "#000000"
    readonly property color secondaryContainer: "#03DAC6"
    readonly property color onSecondaryContainer: "#000000"
    
    readonly property color background: "#121212"
    readonly property color onBackground: "#FFFFFF"
    
    readonly property color surface: "#1E1E1E"
    readonly property color onSurface: "#FFFFFF"
    readonly property color surfaceVariant: "#2D2D2D"
    readonly property color onSurfaceVariant: "#E0E0E0"
    
    readonly property color error: "#B00020"
    readonly property color onError: "#FFFFFF"
    readonly property color errorContainer: "#B00020"
    readonly property color onErrorContainer: "#FFFFFF"
    
    readonly property color outline: "#404040"
    readonly property color outlineVariant: "#303030"
    
    // Typography
    readonly property font displayLarge: Qt.font({family: "Roboto", pixelSize: 57, weight: Font.Bold})
    readonly property font displayMedium: Qt.font({family: "Roboto", pixelSize: 45, weight: Font.Bold})
    readonly property font displaySmall: Qt.font({family: "Roboto", pixelSize: 36, weight: Font.Bold})
    
    readonly property font headlineLarge: Qt.font({family: "Roboto", pixelSize: 32, weight: Font.Bold})
    readonly property font headlineMedium: Qt.font({family: "Roboto", pixelSize: 28, weight: Font.Bold})
    readonly property font headlineSmall: Qt.font({family: "Roboto", pixelSize: 24, weight: Font.Bold})
    
    readonly property font titleLarge: Qt.font({family: "Roboto", pixelSize: 22, weight: Font.Bold})
    readonly property font titleMedium: Qt.font({family: "Roboto", pixelSize: 16, weight: Font.Medium})
    readonly property font titleSmall: Qt.font({family: "Roboto", pixelSize: 14, weight: Font.Medium})
    
    readonly property font bodyLarge: Qt.font({family: "Roboto", pixelSize: 16, weight: Font.Normal})
    readonly property font bodyMedium: Qt.font({family: "Roboto", pixelSize: 14, weight: Font.Normal})
    readonly property font bodySmall: Qt.font({family: "Roboto", pixelSize: 12, weight: Font.Normal})
    
    readonly property font labelLarge: Qt.font({family: "Roboto", pixelSize: 14, weight: Font.Medium})
    readonly property font labelMedium: Qt.font({family: "Roboto", pixelSize: 12, weight: Font.Medium})
    readonly property font labelSmall: Qt.font({family: "Roboto", pixelSize: 11, weight: Font.Medium})
    
    // Spacing
    readonly property int spacingXS: 4
    readonly property int spacingS: 8
    readonly property int spacingM: 16
    readonly property int spacingL: 24
    readonly property int spacingXL: 32
    
    // Border Radius
    readonly property int radiusS: 4
    readonly property int radiusM: 8
    readonly property int radiusL: 16
    readonly property int radiusXL: 24
    
    // Elevation
    readonly property int elevationNone: 0
    readonly property int elevationS: 1
    readonly property int elevationM: 2
    readonly property int elevationL: 4
    readonly property int elevationXL: 8
}
