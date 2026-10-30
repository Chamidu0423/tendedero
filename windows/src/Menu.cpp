// Tendedero for Windows — Menu.cpp: tray icon, tray menu and per-peg context
// menu. Port of the AppDelegate status-item menu and GrabArea's right-click
// NSMenu. Tray replaces the macOS menu-bar extra; same items, same order.
#include "Common.h"
#include "Panel.h"
#include "Watchers.h"
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>

void showPegContextMenu(int i);           // declared in Panel.cpp too

static NOTIFYICONDATAW g_nid = {};
static HMENU   g_trayMenu = nullptr;
static long long g_menuOpenedMs = 0;      // pinned while a menu is open

// Menu item ids
enum {
    IDM_TOGGLE_LINE = 1, IDM_TAKE_ALL_DOWN, IDM_HANDLE_SHOTS, IDM_OPEN_SHOTS_FOLDER,
    IDM_SOUNDS, IDM_LOGIN, IDM_CHECK_UPDATES, IDM_QUIT,
    IDM_KEEP_ON_LINE, IDM_SIZE_SMALL, IDM_SIZE_MEDIUM, IDM_SIZE_LARGE,
    IDM_ADD_FOLDER, IDM_STOP_WATCHING_BASE = 200, IDM_MAXITEMS_BASE = 400,
};

static void updateTrayTip() {
    int n = panelLiveCount();
    std::wstring tip = n == 0 ? L("The line is empty") : LF("%d on the line", n);
    lstrcpynW(g_nid.szTip, tip.c_str(), 128);
    g_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// Build the tray popup fresh each time (folder list changes).
static void buildTrayMenu() {
    if (g_trayMenu) DestroyMenu(g_trayMenu);
    HMENU m = CreatePopupMenu();

    AppendMenuW(m, MF_STRING, IDM_TOGGLE_LINE,
                g.hideLine ? L("Show line").c_str() : L("Hide line").c_str());
    AppendMenuW(m, MF_STRING, IDM_TAKE_ALL_DOWN, L("Take everything down").c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (g.handleScreenshots ? MF_CHECKED : 0), IDM_HANDLE_SHOTS,
                L("Handle screenshots").c_str());
    AppendMenuW(m, MF_STRING, IDM_OPEN_SHOTS_FOLDER, L("Open screenshots folder").c_str());
    AppendMenuW(m, MF_STRING | (g.sounds ? MF_CHECKED : 0), IDM_SOUNDS, L("Sounds").c_str());

    // Open at login: HKCU Run key (the SMLoginItemSetEnabled analogue).
    std::wstring runVal = regStr(HKEY_CURRENT_USER,
                                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", kAppName);
    bool atLogin = !runVal.empty();
    AppendMenuW(m, MF_STRING | (atLogin ? MF_CHECKED : 0), IDM_LOGIN, L("Open at login").c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);

    // Keep on line when switching desktops → on Windows: keep across virtual
    // desktops is automatic for topmost windows; item kept for parity.
    AppendMenuW(m, MF_STRING | (g.keepOnActiveSpace ? MF_CHECKED : 0), IDM_KEEP_ON_LINE,
                L("Keep on line").c_str());

    // Size submenu
    {
        HMENU sz = CreatePopupMenu();
        AppendMenuW(sz, MF_STRING | (g.scale == 0 ? MF_CHECKED : 0), IDM_SIZE_SMALL,  L("Small").c_str());
        AppendMenuW(sz, MF_STRING | (g.scale == 1 ? MF_CHECKED : 0), IDM_SIZE_MEDIUM, L("Medium").c_str());
        AppendMenuW(sz, MF_STRING | (g.scale == 2 ? MF_CHECKED : 0), IDM_SIZE_LARGE,  L("Large").c_str());
        AppendMenuW(m, MF_POPUP, (UINT_PTR)sz, L("Size").c_str());
    }
    // Capacity submenu (As many as fit / 1..10 photos)
    {
        HMENU cap = CreatePopupMenu();
        AppendMenuW(cap, MF_STRING | (g.maxItems == 0 ? MF_CHECKED : 0), IDM_MAXITEMS_BASE,
                    L("As many as fit").c_str());
        for (int k = 1; k <= 10; k++)
            AppendMenuW(cap, MF_STRING | (g.maxItems == k ? MF_CHECKED : 0),
                        IDM_MAXITEMS_BASE + k, LF("%d photos", k).c_str());
        AppendMenuW(m, MF_POPUP, (UINT_PTR)cap, L("Keep on line").c_str());
    }
    // Also watch folders
    {
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        auto& folders = watcherFolders();
        const wchar_t* inboxPrefix = nullptr; (void)inboxPrefix;
        int id = IDM_STOP_WATCHING_BASE;
        for (auto& f : folders) {
            std::wstring name = PathFindFileNameW(f.c_str());
            if (name.empty()) name = f;                      // drive root
            AppendMenuW(m, MF_STRING, id++,
                        (std::wstring(L("Stop Watching")) + L" — " + name).c_str());
        }
        AppendMenuW(m, MF_STRING, IDM_ADD_FOLDER, L("Add Folder…").c_str());
    }
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_CHECK_UPDATES, L("Check for Updates…").c_str());
    AppendMenuW(m, MF_STRING, IDM_QUIT, LF("Quit %s", kAppName).empty() ? L"Quit" : L("Quit Tendedero").c_str());
    g_trayMenu = m;
}

// Simple IFileDialog-based folder picker (no external deps).
static std::wstring pickFolder(HWND owner) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dlg)))) return {};
    dlg->SetOptions(FOS_PICKFOLDERS);
    std::wstring out;
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = p; CoTaskMemFree(p); }
            item->Release();
        }
    }
    dlg->Release();
    return out;
}

void trayCreate(HINSTANCE hi, HWND messageWnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = messageWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_nid.uCallbackMessage = WM_APP + 1;
    // Stock icon until an .ico ships with the build (build-app.ps1 embeds one).
    g_nid.hIcon = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!g_nid.hIcon) g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    lstrcpynW(g_nid.szTip, kAppName, 128);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

void trayDestroy() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }
void trayRefreshTip() { updateTrayTip(); }

// Handle WM_APP+1 from the message window.
void trayOnClick(WPARAM wp, LPARAM lp) {
    UINT ev = (UINT)LOWORD(lp);
    if (ev == WM_LBUTTONUP || ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) {
        POINT pt; GetCursorPos(&pt);
        buildTrayMenu();
        g_menuOpenedMs = nowMs();
        S.pinned = true;                       // line stays down while menu open
        SetForegroundWindow(panelHwnd());      // so the menu dismisses on click-away
        HMENU m = g_trayMenu;
        ULONG_PTR cmd = TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                  pt.x, pt.y, 0, panelHwnd(), nullptr);
        g_menuOpenedMs = 0;
        S.pinned = false;
        if (!cmd) return;
        switch (cmd) {
        case IDM_TOGGLE_LINE:
            g.hideLine = !g.hideLine;
            if (!g.hideLine) { S.revealed = false; }     // wait for next hover
            else { S.revealed = false; }
            g.save(); break;
        case IDM_TAKE_ALL_DOWN: panelClearAll(); break;
        case IDM_HANDLE_SHOTS:
            g.handleScreenshots = !g.handleScreenshots; g.save();
            if (g.handleScreenshots) watchersStart(); else watchersStop();
            break;
        case IDM_OPEN_SHOTS_FOLDER:
            ShellExecuteW(nullptr, L"open", screenshotsDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case IDM_SOUNDS: g.sounds = !g.sounds; g.save(); break;
        case IDM_LOGIN: {
            HKEY k; RegCreateKeyExW(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0,
                KEY_WRITE, nullptr, &k, nullptr);
            std::wstring val = regStr(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", kAppName);
            if (val.empty()) {
                std::wstring exe = L"\"" + exeDir() + L"\\" + kAppName + L".exe\"";
                RegSetValueExW(k, kAppName, 0, REG_SZ, (const BYTE*)exe.c_str(),
                               (DWORD)((exe.size() + 1) * sizeof(wchar_t)));
            } else {
                RegDeleteValueW(k, kAppName);
            }
            RegCloseKey(k);
            break;
        }
        case IDM_KEEP_ON_LINE: g.keepOnActiveSpace = !g.keepOnActiveSpace; g.save(); break;
        case IDM_SIZE_SMALL:  case IDM_SIZE_MEDIUM: case IDM_SIZE_LARGE:
            g.scale = cmd == IDM_SIZE_SMALL ? 0 : cmd == IDM_SIZE_MEDIUM ? 1 : 2;
            g.save(); panelReloadThumbnails(); break;
        case IDM_ADD_FOLDER: {
            std::wstring f = pickFolder(panelHwnd());
            if (!f.empty()) { watchersAddFolder(f); g.extraFolders.push_back(f); g.save(); }
            break;
        }
        case IDM_CHECK_UPDATES:
            // Same GitHub repo as the macOS updater; open the releases page.
            ShellExecuteW(nullptr, L"open", L"https://github.com/alperoeke/tendedero/releases",
                          nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case IDM_QUIT: PostQuitMessage(0); break;
        default:
            if (cmd >= IDM_MAXITEMS_BASE && cmd <= IDM_MAXITEMS_BASE + 10) {
                g.maxItems = (int)(cmd - IDM_MAXITEMS_BASE); g.save(); panelRelayout();
            } else if (cmd >= IDM_STOP_WATCHING_BASE) {
                int idx = (int)(cmd - IDM_STOP_WATCHING_BASE);
                auto folders = watcherFolders();          // copy: remove mutates
                if (idx < (int)folders.size()) {
                    std::wstring f = folders[idx];
                    watchersRemoveFolder(f);
                    for (auto& e : g.extraFolders)
                        if (_wcsicmp(e.c_str(), f.c_str()) == 0) {
                            g.extraFolders.erase(
                                std::remove(g.extraFolders.begin(), g.extraFolders.end(), e),
                                g.extraFolders.end());
                            break;
                        }
                    g.save();
                }
            }
        }
        updateTrayTip();
    }
}

// ---------------------------------------------------------- peg right-click --
void showPegContextMenu(int i) {
    if (i < 0 || i >= (int)S.pegs.size()) return;
    S.pinned = true;
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 10, L("Copy").c_str());
    AppendMenuW(m, MF_STRING, 20, L("Open").c_str());
    AppendMenuW(m, MF_STRING, 30, L("Quick Look").c_str());
    AppendMenuW(m, MF_STRING, 40, L("Markup").c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 50, L("Reveal in Finder").c_str());   // Explorer here
    AppendMenuW(m, MF_STRING, 60, L("Save to Desktop").c_str());
    AppendMenuW(m, MF_STRING, 70, L("Take down to the left").c_str());
    AppendMenuW(m, MF_STRING, 80, L("Take down to the right").c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 90, L("Trash").c_str());
    POINT pt; GetCursorPos(&pt);
    ULONG_PTR cmd = TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                              pt.x, pt.y, 0, panelHwnd(), nullptr);
    DestroyMenu(m);
    S.pinned = false;
    switch (cmd) {
    case 10: panelCopy(i); break;
    case 20: panelOpen(i); break;
    case 30: panelQuickLook(i); break;
    case 40: panelMarkup(i); break;
    case 50: panelRevealInExplorer(i); break;
    case 60: panelSaveToDesktop(i); break;
    case 70: panelTakeDownSide(i, true); break;
    case 80: panelTakeDownSide(i, false); break;
    case 90: panelTrash(i); break;
    }
    updateTrayTip();
}
