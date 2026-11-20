import QtQuick
import QtQuick.Templates as T
import CanUi

// Text tabs with a sliding accent underline.
T.TabBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    spacing: Theme.space.xs

    contentItem: ListView {
        model: control.contentModel
        currentIndex: control.currentIndex
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.AutoFlickIfNeeded
        snapMode: ListView.SnapToItem
        interactive: false

        highlightMoveDuration: Theme.motion.normal
        highlightResizeDuration: Theme.motion.normal
        highlightFollowsCurrentItem: true
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: 0
        preferredHighlightEnd: width
        highlight: Item {
            z: 2
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.space.sm
                anchors.rightMargin: Theme.space.sm
                anchors.bottom: parent.bottom
                height: 2
                radius: 1
                color: Theme.accent
            }
        }
    }

    // No baseline of its own: the page draws one full-width divider under the tabs.
    background: Item {}
}
