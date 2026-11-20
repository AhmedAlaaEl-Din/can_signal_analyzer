import QtQuick

// 1 px separator; vertical when `vertical` is true.
Rectangle {
    property bool vertical: false
    implicitWidth: vertical ? 1 : 100
    implicitHeight: vertical ? 20 : 1
    color: Theme.border
}
