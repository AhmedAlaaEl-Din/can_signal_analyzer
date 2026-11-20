import QtQuick
import QtQuick.Controls.impl as Impl

// Monochrome SVG icon from the project's own icon set, tinted with `color`.
Item {
    id: icon
    property string name: ""
    property int size: Theme.size.icon
    property color color: Theme.text

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    Impl.IconImage {
        anchors.fill: parent
        source: icon.name.length > 0 ? Qt.resolvedUrl("icons/" + icon.name + ".svg") : ""
        sourceSize: Qt.size(icon.size, icon.size)
        color: icon.color
        fillMode: Image.PreserveAspectFit
    }
}
