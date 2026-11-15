# Kanzi

A modern, native Apple Music client for Linux, built with Qt 6 / QML. It runs natively on Wayland.

- Native QML interface: sidebar navigation, Home (recently played, heavy rotation, recommendations), Browse (charts), Search (catalog and library), library Albums / Artists / Songs / Playlists / Recently Added, album, playlist and artist pages
- Full-window Now Playing view with a blurred artwork backdrop, time-synced lyrics (click a line to seek) and Up Next
- Queue panel, Play Next / Play Later, shuffle, repeat, volume, and seeking
- Mini player (Ctrl+M): a frameless, resizable artwork window with controls that you can keep on top
- System tray icon with playback menu, close-to-tray and start-in-tray
- Crossfade (0–12 s) and AutoMix, which ends songs early once they fade into silence
- Discord Rich Presence ("Listening to …" with cover art, progress and a link). Uses Kanzi's own Discord application by default; you can paste a different ID in Settings
- MPRIS2 (`org.mpris.MediaPlayer2.kanzi`): media keys, desktop media widgets and lock screen controls
- Choose the audio output device in Settings (PulseAudio/PipeWire). It switches live, no restart needed
- Follows the system light or dark colour scheme
- Artwork disk cache

## How it works

```
┌──────────── QML UI (native) ────────────┐
│ Sidebar · Pages · PlayerBar · NowPlaying │
└───────┬─────────────────────┬────────────┘
        │ AppleMusic (C++)    │ Player (C++) ──── MPRIS2 over D-Bus
        │ REST → api.music.   │   ▲ │ QWebChannel
        │ apple.com           │   │ ▼
        │                 hidden WebEngineView running MusicKit JS
        │                 (Widevine DRM decode → PulseAudio/PipeWire)
```

Apple Music audio is Widevine-encrypted. The only supported way to decode it on Linux is MusicKit JS inside a Chromium engine. Kanzi keeps one invisible `WebEngineView` (`qml/PlayerHost.qml` + `resources/player.html`) whose only job is playback. Everything you see is QML, and all browsing and metadata goes straight to the Apple Music REST API from C++ (`src/applemusic.cpp`).

The page loads with a `https://music.apple.com/` base URL so that the web player's developer token and CORS rules accept it.

## Requirements

- Qt ≥ 6.8 with: Base, Declarative, QuickControls2, WebEngine (built with proprietary codecs, which Arch's build has), WebChannel, Wayland, DBus
- A Widevine CDM. Kanzi looks for Google Chrome's (`/opt/google/chrome/WidevineCdm/...`) and Chromium's copy. To point it at another one, set `KANZI_WIDEVINE_PATH=/path/to/libwidevinecdm.so`.
- libpulse (PulseAudio or PipeWire-pulse), used for audio output selection
- An Apple Music subscription for full-length playback

## Install (Arch Linux)

```sh
git clone https://github.com/cptn-cosmo/kanzi && cd kanzi/packaging/arch
makepkg -si
```

The PKGBUILD builds the tagged GitHub release. For playback you also need a Widevine CDM, for example the AUR `widevine` package.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/kanzi
# optional
sudo cmake --install build
```

## Developer token

By default Kanzi takes the public developer token from the music.apple.com web bundle and caches it until it expires. To use a token minted from your own MusicKit key instead, do either of these:

```sh
KANZI_DEVELOPER_TOKEN=eyJ... ./build/kanzi
# or paste it in Settings → Advanced → Developer token
```

## Keyboard

| Key | Action |
| --- | --- |
| Space | Play / pause |
| Ctrl+← / Ctrl+→ | Previous / next |
| Ctrl+↑ / Ctrl+↓ | Volume |
| Ctrl+F, Ctrl+L | Search |
| Ctrl+M | Mini player (double-click or Esc to return) |
| Esc, Alt+← | Close overlay / go back |
| Shift+wheel | Scroll a shelf sideways |

## Layout

```
src/applemusic.*      REST client, developer-token bootstrap, TTML lyrics parser
src/player.*          Playback state + command bridge to MusicKit JS
src/mpris.*           MPRIS2 adaptors
src/discordpresence.* Discord Rich Presence over the local IPC socket
src/tray.*            Tray icon and window preferences
src/audiooutput.*     Output device selection (moves our pulse streams to the chosen sink)
resources/player.html MusicKit host page (loaded hidden)
qml/                  UI: Main, Theme, Nav, components/, pages/
```

## License

MIT, see [LICENSE](LICENSE).
