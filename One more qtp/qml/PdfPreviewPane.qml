import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Pdf

Item {
    id: root
    property url source: ""

    PdfDocument {
        id: document
        source: root.source
    }

    PdfScrollablePageView {
        id: pdfView
        anchors.fill: parent
        document: document
    }

    RowLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.margins: 8
        spacing: 8

        Button {
            text: "<---"
            background: Rectangle { radius: 8; color: "#e8e8e8" }
            onClicked: pdfView.goToPage(pdfView.currentPage - 1)
        }
        Label {
            text: (pdfView.currentPage + 1) + " / " + document.pageCount
            color: "black"
        }
        Button {
            text: "--->"
            background: Rectangle { radius: 8; color: "#e8e8e8" }
            onClicked: pdfView.goToPage(pdfView.currentPage + 1)
        }
    }
}
