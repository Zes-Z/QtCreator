import QtQuick
import QtQuick.Controls
import LitApp

ApplicationWindow {
    id: root

    width: 1200
    height: 760

    // 低于这个尺寸三栏面板会互相挤压，干脆不让窗口再缩。
    minimumWidth: 620
    minimumHeight: 480

    visible: true
    title: qsTr("文献阅读 / 办公应用")

    background: Rectangle { color: "#ffffff" }

    Component {
        id: filePageComponent

        FilePage {
            onLogoutRequested: {
                stack.pop(null)
            }

            onOpenPdfFullscreen: function(path) {
                if (!hasPdfSupport) {
                    return
                }

                const comp = Qt.createComponent(
                    "qrc:/qt/qml/LitApp/qml/PdfViewPage.qml")

                if (comp.status !== Component.Ready) {
                    console.warn("PDF 全屏页不可用:", comp.errorString())
                    return
                }

                const page = comp.createObject(stack, { "source": path })

                if (!page) {
                    return
                }

                if (page.backRequested !== undefined) {
                    page.backRequested.connect(function() {
                        stack.pop()
                    })
                }

                stack.push(page)
            }
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: filePageComponent
    }
}
