import QtQuick
import QtQuick.Controls

// The library list: every Markdown file in one folder, pinned ones first.
// Deliberately not a file manager — no tree, no drag, no rename. Opening,
// starting, pinning and discarding a document are the whole surface.
Item {
    id: sidebar

    property var model
    property url currentFile
    property color pageColor
    property color textColor
    property color mutedColor
    property color accentColor
    property real textScale: 1.0

    signal fileChosen(url file)
    signal newRequested()
    signal collapseRequested()

    function scaled(size) {
        return Math.round(size * textScale)
    }

    // Same paper as the editor: the hairline below is the only division.
    Rectangle {
        anchors.fill: parent
        color: sidebar.pageColor
    }

    Rectangle {
        width: 1
        color: Qt.rgba(sidebar.textColor.r, sidebar.textColor.g, sidebar.textColor.b, 0.08)
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
    }

    component HeaderButton: Text {
        required property string tip
        signal triggered()

        color: area.containsMouse ? sidebar.accentColor : sidebar.mutedColor
        font.family: "iA Writer Mono S"
        font.pixelSize: sidebar.scaled(15)

        ToolTip.visible: area.containsMouse
        ToolTip.text: tip
        ToolTip.delay: 600

        MouseArea {
            id: area
            anchors.fill: parent
            anchors.margins: -sidebar.scaled(7)
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.triggered()
        }
    }

    Item {
        id: header
        height: sidebar.scaled(44)
        anchors { left: parent.left; right: parent.right; top: parent.top }

        Text {
            text: "Library"
            color: sidebar.mutedColor
            font.family: "iA Writer Mono S"
            font.pixelSize: sidebar.scaled(11)
            anchors { left: parent.left; leftMargin: sidebar.scaled(16); verticalCenter: parent.verticalCenter }
        }

        Row {
            spacing: sidebar.scaled(14)
            anchors { right: parent.right; rightMargin: sidebar.scaled(14); verticalCenter: parent.verticalCenter }

            HeaderButton {
                objectName: "newDocumentButton"
                text: "+"
                tip: "New document"
                onTriggered: sidebar.newRequested()
            }

            HeaderButton {
                objectName: "collapseButton"
                text: "«"
                tip: "Hide sidebar  (Ctrl+\\)"
                onTriggered: sidebar.collapseRequested()
            }
        }
    }

    ListView {
        id: list
        objectName: "libraryList"
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        anchors.bottomMargin: sidebar.scaled(32)
        clip: true
        model: sidebar.model
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Item {
            id: row

            required property string displayName
            required property url fileUrl
            required property bool pinned

            width: list.width
            height: sidebar.scaled(30)

            readonly property bool current: fileUrl === sidebar.currentFile

            Rectangle {
                anchors.fill: parent
                anchors.rightMargin: 1
                color: row.current
                    ? Qt.rgba(sidebar.textColor.r, sidebar.textColor.g, sidebar.textColor.b, 0.07)
                    : hover.containsMouse
                        ? Qt.rgba(sidebar.textColor.r, sidebar.textColor.g, sidebar.textColor.b, 0.04)
                        : "transparent"
            }

            Text {
                text: row.displayName
                color: row.current ? sidebar.textColor : sidebar.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: sidebar.scaled(12)
                elide: Text.ElideRight
                anchors {
                    left: parent.left
                    right: pinMark.left
                    leftMargin: sidebar.scaled(16)
                    rightMargin: sidebar.scaled(6)
                    verticalCenter: parent.verticalCenter
                }
            }

            // A dot rather than a pin glyph: the font has no pin, and a pinned
            // row is already distinguished by sitting at the top.
            Text {
                id: pinMark
                text: row.pinned ? "•" : ""
                color: sidebar.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: sidebar.scaled(12)
                anchors {
                    right: parent.right
                    rightMargin: sidebar.scaled(14)
                    verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: hover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: function (mouse) {
                    if (mouse.button === Qt.RightButton)
                        rowMenu.popup();
                    else
                        sidebar.fileChosen(row.fileUrl);
                }
            }

            Menu {
                id: rowMenu
                objectName: "rowMenu"

                MenuItem {
                    objectName: "pinMenuItem"
                    text: row.pinned ? "Unpin" : "Pin to top"
                    onTriggered: sidebar.model.togglePinned(row.fileUrl)
                }

                MenuItem {
                    objectName: "deleteMenuItem"
                    text: "Move to trash"
                    onTriggered: sidebar.model.moveToTrash(row.fileUrl)
                }
            }
        }
    }

    Text {
        visible: !sidebar.model || sidebar.model.count === 0
        text: "No documents yet"
        color: sidebar.mutedColor
        font.family: "iA Writer Mono S"
        font.pixelSize: sidebar.scaled(11)
        anchors { horizontalCenter: parent.horizontalCenter; top: header.bottom; topMargin: sidebar.scaled(24) }
    }
}
