import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtWebEngine
import Kanzi

// Hosts the Apple ID popup that MusicKit's authorize() opens.
//
// Apple offers passkeys / security keys once the email is entered. QtWebEngine
// leaves the WebAuthn UI to the application and the page waits indefinitely if
// nobody answers, so the overlay below handles every WebAuthn state and always
// offers a way back to the password form.
Window {
    id: win

    property var profile: AppleMusic.webProfile
    property alias view: web
    property var authRequest: null

    readonly property int authState: authRequest ? authRequest.state : -1

    width: 520
    height: 760
    minimumWidth: 400
    minimumHeight: 560
    title: qsTr("Sign in to Apple Music — Kanzi")
    color: Theme.bg
    modality: Qt.NonModal

    onVisibleChanged: if (!visible && authRequest) { authRequest.cancel(); authRequest = null }

    WebEngineView {
        id: web
        anchors.fill: parent
        profile: win.profile
        settings.javascriptCanOpenWindows: true
        onWindowCloseRequested: win.close()
        onNewWindowRequested: request => request.openIn(web)
        onPermissionRequested: permission => permission.grant()
        onWebAuthUxRequested: request => {
            win.authRequest = request
            pinField.text = ""
        }
        onJavaScriptConsoleMessage: (level, message, line, source) => {
            if (level !== WebEngineView.InfoMessageLevel)
                console.warn("[sign-in]", message, `(${source}:${line})`)
        }
        onLoadingChanged: info => {
            if (info.status === WebEngineView.LoadFailedStatus)
                console.warn("[sign-in] failed to load", info.url, info.errorString)
        }
    }

    Connections {
        target: win.authRequest
        ignoreUnknownSignals: true
        function onStateChanged() {
            const s = win.authRequest.state
            if (s === WebEngineWebAuthUxRequest.WebAuthUxState.Completed
                    || s === WebEngineWebAuthUxRequest.WebAuthUxState.Cancelled)
                win.authRequest = null
        }
    }

    // ---- WebAuthn overlay ----
    Rectangle {
        anchors.fill: parent
        visible: win.authRequest !== null
        color: Qt.rgba(0, 0, 0, 0.55)

        MouseArea { anchors.fill: parent }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 380)
            implicitHeight: box.implicitHeight + 48
            radius: Theme.radiusLarge
            color: Theme.surface
            border.color: Theme.border

            ColumnLayout {
                id: box
                anchors.fill: parent
                anchors.margins: 24
                spacing: 14

                Text {
                    Layout.fillWidth: true
                    text: win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.SelectAccount ? qsTr("Choose an account")
                        : win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin ? qsTr("Enter your security key PIN")
                        : win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.RequestFailed ? qsTr("Security key sign-in failed")
                        : qsTr("Use your security key")
                    color: Theme.text
                    font.pixelSize: Theme.fontLarge
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: text !== ""
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    text: {
                        const rp = win.authRequest ? win.authRequest.relyingPartyId : ""
                        switch (win.authState) {
                        case WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin: {
                            const left = win.authRequest.pinRequest.remainingAttempts
                            return left >= 0 ? qsTr("%1 attempts remaining.").arg(left) : ""
                        }
                        case WebEngineWebAuthUxRequest.WebAuthUxState.RequestFailed:
                            return qsTr("You can try again or sign in with your password.")
                        case WebEngineWebAuthUxRequest.WebAuthUxState.SelectAccount:
                            return rp
                        default:
                            return qsTr("Insert and touch your security key to sign in to %1. Passkeys stored on your phone or in a password manager aren't supported here, so use your password instead.").arg(rp)
                        }
                    }
                }

                Repeater {
                    model: win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.SelectAccount ? win.authRequest.userNames : []
                    PillButton {
                        required property string modelData
                        Layout.fillWidth: true
                        primary: false
                        text: modelData
                        onClicked: win.authRequest.setSelectedAccount(modelData)
                    }
                }

                TextField {
                    id: pinField
                    Layout.fillWidth: true
                    visible: win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin
                    echoMode: TextInput.Password
                    placeholderText: qsTr("PIN")
                    color: Theme.text
                    onAccepted: if (text) win.authRequest.setPin(text)
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: Theme.hover
                        border.width: pinField.activeFocus ? 2 : 0
                        border.color: Theme.accent
                    }
                }

                Spinner {
                    Layout.alignment: Qt.AlignHCenter
                    running: win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.FinishTokenCollection
                             || win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.Discovery
                             || win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.NotStarted
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    spacing: 10
                    Item { Layout.fillWidth: true }
                    PillButton {
                        text: qsTr("Use password instead")
                        primary: false
                        onClicked: {
                            const r = win.authRequest
                            win.authRequest = null
                            if (r) r.cancel()
                        }
                    }
                    PillButton {
                        visible: win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin
                                 || win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.RequestFailed
                        text: win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin ? qsTr("Continue") : qsTr("Retry")
                        onClicked: win.authState === WebEngineWebAuthUxRequest.WebAuthUxState.CollectPin
                                   ? win.authRequest.setPin(pinField.text) : win.authRequest.retry()
                    }
                }
            }
        }
    }
}
