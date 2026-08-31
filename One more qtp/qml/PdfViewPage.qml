import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Pdf

Page {
    id: root
    background: Rectangle {
        color: "#ffffff"
    }
    property url source: ""
    signal backRequested()

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            Button {
                text: qsTr("返回")
                onClicked: root.backRequested()
            }
            Label {
                text: qsTr("PDF 阅读")
                font.bold: true
                Layout.fillWidth: true
            }
        }
    }

    PdfDocument {
        id: document
        source: root.source
    }

    PdfScrollablePageView {
        id: pdfView
        anchors.fill: parent
        anchors.bottomMargin: 48
        document: document
    }

    RowLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.margins: 12
        spacing: 12

        Button {
            text: "<---"
            background: Rectangle { radius: 8; color: "#e8e8e8" }
            onClicked: pdfView.goToPage(pdfView.currentPage - 1)
        }
        Label {
            text: (pdfView.currentPage + 1) + " / " + document.pageCount
            font.bold: true
        }
        Button {
            text: "--->"
            background: Rectangle { radius: 8; color: "#e8e8e8" }
            onClicked: pdfView.goToPage(pdfView.currentPage + 1)
        }
    }
}
