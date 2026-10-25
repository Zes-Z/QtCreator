import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Pdf

/*
 * ================================================================
 * PdfPreviewPane
 * ================================================================
 *
 * 重要：
 *
 *     PdfDocument.source 是 url 类型。
 *
 *     必须传真正的 file:/// URL，不能传 Windows 路径字符串，
 *     否则会被解析成 scheme 为 "c" 的非法 URL，
 *     pageCount 变成 0，视图里一片空白。
 *
 *     调用方请使用 FilePage.toFileUrl()。
 *
 * 关于尺寸 / 居中 / 缩放：
 *
 *     PdfScrollablePageView 本身是一个 Flickable，
 *     contentWidth / contentHeight 就是页面渲染后的尺寸，
 *     renderScale 默认 1（固定尺寸，不随窗口变化）。
 *
 *     所以这里：
 *
 *       1. 文档就绪后自动 scaleToPage / scaleToWidth
 *       2. 页面小于视口时，用 Flickable 的 margin 把它居中
 *       3. 缩放按钮直接改 renderScale
 */
Item {
    id: root

    property url source: ""

    /*
     * "page"   / 整页可见
     * "width"  / 宽度铺满（默认，阅读更舒服）
     * "manual" / 用户手动缩放
     */
    property string fitMode: "width"
    property real zoomStep: 1.25

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

    function setFitMode(mode) {
        root.fitMode = mode
        root.applyFit()
    }

    /*
     * 翻页必须钳制在 [0, pageCount - 1]。
     *
     * 否则在最后一页按「下一页」会请求不存在的页，
     * Qt PDF 会打印 failed to load page N。
     */
    function gotoPage(page) {
        const last = document.pageCount - 1
        if (last < 0) return
        pdfView.goToPage(Math.max(0, Math.min(last, page)))
    }

    function zoomIn() {
        root.fitMode = "manual"
        pdfView.renderScale = Math.min(8, pdfView.renderScale * root.zoomStep)
    }

    function zoomOut() {
        root.fitMode = "manual"
        pdfView.renderScale = Math.max(0.1, pdfView.renderScale / root.zoomStep)
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

    PdfScrollablePageView {
        id: pdfView

        anchors.fill: parent
        anchors.bottomMargin: 42

        document: document
        visible: root.documentReady

        /*
         * 页面比视口小时居中。
         *
         * Flickable 的 contentX 会被限制在
         * [-leftMargin, contentWidth - width + rightMargin] 之间。
         *
         * 当 contentWidth < width 时，
         * 令 leftMargin = rightMargin = (width - contentWidth) / 2，
         * 该区间塌缩成一个点，页面正好居中。
         */
        leftMargin: root.documentReady
                    ? Math.max(0, (width - contentWidth) / 2)
                    : 0
        rightMargin: leftMargin
        topMargin: root.documentReady
                   ? Math.max(0, (height - contentHeight) / 2)
                   : 0
        bottomMargin: topMargin

        onWidthChanged: if (root.fitMode !== "manual") root.applyFit()
        onHeightChanged: if (root.fitMode !== "manual") root.applyFit()
    }

    // ---- 加载失败 ----
    Label {
        anchors.centerIn: parent
        width: parent.width - 32
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

    Label {
        anchors.centerIn: parent
        visible: root.source.toString().length === 0
        color: "#888888"
        text: qsTr("双击左侧或中间的文件进行预览")
    }

    // ---- 工具栏 ----
    RowLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        spacing: 6

        Button {
            text: "−"
            implicitWidth: 32
            enabled: root.documentReady
            onClicked: root.zoomOut()
        }

        Button {
            text: root.zoomPercent + "%"
            implicitWidth: 64
            enabled: root.documentReady
            onClicked: root.setFitMode(root.fitMode === "page" ? "width" : "page")
        }

        Button {
            text: "＋"
            implicitWidth: 32
            enabled: root.documentReady
            onClicked: root.zoomIn()
        }

        Item { Layout.preferredWidth: 10 }

        Button {
            text: "◀"
            implicitWidth: 32
            enabled: root.documentReady && document.pageCount > 0
            onClicked: root.gotoPage(pdfView.currentPage - 1)
        }

        Label {
            text: root.documentReady && document.pageCount > 0
                  ? (pdfView.currentPage + 1) + " / " + document.pageCount
                  : "— / —"
            color: root.paletteText
            horizontalAlignment: Text.AlignHCenter
            Layout.minimumWidth: 64
        }

        Button {
            text: "▶"
            implicitWidth: 32
            enabled: root.documentReady && document.pageCount > 0
            onClicked: root.gotoPage(pdfView.currentPage + 1)
        }
    }

    // 沿用外层主题色（未设置时用中性色）
    property color paletteText: "#222222"
}
