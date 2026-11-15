import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kanzi

Flickable {
    id: root

    contentHeight: column.implicitHeight + 48
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    component Card: Rectangle {
        id: card
        default property alias content: inner.data
        property string title
        Layout.fillWidth: true
        Layout.maximumWidth: 760
        Layout.leftMargin: Theme.gutter
        Layout.rightMargin: Theme.gutter
        implicitHeight: inner.implicitHeight + 40
        radius: Theme.radiusLarge
        color: Theme.surface
        border.color: Theme.border

        ColumnLayout {
            id: inner
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14
            Text {
                text: card.title
                color: Theme.text
                font.pixelSize: Theme.fontLarge
                font.weight: Font.Bold
            }
        }
    }

    component Toggle: Switch {
        focusPolicy: Qt.NoFocus
        palette.button: Theme.surface
        palette.dark: Theme.accent
        palette.mid: Theme.placeholder
    }

    component Row_: RowLayout {
        id: settingRow
        property string label
        property string hint
        Layout.fillWidth: true
        spacing: 16
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text { text: settingRow.label; color: Theme.text }
            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: settingRow.hint
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }
        }
    }

    ColumnLayout {
        id: column
        width: root.width
        spacing: 20

        PageHeader {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            title: qsTr("Settings")
        }

        // ---- playback ----
        Card {
            title: qsTr("Playback")

            Row_ {
                label: qsTr("Audio output")
                hint: AudioOutput.available
                      ? qsTr("Kanzi's audio goes to this device. Switching takes effect immediately.")
                      : qsTr("No PulseAudio/PipeWire server found, so Kanzi uses the system default.")

                ComboBox {
                    id: outputBox
                    Layout.preferredWidth: 300
                    enabled: AudioOutput.available
                    textRole: "name"
                    valueRole: "id"

                    model: {
                        const list = [{ id: "", name: qsTr("System default") }].concat(AudioOutput.devices)
                        // Keep a remembered device visible even while it's unplugged.
                        if (AudioOutput.device && !list.some(d => d.id === AudioOutput.device))
                            list.push({ id: AudioOutput.device, name: qsTr("%1 (disconnected)").arg(AudioOutput.device) })
                        return list
                    }
                    onModelChanged: currentIndex = indexOfValue(AudioOutput.device)
                    Component.onCompleted: currentIndex = indexOfValue(AudioOutput.device)
                    onActivated: AudioOutput.device = currentValue
                }
            }

            Row_ {
                label: qsTr("Crossfade")
                hint: Player.crossfade > 0
                      ? qsTr("Fades each song out over its last %1 seconds and the next one in. Apple Music streams one song at a time, so songs don't overlap.").arg(Player.crossfade)
                      : qsTr("Off")
                Slider {
                    id: crossfadeSlider
                    Layout.preferredWidth: 220
                    from: 0
                    to: 12
                    stepSize: 1
                    snapMode: Slider.SnapAlways
                    value: Player.crossfade
                    onMoved: Player.crossfade = value
                }
                Text {
                    Layout.preferredWidth: 34
                    text: crossfadeSlider.value > 0 ? qsTr("%1 s").arg(crossfadeSlider.value) : qsTr("Off")
                    color: Theme.textDim
                    horizontalAlignment: Text.AlignRight
                }
            }

            Row_ {
                label: qsTr("AutoMix")
                hint: qsTr("Listens to the end of each song and moves on as soon as it fades into silence, so there's no dead air between songs. Fades adapt to how the song ends.")
                Toggle {
                    checked: Player.automix
                    onToggled: Player.automix = checked
                }
            }
        }

        // ---- discord ----
        Card {
            title: qsTr("Discord")

            Row_ {
                label: qsTr("Show what you're listening to")
                hint: DiscordPresence.status
                Toggle {
                    checked: DiscordPresence.enabled
                    onToggled: DiscordPresence.enabled = checked
                }
            }
            Row_ {
                label: qsTr("Also show while paused")
                Toggle {
                    enabled: DiscordPresence.enabled
                    checked: DiscordPresence.showWhenPaused
                    onToggled: DiscordPresence.showWhenPaused = checked
                }
            }
            Row_ {
                label: qsTr("Discord application ID")
                hint: qsTr("Kanzi's own application is used by default. To show a different name after “Listening to”, paste the ID of your own app from discord.com/developers/applications. Clear the field to go back to the default.")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                TextField {
                    id: discordId
                    Layout.fillWidth: true
                    enabled: DiscordPresence.enabled
                    text: DiscordPresence.clientId
                    placeholderText: qsTr("e.g. 1234567890123456789")
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: RegularExpressionValidator { regularExpression: /[0-9]{0,24}/ }
                    color: Theme.text
                    selectByMouse: true
                    onAccepted: DiscordPresence.clientId = text
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: Theme.hover
                        border.width: discordId.activeFocus ? 2 : 0
                        border.color: Theme.accent
                    }
                }
                PillButton {
                    text: qsTr("Save")
                    primary: false
                    enabled: DiscordPresence.enabled && discordId.text !== DiscordPresence.clientId
                    onClicked: DiscordPresence.clientId = discordId.text
                }
            }
        }

        // ---- window ----
        Card {
            title: qsTr("Window")

            Row_ {
                label: qsTr("Show icon in system tray")
                hint: Tray.available ? qsTr("Control playback from the tray; left-click shows or hides Kanzi, middle-click plays or pauses.")
                                     : qsTr("No system tray found. On GNOME, install the AppIndicator extension.")
                Toggle {
                    enabled: Tray.available
                    checked: Tray.enabled
                    onToggled: Tray.enabled = checked
                }
            }
            Row_ {
                label: qsTr("Keep running in the tray when closed")
                hint: qsTr("Closing the window keeps the music playing. Quit from the tray menu or with Ctrl+Q.")
                Toggle {
                    enabled: Tray.active
                    checked: Tray.closeToTray
                    onToggled: Tray.closeToTray = checked
                }
            }
            Row_ {
                label: qsTr("Start in the tray")
                hint: qsTr("Launch without opening the main window.")
                Toggle {
                    enabled: Tray.active
                    checked: Tray.startInTray
                    onToggled: Tray.startInTray = checked
                }
            }
            Row_ {
                label: qsTr("Keep the mini player on top")
                hint: Tray.kwin || Qt.platform.pluginName !== "wayland"
                      ? qsTr("Keeps the mini player above other windows (Ctrl+M).")
                      : qsTr("Keeps the mini player above other windows (Ctrl+M). Wayland has no standard way to request this; it works on KDE Plasma (KWin) but may not on this compositor.")
                Toggle {
                    checked: Tray.miniPlayerOnTop
                    onToggled: Tray.miniPlayerOnTop = checked
                }
            }
        }

        // ---- account ----
        Card {
            title: qsTr("Account")

            Row_ {
                label: AppleMusic.authorized ? qsTr("Signed in to Apple Music") : qsTr("Not signed in")
                hint: AppleMusic.authorized ? qsTr("Storefront: %1 · Language: %2").arg(AppleMusic.storefront.toUpperCase()).arg(AppleMusic.language)
                                            : qsTr("Sign in to play full songs and see your library.")
                PillButton {
                    text: AppleMusic.authorized ? qsTr("Sign out") : qsTr("Sign in")
                    primary: !AppleMusic.authorized
                    enabled: Player.ready
                    onClicked: AppleMusic.authorized ? AppleMusic.signOut() : Player.authorize()
                }
            }
        }

        // ---- advanced ----
        Card {
            title: qsTr("Advanced")

            Row_ {
                label: qsTr("Developer token")
                hint: AppleMusic.status === AppleMusic.Error ? AppleMusic.errorString
                    : AppleMusic.status === AppleMusic.Loading ? qsTr("Fetching…")
                    : tokenField.text ? qsTr("Using your own token.")
                    : qsTr("Using the token from the Apple Music web player. Paste a MusicKit JWT to use your own.")
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                TextField {
                    id: tokenField
                    Layout.fillWidth: true
                    placeholderText: qsTr("eyJ… (leave empty for automatic)")
                    text: AppleMusic.developerTokenOverride()
                    echoMode: activeFocus ? TextInput.Normal : TextInput.Password
                    color: Theme.text
                    selectByMouse: true
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: Theme.hover
                        border.width: tokenField.activeFocus ? 2 : 0
                        border.color: Theme.accent
                    }
                }
                PillButton {
                    text: qsTr("Apply")
                    primary: false
                    onClicked: {
                        AppleMusic.setDeveloperTokenOverride(tokenField.text)
                        Nav.toast(qsTr("Developer token updated"))
                    }
                }
                PillButton {
                    text: qsTr("Refresh")
                    primary: false
                    visible: tokenField.text === ""
                    onClicked: AppleMusic.retry()
                }
            }
        }

        // ---- about ----
        Card {
            title: qsTr("About")
            Text {
                Layout.fillWidth: true
                text: qsTr("Kanzi %1 · Native Apple Music client · %2 backend").arg(Qt.application.version).arg(Qt.platform.pluginName)
                color: Theme.textDim
                wrapMode: Text.WordWrap
            }
        }
    }
}
