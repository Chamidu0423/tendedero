// Tendedero for Windows — main.cpp: entry point, message window, hotkey,
// single instance. Port of main.swift + AppDelegate bootstrap + HotKey.swift
// (Carbon RegisterEventHotKey → Win32 RegisterHotKey; no permissions needed).
#include "Common.h"
#include "Panel.h"
#include "Watchers.h"
#include "CaptureFlight.h"
#include <shellapi.h>
#include <dbt.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>

// --- crash diagnostics: if startup throws/dies, write %TEMP%\Tendedero.log
// so the user can tell us what happened instead of "nothing happens".
static FILE* g_log = nullptr;
void logf(const char* fmt, ...) {
    if (!g_log) {
        char path[MAX_PATH]; GetTempPathA(MAX_PATH, path);
        strcat_s(path, "Tendedero.log");
        fopen_s(&g_log, path, "a");
        if (g_log) { time_t t = time(nullptr); fprintf(g_log, "\n=== run %s", ctime(&t)); fflush(g_log); }
    }
    if (!g_log) return;
    va_list ap; va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}
static LONG WINAPI excFilter(EXCEPTION_POINTERS* e) {
    logf("UNHANDLED EXCEPTION code=0x%08lX addr=%p", e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}
struct ExcGuard { ExcGuard() { SetUnhandledExceptionFilter(excFilter); } };

void trayCreate(HINSTANCE hi, HWND messageWnd);
void trayDestroy();
void trayOnClick(WPARAM wp, LPARAM lp);
void trayRefreshTip();

static HWND     g_msg = nullptr;         // hidden message-only window
static HANDLE   g_mutex = nullptr;       // single-instance guard
static bool     g_hotkeyId = false;

// Parse "ctrl+alt+shift+win+a" → RegisterHotKey modifiers + VK.
static bool parseHotkey(const std::wstring& s, UINT& mods, UINT& vk) {
    mods = 0; vk = 0;
    size_t pos = 0;
    std::vector<std::wstring> parts;
    while (pos <= s.size()) {
        size_t next = s.find(L'+', pos);
        parts.push_back(s.substr(pos, next == std::wstring::npos ? std::wstring::npos : next - pos));
        if (next == std::wstring::npos) break;
        pos = next + 1;
    }
    if (parts.empty()) return false;
    std::wstring key = parts.back();
    for (size_t i = 0; i + 1 < parts.size(); i++) {
        std::wstring m = parts[i];
        for (auto& c : m) c = (wchar_t)towlower(c);
        if (m == L"ctrl")  mods |= MOD_CONTROL;
        else if (m == L"alt")   mods |= MOD_ALT;
        else if (m == L"shift") mods |= MOD_SHIFT;
        else if (m == L"win")   mods |= MOD_WIN;
    }
    if (key.size() == 1) {
        wchar_t c = (wchar_t)towupper(key[0]);
        if (c >= L'0' && c <= L'9') vk = c;            // 0x30-39 == '0'..'9'
        else if (c >= L'A' && c <= L'Z') vk = c;       // 0x41-5A == 'A'..'Z'
        else if (c == L' ') vk = VK_SPACE;
    } else {
        std::wstring lower = key; for (auto& ch : lower) ch = (wchar_t)towlower(ch);
        if (lower == L"space") vk = VK_SPACE;
        else if (lower.size() == 1) vk = (UINT)towupper(lower[0]);
    }
    return vk != 0;
}

static void applyHotkey() {
    if (g_hotkeyId) { UnregisterHotKey(g_msg, 1); g_hotkeyId = false; }
    UINT mods, vk;
    if (!g.hotkey.empty() && parseHotkey(g.hotkey, mods, vk))
        g_hotkeyId = RegisterHotKey(g_msg, 1, mods, vk) != 0;
}

// First-run prompt: "Let Tendedero handle your screenshots?" (port of the
// NSAlert in AppDelegate). Only when the user never answered (no registry val).
static void maybeAskInboxTakeover() {
    DWORD asked = regDword(HKEY_CURRENT_USER, kAppRegKey, L"InboxAsked", 0);
    if (asked) return;
    setRegDword(HKEY_CURRENT_USER, kAppRegKey, L"InboxAsked", 1);
    int r = MessageBoxW(nullptr,
        L("Screenshots will hang on the line the instant you take them, without the "
          "floating thumbnail, and will not pile up on your Desktop. Drag one to a "
          "folder to keep it, or discard it with the cross. You can turn this off "
          "from the menu bar, and your settings come back when Tendedero quits.").c_str(),
        L("Let Tendedero handle your screenshots?").c_str(),
        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1);
    g.handleScreenshots = (r == IDYES);
    g.save();
}

static LRESULT CALLBACK msgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_APP + 1:                       // tray callback
        trayOnClick(wp, lp);
        return 0;
    case WM_CLIPBOARDUPDATE:               // ClipboardWatcher port
        clipboardCheckNow();
        trayRefreshTip();
        return 0;
    case WM_HOTKEY:                        // "Bring the line down" shortcut
        if (wp == 1) { S.revealed = !S.revealed ? true : S.revealed;
                       S.revealed = true; S.inBandSince = 0; panelRefresh(); }
        return 0;
    case WM_DISPLAYCHANGE:                 // resolution / monitor changed
        panelRelayout();
        return 0;
    case WM_SETTINGCHANGE:
        if (lp && _wcsicmp((LPCWSTR)lp, L"Intl") == 0) initLocalization();
        return 0;
    case WM_QUERYENDSESSION:
        return TRUE;
    case WM_ENDSESSION:
        watchersStop(); trayDestroy(); panelDestroy();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Show the line briefly as a first-run hint (macOS reveals on hover of the
// menu bar; Windows has no equivalent band — so we pin+reveal for 3 s once).
static void firstRunHint() {
    DWORD done = regDword(HKEY_CURRENT_USER, kAppRegKey, L"FirstRunHint", 0);
    if (done) return;
    setRegDword(HKEY_CURRENT_USER, kAppRegKey, L"FirstRunHint", 1);
    S.revealed = true; S.pinned = true; panelRefresh();
    SetTimer(g_msg, 99, 3500, nullptr);   // WM_TIMER 99 → unpin in msgProc
}

int APIENTRY wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR, int) {
    ExcGuard excGuard;
    logf("startup: pid=%lu", GetCurrentProcessId());
    // Single instance (macOS relies on LaunchServices; Windows needs a mutex).
    g_mutex = CreateMutexW(nullptr, TRUE, L"Local\\TendederoSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) { logf("already running, exiting"); return 0; }

    // Win8.1+: per-monitor v2 DPI via manifest; this is the fallback path.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    g.load();
    initLocalization();
    logf("lang loaded, watchers folders ready");
    maybeAskInboxTakeover();

    panelCreate(hi);
    logf("panel created hwnd=%p", (void*)panelHwnd());

    WNDCLASSW wc = {};
    wc.lpfnWndProc = msgProc; wc.hInstance = hi; wc.lpszClassName = L"TendederoMessage";
    RegisterClassW(&wc);
    g_msg = CreateWindowExW(0, L"TendederoMessage", L"Tendedero", WS_POPUP,
                            0, 0, 0, 0, nullptr, nullptr, hi, nullptr);
    SetWindowLongPtrW(g_msg, GWLP_HWNDPARENT, 0);

    AddClipboardFormatListener(g_msg);     // replaces changeCount polling
    clipboardWatchStart(g_msg);
    applyHotkey();
    watchersStart();
    trayCreate(hi, g_msg);
    firstRunHint();
    logf("tray added, entering message loop");

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        if (m.message == WM_TIMER && m.hwnd == g_msg && m.wParam == 99) { // end hint
            KillTimer(g_msg, 99); S.pinned = false; panelRefresh(); continue;
        }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    logf("message loop exited");

    if (g_hotkeyId) UnregisterHotKey(g_msg, 1);
    RemoveClipboardFormatListener(g_msg);
    watchersStop();
    trayDestroy();
    panelDestroy();
    g.save();
    CoUninitialize();
    return 0;
}
