# Tendedero for Windows

A port of the macOS menu-bar app that hangs your screenshots on a clothesline
at the top of the screen. C++17 / Win32 / GDI+, built with mingw-w64 or MSVC.

## Build (Linux cross-compile or MSYS2)
    ./build.sh          # -> build/Tendedero.exe (statically linked, x64)

Build deps: `mingw-w64-x86-64-gcc`, `mingw-w64-x86-64-windres`.
Native Windows: use MSYS2/MinGW64 shell with the same script, or open
`src/*.cpp` + `res/app.rc` in Visual Studio (link gdiplus, shell32, ole32,
uuid, comdlg32, shlwapi, advapi32; set subsystem to Windows).

## Run
Double-click `Tendedero.exe`. It lives in the system tray (no taskbar window).
Take a screenshot (Win+Shift+S or PrtScn saved to Pictures\Screenshots) and it
flies up and hangs on the line above your apps.

Gestures on a hanging capture:
- Click ........ copy to clipboard
- Double-click . open in default viewer (Photos)
- Hold ......... open Paint 3D / MS Paint markup (`ms-paint:` URI fallback)
- Force/drag ... drag out to keep (copy), drop on trash = delete
- Right-click .. context menu (copy, reveal, remove, pin)

Tray menu: pause/resume, keep-on-line limit, language, hotkey, quit.

## Feature parity vs macOS — honest status
Everything below is implemented in this port: clothesline layout & sag math,
pegs/glass frames, breeze animation, capture flight animation, folder watcher
(ReadDirectoryChangesW), clipboard watcher, drag-out via OLE IDropTarget source,
tray + hotkey (RegisterHotKey), 11-language localization, "takeover" mode
(disables Snipping Tool thumbnail via registry, restored on exit).

Not 100% identical, by OS constraints:
- SwiftUI vibrancy/glass materials → approximated with layered GDI+ alpha art.
- Quick Look (spacebar preview) → replaced with default-app open; no equivalent.
- Apple Markup editor → replaced with Paint via ms-paint: URI (quality differs).
- kMDItemIsScreenCapture xattr tagging → Windows has no per-file screenshot
  flag; we detect by filename pattern + Pictures\Screenshots location instead.
- Per-Space window pinning (macOS private API) → Windows virtual desktops have
  no public API; window stays topmost across all desktops.
- Sounds (NSSound) → MessageBeep fallback only.

Runtime behavior was NOT verified on real Windows hardware from this Linux CI
environment; the binary links cleanly and PE headers validate. Please test on
Windows 10/11 and report issues.
