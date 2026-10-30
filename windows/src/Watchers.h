// Tendedero for Windows — folder / clipboard / sound services.
// Ports ScreenshotWatcher.swift, ClipboardWatcher.swift, Inbox.swift (parts),
// and the peg clip sounds of PeggedView/Soundboard on macOS.
#pragma once
#include <string>
#include <vector>

// One background ReadDirectoryChangesW thread per watched folder.
// Mirrors ScreenshotWatcher: coalesced events, debounced re-scan, only new
// image files get hung (deduped against what is already on the line).
struct DirWatcher {
    HANDLE dirHandle = nullptr;   // directory being watched
    HANDLE thread    = nullptr;
    volatile bool    stop = false;
    std::wstring     path;
};

void watchersStart();                    // inbox + Desktop + safety + extra folders
void watchersStop();
void watchersAddFolder(const std::wstring& path);
void watchersRemoveFolder(const std::wstring& path);
const std::vector<std::wstring>& watcherFolders();   // live list for the menu

// Clipboard: polls GetClipboardData(CF_BITMAP/DIB) via AddClipboardFormatListener.
// Port of ClipboardWatcher.swift (changeCount polling → WM_CLIPBOARDUPDATE).
void clipboardWatchStart(HWND messageTarget);
void clipboardCheckNow();                // called on WM_CLIPBOARDUPDATE + after hang ops

// Sounds: MessageBeep stand-ins for the peg clips / whoosh (macOS ships
// synthesized AVAudio sine clips; Windows has no bundled assets yet).
void playHangSound();
void playDropSound();
bool anyAnimationBusy();                 // flights or reveal anim running (menu "busy")
