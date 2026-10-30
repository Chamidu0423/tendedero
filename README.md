<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/images/hero-dark.png">
  <img src="docs/images/hero-light.png" alt="Tendedero. Screenshots, hung out to dry. Three screenshots in glass frames hang from a thin line under the top of the screen.">
</picture>

<p align="center">
  <b>Tendedero for Windows — version 1.0</b>
  <br>
  Free and open source. For Windows 10 and 11 (x64).
  <br>
  A port of the macOS menu-bar app. Same clothesline, now on your PC.
</p>

<br>

## Out of sight. Within reach.

Every screenshot you take hangs on a line just above your screen.
Rest the pointer at the top edge and it glides down. Move away and it's gone.
No taskbar clutter. No Desktop pile-up. Just a line with your captures on it.

<br>

## A gesture for everything.

| | |
|:--|:--|
| Click | Copy the image to the clipboard. |
| Double click | Open it in your default viewer (Photos). |
| Press and hold | Open it in Paint for mark-up. |
| Drag into an app | Send a copy. It stays on the line. |
| Drag into a folder | Keep it there. It leaves the line. |
| Drag to the Recycle Bin, or click the cross | Let it go. |
| Right-click | Copy · Reveal · Remove · Pin. |
| Rest the pointer at the top edge | Bring the line down. |
| Tray icon | Pause, keep-on-line limit, language, shortcut, quit. |
| <kbd>Ctrl</kbd>&thinsp;<kbd>Alt</kbd>&thinsp;<kbd>T</kbd> | Show or hide the line. |

<br>

## Your Desktop. Finally clear.

Hand Tendedero your screenshots and they skip the Desktop entirely.
Each capture flies up and hangs the instant you take it, and only what you
drag out is kept. On first run it offers to take over: it turns off the
Snipping Tool floating thumbnail while it runs, and puts your settings back
when it quits.

<br>

## Copied images too.

Copy any image — <kbd>Ctrl</kbd>+<kbd>PrtScn</kbd>, or Ctrl+C in an app — and
it hangs on the line like any other when *Hang copied images* is on in the
tray menu. Only images: anything copied along with text or files is left alone.

<br>

## Private by design.

No account. No network. No analytics.
Tendedero runs entirely on your PC, and your screenshots never leave it.

<br>

## Tech Specs

| | |
|:--|:--|
| **Compatibility** | Windows 10 / 11, x64 |
| **Size** | ~350 KB single .exe, no installer, no dependencies |
| **Languages** | English, Spanish, Galician, French, Portuguese (Brazil), Japanese, Korean, Chinese (Simplified), Turkish, Azerbaijani, Hindi |
| **Built with** | C++17, Win32, GDI+ (mingw-w64 or MSVC) |
| **Network access** | None |
| **Price** | Free |
| **License** | MIT for the code. The name and icon are not included. |

<br>

## Install

Grab `Tendedero.exe` from the [v1.0 release](../../releases/tag/v1.0-windows)
and double-click it. It lives in the system tray — no window, no installer,
no admin rights. To start it with Windows, drop a shortcut into
`shell:startup`.

<br>

## Build from source

```sh
git clone <this repo> && cd <repo> && git checkout windows/v1.0.0
cd windows
./build.sh            # -> build/Tendedero.exe
```

Requires mingw-w64 (`x86_64-w64-mingw32-g++` + `windres`) on Linux or MSYS2.
On native Windows you can also compile `src/*.cpp` plus `res/app.rc` in
Visual Studio — link `gdiplus shell32 ole32 uuid comdlg32 shlwapi advapi32
user32 gdi32 comctl32`, set the subsystem to *Windows*.

Source layout:

| File | Role |
|:--|:--|
| `windows/src/main.cpp` | WinMain, tray icon, message loop |
| `windows/src/Panel.cpp` | The layered top-of-screen strip; reveal/hide state machine |
| `windows/src/Line.cpp/.h` | What is hanging, rope sag math, layout |
| `windows/src/CaptureFlight.cpp` | Fly-up-and-hang animation |
| `windows/src/Watchers.cpp` | Screenshot folder watcher + clipboard watcher |
| `windows/src/Menu.cpp` | Tray menu, context menu, takeover settings |
| `windows/src/Localization.cpp` | 11-language string loading (`windows/strings/*.txt`) |
| `windows/res/app.rc` | Icon, manifest, version info (1.0.0) |

<br>

## How faithful is this port?

Honest answer: functionally equivalent, not pixel-identical — some macOS
features have no Windows twin. See [`windows/README-WINDOWS.md`](windows/README-WINDOWS.md)
for the full parity table. Highlights: SwiftUI vibrancy → GDI+ alpha art;
Quick Look → default-app open; Apple Markup → Paint; per-Space pinning →
topmost across virtual desktops.

Runtime QA on real Windows hardware is ongoing for v1.0 — please report
issues on the tracker.

<br>
<br>

<p align="center">
  <img src="docs/images/icon.png" width="64" height="64" alt="">
  <br>
  <sub>The code is MIT licensed. The Tendedero name and icon are not, so forks need their own. See <a href="LICENSE">LICENSE</a>.</sub>
  <br>
  <sub>Original macOS app designed and built by <a href="https://alejandrobujan.com">Alejandro Buján</a>. Windows port maintained here.</sub>
</p>
