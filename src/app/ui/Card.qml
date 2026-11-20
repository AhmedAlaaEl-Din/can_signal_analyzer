import QtQuick
import QtQuick.Effects

// Surface container. `elevation` 0..3 maps to a surface color plus a soft shadow.
Item {
    id: card
    property int elevation: 1
    property int radius: Theme.radius.lg
    property bool bordered: true
    property bool shadow: elevation > 0
    property alias color: surface.color
    default property alias content: surface.data
    readonly property alias surfaceItem: surface

    Rectangle {
        id: shadowSource
        anchors.fill: surface
        radius: card.radius
        color: "black"
        visible: false
    }
    MultiEffect {
        source: shadowSource
        anchors.fill: shadowSource
        visible: card.shadow
        shadowEnabled: true
        shadowColor: Theme.shadow
        shadowBlur: Theme.shadowBlur(card.elevation)
        shadowVerticalOffset: Theme.shadowOffset(card.elevation)
        shadowHorizontalOffset: 0
        autoPaddingEnabled: true
        opacity: 0.9
    }

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: card.radius
        color: Theme.elevationColor(card.elevation)
        border.width: card.bordered ? 1 : 0
        border.color: Theme.border
    }
}
