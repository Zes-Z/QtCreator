import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Pdf

/*
 * ================================================================
 * PdfViewPage —— 全屏阅读
 * ================================================================
 *
 * 与预览面板的区别：
 *
 *     预览面板：一次一页，带上一页 / 下一页
 *     本页面  ：上下连续滚动，不提供翻页按钮
 *
 * 因此这里使用 PdfMultiPageView（多页连续视图），
 * 而不是 PdfScrollablePageView。
 *
 * source 必须是 file:/// URL。
 */
Page {
    id: root

    background: Rectangle { color: "#ffffff" }

    property url source: ""
    property string fitMode: "width"

    signal backRequested()

    readonly property bool documentReady:
        document.status === PdfDocument.Ready

    readonly property real zoomPercent:
        Math.round(pdfView.renderScale * 100)

    function applyFit() {
        if (!root.documentReady || pdfView.width <= 0 || pdfView.height <= 0) {
            return
        }

        if (root.fitMode === "width") {
            pdfView.scaleToWidth(pdfView.width, pdfView.height)
        } else {
            pdfView.scaleToPage(pdfView.width, pdfView.height)
        }
    }

    function zoomIn() {
        root.fitMode = "manual"
        pdfView.renderScale = Math.min(8, pdfView.renderScale * 1.25)
    }

    function zoomOut() {
        root.fitMode = "manual"
        pdfView.renderScale = Math.max(0.1, pdfView.renderScale / 1.25)
    }

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
            }

            Item { Layout.fillWidth: true }

            // 只显示进度，不提供翻页按钮。
            Label {
                text: root.documentReady && document.pageCount > 0
                      ? (pdfView.currentPage + 1) + " / " + document.pageCount
                      : "— / —"
                color: "#666666"
                horizontalAlignment: Text.AlignHCenter
                Layout.minimumWidth: 72
            }

            Button {
                text: "−"
                onClicked: root.zoomOut()
            }

            Button {
                text: root.zoomPercent + "%"
                implicitWidth: 76
                onClicked: root.fitMode =
                           root.fitMode === "page" ? "width" : "page"
            }

            Button {
                text: "＋"
                onClicked: root.zoomIn()
            }
        }
    }

    PdfDocument {
        id: document

        source: root.source

        onStatusChanged: {
            if (status === PdfDocument.Ready) {
                root.applyFit()
            }
        }
    }

    // 多页连续滚动
    PdfMultiPageView {
        id: pdfView

        anchors.fill: parent

        document: document
        visible: root.documentReady

        onWidthChanged: if (root.fitMode !== "manual") root.applyFit()
        onHeightChanged: if (root.fitMode !== "manual") root.applyFit()
    }

    Label {
        anchors.centerIn: parent
        width: parent.width - 64
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        visible: document.status === PdfDocument.Error
        color: "#d05a5a"
        text: qsTr("无法打开 PDF：")
              + (document.error.length > 0
                 ? document.error
                 : qsTr("文件不存在或格式不受支持"))
    }

    Label {
        anchors.centerIn: parent
        visible: root.source.toString().length > 0
                 && document.status === PdfDocument.Loading
        color: "#888888"
        text: qsTr("正在载入…")
    }
}
