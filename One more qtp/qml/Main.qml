import QtQuick
import QtQuick.Controls
import LitApp

ApplicationWindow {
    id: root
    width: 1100
    height: 700
    visible: true
    title: qsTr("文献阅读 / 办公应用")
    background: Rectangle {
        color: "#ffffff"
    }

    Component {
        id: filePageComponent
        FilePage {
            onLogoutRequested: stack.replace(filePageComponent)
            onOpenPdfFullscreen: function(path) {
                var comp = Qt.createComponent("qrc:/qt/qml/LitApp/qml/PdfViewPage.qml")
                if (comp.status === Component.Ready) {
                    var page = comp.createObject(stack, { "source": path })
                    page.backRequested.connect(function() {
                        stack.pop()
                    })
                    stack.push(page)
                } else {
                    console.warn(comp.errorString())
                }
            }
        }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: filePageComponent
    }
}
