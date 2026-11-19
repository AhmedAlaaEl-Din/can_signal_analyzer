import QtQuick

// Keyboard focus indicator drawn just outside the parent rounded rectangle.
Rectangle {
    property Item target
    anchors.fill: parent
    anchors.margins: -3
    radius: (parent && parent.radius !== undefined ? parent.radius : 0) + 3
    color: "transparent"
    border.width: 2
    border.color: Theme.focusRing
    visible: target !== null && target.visualFocus
}
