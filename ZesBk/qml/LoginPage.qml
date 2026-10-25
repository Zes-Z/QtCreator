import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    background: Rectangle {
        color: "#ffffff"
    }
    signal loginSucceeded()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, 360)
        spacing: 12

        Label {
            text: qsTr("文献阅读 / 办公应用")
            font.pixelSize: 24
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        TextField {
            id: emailField
            placeholderText: qsTr("邮箱")
            Layout.fillWidth: true
        }

        TextField {
            id: passwordField
            placeholderText: qsTr("密码")
            echoMode: TextInput.Password
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("登录")
                Layout.fillWidth: true
                onClicked: authService.login(emailField.text.trim(), passwordField.text)
            }
            Button {
                text: qsTr("注册")
                Layout.fillWidth: true
                onClicked: authService.registerAccount(emailField.text.trim(), passwordField.text)
            }
        }

        Label {
            id: statusLabel
            color: "red"
            wrapMode: Text.Wrap
            visible: text.length > 0
            Layout.fillWidth: true
        }
    }

    Connections {
        target: authService
        function onLoginFinished(ok, email, error) {
            if (ok) {
                root.loginSucceeded()
            } else {
                statusLabel.text = error
            }
        }
        function onRegisterFinished(ok, email, error) {
            if (ok) {
                root.loginSucceeded()
            } else {
                statusLabel.text = error
            }
        }
    }
}
