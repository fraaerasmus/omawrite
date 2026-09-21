import QtQuick
import QtQuick.Controls
import Qt.labs.folderlistmodel

// The library list: every Markdown file in one folder, newest activity at the
// writer's fingertips. Deliberately not a file manager — no tree, no rename, no
// drag. Picking a document and starting a new one are the whole surface.
Item {
    id: sidebar

    property url folder
    property url currentFile
    property color pageColor
    property color textColor
    property color mutedColor
    property color accentColor
    property real textScale: 1.0

    signal fileChosen(url file)
    signal newRequested()

    function scaled(size) {
        return Math.round(size * textScale)
    }

    // A hair off the page so the edge reads without a hard rule.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(sidebar.textColor.r, sidebar.textColor.g, sidebar.textColor.b, 0.035)
    }

    Rectangle {
        width: 1
        color: Qt.rgba(sidebar.textColor.r, sidebar.textColor.g, sidebar.textColor.b, 0.08)
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
    }

    FolderListModel {
        id: files
        folder: sidebar.folder
        nameFilters: ["*.md", "*.markdown", "*.txt"]
        showDirs: false
        showHidden: false
        sortField: FolderListModel.Name
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

        Text {
            objectName: "newDocumentButton"
            text: "+"
            color: newArea.containsMouse ? sidebar.accentColor : sidebar.mutedColor
            font.family: "iA Writer Mono S"
            font.pixelSize: sidebar.scaled(17)
            anchors { right: parent.right; rightMargin: sidebar.scaled(14); verticalCenter: parent.verticalCenter }

            MouseArea {
                id: newArea
                anchors.fill: parent
                anchors.margins: -sidebar.scaled(8)
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: sidebar.newRequested()
            }
        }
    }

    ListView {
        id: list
        objectName: "libraryList"
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }
        anchors.bottomMargin: sidebar.scaled(32)
        clip: true
        model: files
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Item {
            required property int index
            required property string fileName
            required property url fileUrl

            width: list.width
            height: sidebar.scaled(30)

            readonly property bool current: fileUrl === sidebar.currentFile

            Rectangle {
                anchors.fill: parent
                anchors.rightMargin: 1
                color: parent.current
                    ? Qt.rgba(sidebar.textColor.r, sidebar.textColor.g, sidebar.textColor.b, 0.07)
                    : hover.containsMouse
                        ? Qt.rgba(sidebar.textColor.r, sidebar.textColor.g, sidebar.textColor.b, 0.04)
                        : "transparent"
            }

            Text {
                // The extension is noise when every row has one.
                text: fileName.replace(/\.(md|markdown|txt)$/i, "")
                color: parent.current ? sidebar.textColor : sidebar.mutedColor
                font.family: "iA Writer Mono S"
                font.pixelSize: sidebar.scaled(12)
                elide: Text.ElideRight
                anchors {
                    left: parent.left
                    right: parent.right
                    leftMargin: sidebar.scaled(16)
                    rightMargin: sidebar.scaled(12)
                    verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: hover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: sidebar.fileChosen(fileUrl)
            }
        }
    }

    Text {
        visible: files.count === 0 && files.status === FolderListModel.Ready
        text: "No documents yet"
        color: sidebar.mutedColor
        font.family: "iA Writer Mono S"
        font.pixelSize: sidebar.scaled(11)
        anchors { horizontalCenter: parent.horizontalCenter; top: header.bottom; topMargin: sidebar.scaled(24) }
    }
}
