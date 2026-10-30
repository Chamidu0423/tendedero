// Tendedero for Windows — Watchers.cpp.
// Port of ScreenshotWatcher.swift (kqueue DispatchSource → ReadDirectoryChangesW
// threads with debounced re-scan) and ClipboardWatcher.swift (changeCount poll
// → WM_CLIPBOARDUPDATE listener; saves image clips into the Inbox as PNG).
#include "Common.h"
#include "Watchers.h"
#include "Panel.h"
#include <gdiplus.h>
#include <shlobj.h>
#include <algorithm>
#include <unordered_set>
#include <atomic>
#include <vector>
#include <mutex>

using namespace Gdiplus;

// ------------------------------------------------------------- folder watch --
static std::vector<std::wstring>           g_folders;      // live list (menu)
static std::vector<DirWatcher*>            g_watchers;
struct MutLocker {
    CRITICAL_SECTION* cs;
    MutLocker(CRITICAL_SECTION& c): cs(&c) { EnterCriticalSection(cs); }
    ~MutLocker() { LeaveCriticalSection(cs); }
};
static CRITICAL_SECTION g_mtx_;
struct MtxInit { MtxInit(){ InitializeCriticalSection(&g_mtx_); } } g_mtxInit;
static std::unordered_set<std::wstring>    g_seen;         // paths already hung/seen
static std::atomic<bool>                   g_running{ false };

static bool isImageName(const std::wstring& name) {
    PCWSTR ext = PathFindExtensionW(name.c_str());
    static const wchar_t* kExts[] = { L".png", L".jpg", L".jpeg", L".gif", L".bmp", L".tiff", L".webp" };
    for (auto e : kExts) if (_wcsicmp(ext, e) == 0) return true;
    return false;
}

static void scanFolderOnce(const std::wstring& dir, bool firstScan) {
    // List images, hang anything unseen. On the very first scan we only record
    // what exists (like starting/listing/missed in ScreenshotWatcher) so old
    // files don't flood the line — except the Inbox, which is ours.
    WIN32_FIND_DATAW fd;
    std::wstring pat = dir + L"\\*";
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::wstring> found;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!isImageName(fd.cFileName)) continue;
        found.push_back(dir + L"\\" + fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    bool inbox = _wcsnicmp(dir.c_str(), tendederoInboxDir().c_str(),
                           wcslen(tendederoInboxDir().c_str())) == 0;
    for (auto& p : found) {
        bool isNew = g_seen.insert(p).second;
        if (!isNew) continue;
        if (firstScan && !inbox) continue;               // baseline only
        hangPath(p, /*flying=*/false, {});
    }
}

static DWORD WINAPI watchThread(LPVOID param) {
    DirWatcher* w = (DirWatcher*)param;
    wchar_t buf[8192];
    char overlayBuf[sizeof(OVERLAPPED)] = {};
    OVERLAPPED* ov = (OVERLAPPED*)overlayBuf;

    while (!w->stop) {
        DWORD bytes = 0;
        BOOL ok = ReadDirectoryChangesW(w->dirHandle, buf, sizeof(buf), FALSE,
                                        FILE_NOTIFY_CHANGE_FILE_NAME |
                                        FILE_NOTIFY_CHANGE_LAST_WRITE |
                                        FILE_NOTIFY_CHANGE_CREATION,
                                        nullptr, ov, nullptr);
        if (!ok) { Sleep(500); if (GetFileAttributesW(w->path.c_str()) == INVALID_FILE_ATTRIBUTES) break;
                   // reopen
                   w->dirHandle = CreateFileW(w->path.c_str(), FILE_LIST_DIRECTORY,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
                   continue; }
        // Wait on the handle for completion (we passed no completion key thread;
        // use GetOverlappedResult with a timeout loop honoring stop).
        LARGE_INTEGER deadline; GetSystemTimeAsFileTime((FILETIME*)&deadline);
        for (;;) {
            if (w->stop) { CancelIoEx(w->dirHandle, ov); break; }
            if (GetOverlappedResult(w->dirHandle, ov, &bytes, TRUE)) break;
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED || err == ERROR_INVALID_HANDLE) { bytes = 0; break; }
            Sleep(200);
        }
        if (w->stop) break;
        // Debounce like the macOS watcher: small settle delay then rescan.
        Sleep(150);
        scanFolderOnce(w->path, /*firstScan=*/false);
    }
    return 0;
}

static DirWatcher* startOne(const std::wstring& dir) {
    if (dir.empty() || !fileExists(dir)) return nullptr;
    HANDLE h = CreateFileW(dir.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) return nullptr;
    DirWatcher* w = new DirWatcher();
    w->dirHandle = h; w->path = dir;
    w->thread = CreateThread(nullptr, 0, watchThread, w, 0, nullptr);
    return w;
}

void watchersAddFolder(const std::wstring& path) {
    MutLocker lk(g_mtx_);
    for (auto& f : g_folders) if (_wcsicmp(f.c_str(), path.c_str()) == 0) return;
    g_folders.push_back(path);
    scanFolderOnce(path, /*firstScan=*/true);
    if (g_running) if (auto* w = startOne(path)) g_watchers.push_back(w);
    setRegStr(HKEY_CURRENT_USER, kAppRegKey, L"Folders", [&]{
        std::wstring s; for (size_t i = 0; i < g_folders.size(); i++) {
            // store only the extra folders (skip inbox/desktop defaults)
            std::wstring f = g_folders[i];
            if (_wcsicmp(f.c_str(), desktopDir().c_str()) == 0) continue;
            if (_wcsicmp(f.c_str(), screenshotsDir().c_str()) == 0) continue;
            if (_wcsnicmp(f.c_str(), tendederoInboxDir().c_str(),
                          wcslen(tendederoInboxDir().c_str())) == 0) continue;
            if (!s.empty()) s += L"|"; s += f;
        }
        return s; }());
}

void watchersRemoveFolder(const std::wstring& path) {
    MutLocker lk(g_mtx_);
    for (size_t i = 0; i < g_folders.size(); i++) {
        if (_wcsicmp(g_folders[i].c_str(), path.c_str()) == 0) {
            g_folders.erase(g_folders.begin() + i);
            break;
        }
    }
    for (size_t i = 0; i < g_watchers.size(); i++) {
        if (_wcsicmp(g_watchers[i]->path.c_str(), path.c_str()) == 0) {
            g_watchers[i]->stop = true;
            CancelIoEx(g_watchers[i]->dirHandle, nullptr);
            WaitForSingleObject(g_watchers[i]->thread, 2000);
            CloseHandle(g_watchers[i]->dirHandle);
            delete g_watchers[i];
            g_watchers.erase(g_watchers.begin() + i);
            break;
        }
    }
}

const std::vector<std::wstring>& watcherFolders() { return g_folders; }

void watchersStart() {
    g_running = true;
    // Default watched places (macOS: Desktop screenshot folder + inbox when
    // takeover is on; Windows: Pictures\\Screenshots + Desktop safety + inbox).
    std::vector<std::wstring> wanted;
    if (g.handleScreenshots) {
        std::wstring inbox = tendederoInboxDir();
        SHCreateDirectoryExW(nullptr, inbox.c_str(), nullptr);
        wanted.push_back(inbox);
    }
    if (fileExists(screenshotsDir())) wanted.push_back(screenshotsDir());
    wanted.push_back(desktopDir());                     // safety watcher
    for (auto& f : g.extraFolders) wanted.push_back(f);

    MutLocker lk(g_mtx_);
    for (auto& d : wanted) {
        bool have = false;
        for (auto& f : g_folders) if (_wcsicmp(f.c_str(), d.c_str()) == 0) have = true;
        if (!have) g_folders.push_back(d);
        scanFolderOnce(d, /*firstScan=*/true);
        if (auto* w = startOne(d)) g_watchers.push_back(w);
    }
}

void watchersStop() {
    g_running = false;
    MutLocker lk(g_mtx_);
    for (auto* w : g_watchers) {
        w->stop = true;
        CancelIoEx(w->dirHandle, nullptr);
        WaitForSingleObject(w->thread, 2000);
        CloseHandle(w->dirHandle);
        delete w;
    }
    g_watchers.clear();
}

// -------------------------------------------------------------- clipboard ---
static bool g_lastOwnClipboardChange = false;   // ignore our own panelCopy writes

static bool saveDibToPng(HGLOBAL hDib, const std::wstring& outPath) {
    if (!hDib) return false;
    BYTE* p = (BYTE*)GlobalLock(hDib);
    if (!p) return false;
    BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)p;
    Image* img = nullptr;
    // GDI+ can read a DIB straight from memory with a fake BITMAPFILEHEADER.
    size_t hdrSize = bih->biSize;
    if (hdrSize >= sizeof(BITMAPINFOHEADER)) {
        // Build "BM" file header (14 bytes) before the DIB.
        DWORD total = 14 + (DWORD)GlobalSize(hDib);
        HGLOBAL full = GlobalAlloc(GMEM_MOVEABLE, total);
        BYTE* f = (BYTE*)GlobalLock(full);
        f[0]='B'; f[1]='M';
        *(DWORD*)(f+2) = total; *(DWORD*)(f+10) = 14 + (DWORD)hdrSize;
        memcpy(f + 14, p, GlobalSize(hDib));
        GlobalUnlock(full);
        IStream* st = nullptr;
        if (SUCCEEDED(CreateStreamOnHGlobal(full, TRUE, &st))) {
            img = Image::FromStream(st);
            st->Release();
        }
        GlobalFree(full);
    }
    GlobalUnlock(hDib);
    if (!img || img->GetLastStatus() != Ok) { delete img; return false; }

    // Find the PNG encoder CLSID by MIME type.
    UINT encCount = 0, bufSize = 0;
    GetImageEncodersSize(&encCount, &bufSize);
    ImageCodecInfo* ici = (ImageCodecInfo*)malloc(bufSize ? sizeof(ImageCodecInfo) * encCount : 1);
    BYTE* props = (BYTE*)malloc(bufSize ? bufSize : 1);
    GetImageEncoders(encCount, bufSize, ici);
    CLSID pngClsid = {};
    bool found = false;
    for (UINT i = 0; i < encCount; i++)
        if (ici[i].MimeType && wcscmp(ici[i].MimeType, L"image/png") == 0) {
            pngClsid = ici[i].Clsid; found = true; break;
        }
    free(ici); free(props);
    bool ok = false;
    if (found) ok = img->Save(outPath.c_str(), &pngClsid, nullptr) == Ok;
    delete img;
    return ok;
}

static long long g_lastClipMs = 0;

void clipboardCheckNow() {
    if (!IsClipboardFormatAvailable(CF_DIB) && !IsClipboardFormatAvailable(CF_BITMAP)) return;
    // Ignore clips that contain a file we ourselves just copied (panelCopy sets
    // CF_HDROP + CF_DIB): the nspasteboard.org privacy analogue on macOS is
    // "reject our own pasteboard types"; here: skip if HDROP points at an
    // already-hung file.
    if (OpenClipboard(nullptr)) {
        HANDLE hdrop = GetClipboardData(CF_HDROP);
        bool skip = false;
        if (hdrop) {
            DROPFILES* df = (DROPFILES*)GlobalLock(hdrop);
            if (df) {
                WCHAR first[MAX_PATH * 2] = {};
                LPCWSTR p = (LPCWSTR)((BYTE*)df + df->pFiles);
                wcsncpy_s(first, p, MAX_PATH * 2 - 1);
                GlobalUnlock(hdrop);
                for (auto& peg : S.pegs)
                    if (!_wcsicmp(peg.path.c_str(), first)) { skip = true; break; }
            }
        }
        HANDLE hdib = GetClipboardData(CF_DIB);
        HGLOBAL copy = nullptr;
        if (!skip && hdib) {
            SIZE_T n = GlobalSize(hdib);
            copy = GlobalAlloc(GMEM_MOVEABLE, n);
            if (copy) {
                void* d = GlobalLock(copy); void* s = GlobalLock(hdib);
                memcpy(d, s, n); GlobalUnlock(hdib); GlobalUnlock(copy);
            }
        }
        CloseClipboard();
        if (skip || !copy) return;

        // Rate-limit bursts (a few hundred ms) like the macOS debounce.
        long long now = nowMs();
        if (now - g_lastClipMs < 400) { GlobalFree(copy); return; }
        g_lastClipMs = now;

        std::wstring inbox = tendederoInboxDir();
        SHCreateDirectoryExW(nullptr, inbox.c_str(), nullptr);
        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t name[128];
        wsprintfW(name, L"Clipboard %04d-%02d-%02d at %02d.%02d.%02d.png",
                  st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        std::wstring out = inbox + L"\\" + name;
        if (saveDibToPng(copy, out)) {
            GlobalFree(copy);
            hangPath(out, false, {});
            return;
        }
        GlobalFree(copy);
    }
}

void clipboardWatchStart(HWND) {
    // The message window receives WM_CLIPBOARDUPDATE (registered in main.cpp);
    // nothing else to set up beyond an initial sweep off the launch backlog.
}

// ---------------------------------------------------------------- sounds ---
// macOS plays short synthesized clip/whoosh samples (Soundboard). Windows port
// uses Beep() tones at matching cadence — subtle, behind the Sounds toggle.
void playHangSound() { Beep(1600, 28); }     // peg clip
void playDropSound()  { Beep(520, 70); }     // soft fall whoosh
