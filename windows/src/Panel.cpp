// Tendedero for Windows — Panel.cpp: the clothesline panel window.
// Port of LinePanel.swift (borderless non-activating always-on-top panel →
// layered topmost WS_EX_TOOLWINDOW), the reveal/retract state machine of
// AppDelegate.swift (30 Hz cursor tick, 0.25 s dwell / 0.5 s leave), and the
// gesture routing of GrabArea.swift (click = copy, double-click = open,
// hold = markup, drag = file drag, cross = take down, wheel = scroll).
#include "Common.h"
#include "Panel.h"
#include "Watchers.h"
#include "CaptureFlight.h"
#include <gdiplus.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>
#include <cmath>
#include <random>
#include <algorithm>

using namespace Gdiplus;

void showPegContextMenu(int pegIndex);   // Menu.cpp (tray/context menus)

PanelState S;
RECT g_captureArea = {};

static HWND   g_hwnd      = nullptr;
static HINSTANCE g_inst   = nullptr;
static ULONG_PTR g_gdiToken = 0;
static bool   g_dragging  = false;   // an OLE file drag is in progress (busy)
static double g_panelW    = 1280;    // client width in px
static double g_panelH    = 210;     // Layout.panelHeight * scale-ish

// ---------------------------------------------------------------- helpers ---
static void setClickThrough(bool through) {
    LONG ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    if (through) ex |= WS_EX_TRANSPARENT; else ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
}

bool anyAnimationBusy() {
    return flightsInProgress() > 0 || g_dragging || (S.revealAnim > 0.0 && S.revealAnim < 1.0);
}

int panelLiveCount() {
    int n = 0;
    for (size_t i = 0; i < S.pegs.size(); i++)
        if (i < S.fallRemoveAt.size() ? S.fallRemoveAt[i] == 0 : true) n++;
    return n;
}

bool panelIsInInbox(int i) {
    std::wstring inbox = tendederoInboxDir() + L"\\";
    const auto& p = S.pegs[i].path;
    return _wcsnicmp(p.c_str(), inbox.c_str(), (unsigned)inbox.size()) == 0;
}

std::vector<int> panelNeighbours(int i, bool toTheLeft) {
    // like neighboursOf(): the two cards on that side, innermost first
    std::vector<int> out;
    if (toTheLeft) { if (i - 1 >= 0) out.push_back(i - 1); if (i - 2 >= 0) out.push_back(i - 2); }
    else { if ((size_t)i + 1 < S.pegs.size()) out.push_back(i + 1);
           if ((size_t)i + 2 < S.pegs.size()) out.push_back(i + 2); }
    return out;
}

void panelRefresh() { if (g_hwnd) InvalidateRect(g_hwnd, nullptr, FALSE); }

HWND panelHwnd() { return g_hwnd; }

// Visible count: as many as fit (Layout capacity), or the maxItems cap.
static int visibleCapacity() {
    int spacingI = (int)layoutSpacing(g.scale);
    int fits = max(1, (int)((g_panelW - 60) / spacingI));
    int vis = fits;
    if (g.maxItems > 0) vis = min(vis, g.maxItems);
    return vis;
}

void panelRelayout() {
    RECT rc; GetClientRect(g_hwnd, &rc);
    g_panelW = rc.right; g_panelH = rc.bottom;
    bool hasKeep = (int)S.pegs.size() > visibleCapacity();
    layoutRope(S.pegs, g_panelW, g.scale, visibleCapacity(), S.scroll, hasKeep);
    panelRefresh();
}

// Screen rect of a card (for flight targets / drag feedback).
RECT panelPegScreenRect(int i) {
    RECT wr; GetWindowRect(g_hwnd, &wr);
    const Peg& p = S.pegs[i];
    RECT r = { wr.left + (LONG)(p.x + 4), wr.top + (LONG)p.y,
               wr.left + (LONG)(p.x + p.frameW - 4), wr.top + (LONG)(p.y + p.frameH) };
    return r;
}

// ------------------------------------------------------------ hang / drop ---
static bool pathAlreadyHung(const std::wstring& path) {
    for (auto& p : S.pegs) if (_wcsicmp(p.path.c_str(), path.c_str()) == 0) return true;
    return false;
}

void hangPath(const std::wstring& path, bool flyingFromCaptureArea, RECT captureRect) {
    if (path.empty() || pathAlreadyHung(path)) return;

    Peg peg;
    peg.path = path;
    peg.addedMs = nowMs();
    double maxPix = layoutPhotoMaxHeight(g.scale) * 3;   // generous thumb, like macOS
    if (!loadPegImage(peg, path, maxPix)) { freePeg(peg); return; }  // not an image / unreadable

    // keep newest at the right; trim overflow when "as many as fit"
    S.pegs.push_back(peg);
    S.fallRemoveAt.resize(S.pegs.size(), 0);
    size_t cap = (size_t)visibleCapacity();
    while (S.pegs.size() > cap + 8) { // hard safety so memory can't grow unbounded
        freePeg(S.pegs.front());
        S.pegs.erase(S.pegs.begin());
        S.fallRemoveAt.erase(S.fallRemoveAt.begin());
    }
    panelRelayout();

    int idx = (int)S.pegs.size() - 1;
    if (flyingFromCaptureArea) {
        // CaptureFlight: from the capture flash area to the card slot.
        RECT from = captureRect;
        if (from.right <= from.left) from = g_captureArea;
        if (from.right <= from.left) {
            POINT pt; GetCursorPos(&pt);
            SetRect(&from, pt.x - 220, pt.y - 150, pt.x + 220, pt.y + 150);
        }
        S.pegs[idx].flying = true;
        RECT to = panelPegScreenRect(idx);
        startFlight(S.pegs[idx].bitmap, from, to, S.pegs[idx].rot * 180 / 3.14159265358979,
                    /*falling=*/false, [idx, path]() {
            // land: clear flying, spring-settle already handled by gust model
            for (auto& p : S.pegs)
                if (!_wcsicmp(p.path.c_str(), path.c_str())) { p.flying = false; break; }
            panelRefresh();
        });
    }
    if (g.sounds) playHangSound();
    // "Bring the line down: when something new hangs"
    if (!g.hideLine) { S.revealed = true; S.inBandSince = 0; }
    panelRefresh();
}

void panelDrop(int i, bool quietly) {
    if (i < 0 || i >= (int)S.pegs.size()) return;
    S.fallRemoveAt.resize(S.pegs.size(), 0);
    if (S.fallRemoveAt[i]) return;                       // already falling
    RECT to = panelPegScreenRect(i);
    Bitmap* img = S.pegs[i].bitmap;
    startFlight(img, to, to, S.pegs[i].rot * 180 / 3.14159265358979,
                /*falling=*/true, [i, quietly]() {
        if (i < (int)S.pegs.size()) {
            freePeg(S.pegs[i]);
            S.pegs.erase(S.pegs.begin() + i);
            if (i < (int)S.fallRemoveAt.size())
                S.fallRemoveAt.erase(S.fallRemoveAt.begin() + i);
            panelRelayout();
        }
        if (!quietly && g.sounds) playDropSound();
    });
    S.fallRemoveAt[i] = nowMs() + 600;                    // remove after 0.55 s fall (+slack)
    panelRefresh();
}

void panelDiscard(int i) {
    // cross button: trash inbox copies (they came from our takeover folder),
    // otherwise just take it off the line (the file stays where it is).
    if (i < 0 || i >= (int)S.pegs.size()) return;
    if (panelIsInInbox(i)) {
        std::wstring p = S.pegs[i].path;
        std::wstring from = p;
        from.push_back(L'\0'); from.push_back(L'\0');  // double-null terminate for SHFileOperation
        SHFILEOPSTRUCTW op = {};
        op.wFunc = FO_DELETE;
        op.pFrom = from.c_str();
        op.fFlags = FOF_ALLOWUNDO | FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI;
        SHFileOperationW(&op);
    }
    panelDrop(i, false);
}

// ------------------------------------------------------------- operations ---
static bool writeBitmapToClipboard(Bitmap* bmp) {
    if (!bmp) return false;
    // CF_DIB: serialize the GDI+ bitmap into a DIB via GetHBITMAP + GetObject.
    HBITMAP h = nullptr;
    if (bmp->GetHBITMAP(Color(0, 0, 0, 0), &h) != Ok) return false;
    DIBSECTION ds;
    if (!GetObjectW(h, sizeof(ds), &ds)) { DeleteObject(h); return false; }
    BITMAPINFOHEADER bih = ds.dsBmih;
    bih.biHeight = abs(bih.biHeight);
    DWORD sizeImage = (DWORD)bih.biSizeImage;
    if (!sizeImage) sizeImage = bih.biWidth * bih.biHeight * (bih.biBitCount / 8);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(bih) + sizeImage);
    if (!mem) { DeleteObject(h); return false; }
    BYTE* p = (BYTE*)GlobalLock(mem);
    memcpy(p, &bih, sizeof(bih));
    // bottom-up DIB: copy rows flipped
    BYTE* bits = (BYTE*)ds.dsBm.bmBits;
    int rowBytes = (int)(bih.biWidth * (bih.biBitCount / 8));
    for (LONG y = 0; y < bih.biHeight; y++)
        memcpy(p + sizeof(bih) + (size_t)y * rowBytes,
               bits + (size_t)(bih.biHeight - 1 - y) * rowBytes, rowBytes);
    GlobalUnlock(mem);
    DeleteObject(h);
    if (!OpenClipboard(g_hwnd)) { GlobalFree(mem); return false; }
    EmptyClipboard();
    SetClipboardData(CF_DIB, mem);          // ownership moves to clipboard
    CloseClipboard();
    return true;
}

void panelCopy(int i) {
    if (i < 0 || i >= (int)S.pegs.size()) return;
    const Peg& p = S.pegs[i];
    if (!p.path.empty() && PathFileExistsW(p.path.c_str())) {
        // Put the real file (CF_HDROP) plus a CF_DIB on the clipboard, so both
        // file targets and image-only targets get a paste, like macOS does.
        size_t bytes = sizeof(DROPFILES) + (p.path.size() + 2) * sizeof(wchar_t);
        HGLOBAL hgl = GlobalAlloc(GMEM_MOVEABLE, bytes);
        DROPFILES* df = (DROPFILES*)GlobalLock(hgl);
        df->pFiles = sizeof(DROPFILES);
        df->fWide = TRUE;
        wcscpy_s((wchar_t*)((BYTE*)df + sizeof(DROPFILES)), p.path.size() + 2, p.path.c_str());
        GlobalUnlock(hgl);
        if (OpenClipboard(g_hwnd)) {
            // Remember a DIB too so image-only paste targets work.
            writeBitmapToClipboard(p.bitmap);
            OpenClipboard(g_hwnd); EmptyClipboard();
            SetClipboardData(CF_HDROP, hgl); CloseClipboard();
        }
    } else {
        writeBitmapToClipboard(p.bitmap);
    }
    panelRefresh();
}

void panelOpen(int i) {
    if (i < 0 || i >= (int)S.pegs.size()) return;
    const auto& p = S.pegs[i].path;
    if (!p.empty()) ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void panelTrash(int i) { panelDiscard(i); }   // same semantics as macOS menu Trash

void panelMarkup(int i) {
    // macOS uses the built-in Markup editor; Windows opens Paint (closest
    // stock editor) — documented difference in PORTING.md.
    if (i < 0 || i >= (int)S.pegs.size()) return;
    const auto& p = S.pegs[i].path;
    if (p.empty()) return;
    std::wstring paint = envPath(L"SystemRoot", L"C:\\Windows") + L"\\System32\\mspaint.exe";
    std::wstring args = L"\"" + p + L"\"";
    ShellExecuteW(nullptr, L"open", paint.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL);
}

void panelQuickLook(int i) {
    // No QLPreviewPanel on Windows: Space-style peek → default viewer.
    panelOpen(i);
}

void panelRevealInExplorer(int i) {
    if (i < 0 || i >= (int)S.pegs.size()) return;
    const auto& p = S.pegs[i].path;
    if (p.empty()) return;
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(p.c_str());
    if (pidl) { SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0); ILFree(pidl); }
}

void panelSaveToDesktop(int i) {
    if (i < 0 || i >= (int)S.pegs.size()) return;
    const auto& src = S.pegs[i].path;
    if (src.empty()) return;
    std::wstring dst = desktopDir() + L"\\" + PathFindFileNameW(src.c_str());
    if (!_wcsicmp(src.c_str(), dst.c_str())) return;         // already there
    CopyFileW(src.c_str(), dst.c_str(), FALSE);
}

void panelTakeDownSide(int i, bool toTheLeft) {
    // "Take down to the left/right": re-hang this photo next to its neighbour.
    if (i < 0 || i >= (int)S.pegs.size()) return;
    Peg moved = S.pegs[i];
    S.pegs.erase(S.pegs.begin() + i);
    if (i < (int)S.fallRemoveAt.size()) S.fallRemoveAt.erase(S.fallRemoveAt.begin() + i);
    int target = toTheLeft ? i - 1 : i;
    target = max(0, min(target, (int)S.pegs.size()));
    S.pegs.insert(S.pegs.begin() + target, moved);
    panelRelayout();
}

void panelClearAll() {
    // Take everything down: cascade falls like the macOS stagger.
    for (int i = (int)S.pegs.size() - 1; i >= 0; i--)
        if (!S.fallRemoveAt[i]) panelDrop(i, true);
}

void panelScroll(double delta) {
    bool hasKeep = (int)S.pegs.size() > visibleCapacity();
    if (!hasKeep) return;
    double spacing = layoutSpacing(g.scale);
    double span = ((double)S.pegs.size() - visibleCapacity()) * spacing;
    S.scroll = max(-span, min(0.0, S.scroll + delta * spacing));
    panelRelayout();
}

void panelReloadThumbnails() {
    double maxPix = layoutPhotoMaxHeight(g.scale) * 3;
    for (auto& p : S.pegs)
        if (!p.path.empty()) loadPegImage(p, p.path, maxPix);
    panelRelayout();
}

void panelPrune() {
    for (int i = (int)S.pegs.size() - 1; i >= 0; i--)
        if (!S.pegs[i].path.empty() && !fileExists(S.pegs[i].path))
            panelDrop(i, true);
}

void panelShowHint(bool) { /* drawn inline in paint when the line is empty */ }

// --------------------------------------------------------------- hit tests ---
static bool ptInRect(double x, double y, const RectF& r) {
    return x >= r.X && x <= r.X + r.Width && y >= r.Y && y <= r.Y + r.Height;
}

bool panelPointInPegs(POINT screenPt) {
    if (!S.revealed) return false;
    RECT wr; GetWindowRect(g_hwnd, &wr);
    double x = screenPt.x - wr.left, y = screenPt.y - wr.top;
    for (size_t i = 0; i < S.pegs.size(); i++) {
        if (S.fallRemoveAt[i]) continue;
        if (S.pegs[i].flying) continue;
        if (ptInRect(x, y, pegFrameRect(S.pegs[i]))) return true;
    }
    return false;
}

int panelHitTest(POINT screenPt, bool* onCross) {
    if (onCross) *onCross = false;
    if (!S.revealed) return -1;
    RECT wr; GetWindowRect(g_hwnd, &wr);
    double x = screenPt.x - wr.left, y = screenPt.y - wr.top;
    for (int i = (int)S.pegs.size() - 1; i >= 0; i--) {   // topmost last drawn
        if (S.fallRemoveAt[i] || S.pegs[i].flying) continue;
        const Peg& p = S.pegs[i];
        if (ptInRect(x, y, pegFrameRect(p))) {
            if (ptInRect(x, y, pegCrossRect(p))) { if (onCross) *onCross = true; }
            return i;
        }
    }
    return -1;
}

void panelSetCursorShape(int hit) {
    SetCursor(hit >= 0 ? LoadCursor(nullptr, IDC_HAND) : LoadCursor(nullptr, IDC_ARROW));
}

// ---------------------------------------------------------------- painting ---
// Layered-window paint: draw into a 32bpp DIB with premultiplied alpha and
// upload with UpdateLayeredWindow — the Windows stand-in for the transparent
// borderless NSPanel of LinePanel.swift.
static void paintLayered(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    if (W <= 0 || H <= 0) return;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC screen = GetDC(hwnd);
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP old = (HBITMAP)SelectObject(mem, dib);

    {
        Graphics gfx(mem);
        gfx.Clear(Color(0, 0, 0, 0));
        REAL alpha = (REAL)S.revealAnim;                 // 0 hidden .. 1 down
        drawRope(gfx, g_panelW, alpha);
        for (size_t i = 0; i < S.pegs.size(); i++) {
            const Peg& p = S.pegs[i];
            if (p.flying) continue;                       // drawn as a flight instead
            Matrix save; gfx.GetTransform(&save);
            gfx.TranslateTransform(0, -(1.0f - alpha) * (float)g_panelH);  // slide in
            drawPeg(gfx, p, (int)i == S.hoverPeg, (int)i == S.pressPeg,
                    (int)i == S.hoverPeg);                // cross appears on hover
            gfx.SetTransform(&save);
        }
        // Empty-line hint: "The line is empty" centred under the rope.
        if (S.pegs.empty() && S.revealAnim > 0.5) {
            FontFamily ff(L"Segoe UI");
            Font font(&ff, 13, FontStyleRegular, UnitPixel);
            StringFormat sf; sf.SetAlignment(StringAlignmentCenter);
            std::wstring hint = L("The line is empty");
            PointF pt((REAL)(g_panelW / 2), 26);
            SolidBrush br(Color(150, 60, 60, 65));
            gfx.DrawString(hint.c_str(), -1, &font, pt, &sf, &br);
        }
        RECT wr; GetWindowRect(hwnd, &wr);
        drawActiveFlights(gfx, wr.left, wr.top);          // screen -> panel coords
    }

    RECT outer; GetWindowRect(hwnd, &outer);
    POINT pos = { outer.left, outer.top };
    SIZE sz = { (LONG)W, (LONG)H };
    POINT src = { 0, 0 };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd, screen, &pos, &sz, mem, &src, 0, &bf, 0);

    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(hwnd, screen);
}

// ------------------------------------------------------- reveal state mach ---
// AppDelegate.swift tick(): band = top kMenuBarBandPx of the primary screen;
// dwell kRevealDelayS → drop (spring); leave kRetractDelayS → pull up
// (easeIn), unless pinned/busy/hideLine.
void panelTick() {
    POINT pt; GetCursorPos(&pt);
    // Band = top of the *screen* (not work area): on Windows the taskbar is
    // usually at the bottom, so the trigger lives under the hidden auto-
    // reveal zone at the very top edge — same feel as macOS's menu bar band.
    RECT sr = {0}; MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY), &mi)) sr = mi.rcMonitor;
    else { sr.left = 0; sr.top = 0; sr.right = GetSystemMetrics(SM_CXSCREEN); sr.bottom = GetSystemMetrics(SM_CYSCREEN); }
    bool inBand = pt.y <= sr.top + kMenuBarBandPx;
    long long now = nowMs();

    if (inBand) {
        S.outBandSince = 0;
        if (!S.inBandSince) S.inBandSince = now;
        if (!S.revealed && !g.hideLine &&
            (double)(now - S.inBandSince) >= kRevealDelayS * 1000.0) {
            S.revealed = true;
        }
    } else {
        S.inBandSince = 0;
        if (!S.outBandSince) S.outBandSince = now;
        bool busy = anyAnimationBusy();
        if (S.revealed && !S.pinned && !busy &&
            (double)(now - S.outBandSince) >= kRetractDelayS * 1000.0) {
            S.revealed = false;
        }
    }

    // Animate reveal toward target (spring-ish ease-out on drop, ease-in up).
    double target = S.revealed ? 1.0 : 0.0;
    if (S.revealAnim != target) {
        double rate = S.revealed ? 6.5 : 8.0;   // per second
        double step = rate * (kTickMs / 1000.0);
        if (S.revealAnim < target) S.revealAnim = min(target, S.revealAnim + step);
        else                       S.revealAnim = max(target, S.revealAnim - step);
        // overshoot charm on landing (like .smooth/bouncy springs)
        panelRefresh();
    }

    // Breeze gusts — scheduleGust(): every 7..16 s nudge curRot/springV.
    if (S.nextGustMs == 0 || now >= S.nextGustMs) {
        static std::mt19937 rng((unsigned)GetTickCount64() ^ 0x5EED);
        std::uniform_real_distribution<double> period(kGustEveryMinS, kGustEveryMaxS);
        std::uniform_real_distribution<double> kick(-0.03, 0.03);
        S.nextGustMs = now + (long long)(period(rng) * 1000.0);
        for (auto& p : S.pegs) if (!p.flying) p.springV += kick(rng);
    }
    // Integrate the sway spring (damped): θ'' = -ω²θ - cθ'
    bool moving = false;
    for (auto& p : S.pegs) {
        if (fabs(p.curRot) > 1e-5 || fabs(p.springV) > 1e-5) {
            double w2 = 18.0, c = 2.2, dt = kTickMs / 1000.0;
            p.springV += (-w2 * p.curRot - c * p.springV) * dt;
            p.curRot  += p.springV * dt;
            moving = true;
        }
    }
    if (moving) panelRefresh();

    // Click-through when the pointer is not over any card (dynamic
    // ignoresMouseEvents port).
    setClickThrough(!panelPointInPegs(pt) && S.revealAnim < 0.99);

    // Flights: advance, repaint, reap completions; prune vanished files.
    if (tickFlights() || flightsInProgress()) panelRefresh();
    reapFinishedFlights();
    static long long lastPrune = 0;
    if (now - lastPrune > 2000) { lastPrune = now; panelPrune(); }
}

// ------------------------------------------------------------------ input ---
// GrabArea gestures: tap = copy, double-tap = open, long-press (0.45 s) =
// markup, drag (4 px) = OLE file drag, cross click = discard, wheel = scroll.
static int   g_downPeg = -1;
static bool  g_downOnCross = false;
static POINT g_downPt = {};
static DWORD g_downTime = 0;
static bool  g_holdFired = false;
static bool  g_dragStarted = false;
static DWORD g_lastClickTime = 0;
static int   g_lastClickPeg = -1;

class TendederoDropSource : public IDropSource, public IDataObject {
    LONG ref_ = 1;
public:
    // IDropSource — effect copy/move/delete masks like NSSession draggingMask
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** o) override {
        if (riid == IID_IUnknown || riid == IID_IDropSource) *o = (IDropSource*)this;
        else if (riid == IID_IDataObject) *o = (IDataObject*)this;
        else return E_NOINTERFACE;
        AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++ref_; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = --ref_; if (!r) delete this; return r;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL esc, DWORD kb) override {
        if (esc) return DRAGDROP_S_CANCEL;
        if (!(kb & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    // IDataObject — expose CF_HDROP with the single file path
    struct Payload { DROPFILES* df; HGLOBAL hgl; std::wstring path; };
    static Payload& payload(IDataObject* self) {
        static thread_local Payload pl; return pl;
    }
    void setPath(const std::wstring& p) {
        auto& pl = payload(this); pl.path = p;
        size_t bytes = sizeof(DROPFILES) + (p.size() + 2) * sizeof(wchar_t);
        pl.hgl = GlobalAlloc(GMEM_MOVEABLE, bytes);
        pl.df = (DROPFILES*)GlobalLock(pl.hgl);
        pl.df->pFiles = sizeof(DROPFILES); pl.df->fWide = TRUE;
        wcscpy_s((wchar_t*)((BYTE*)pl.df + sizeof(DROPFILES)), p.size() + 2, p.c_str());
        GlobalUnlock(pl.hgl);
    }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* fe, STGMEDIUM* m) override {
        ZeroMemory(m, sizeof(*m));
        if (fe->cfFormat == RegisterClipboardFormatW(L"FileGroupDescriptorW") ||
            fe->cfFormat == RegisterClipboardFormatW(L"FileContents")) return DV_E_FORMATETC;
        if (fe->cfFormat != CF_HDROP) return DV_E_FORMATETC;
        auto& pl = payload(this);
        m->tymed = TYMED_HGLOBAL;
        m->hGlobal = GlobalAlloc(GMEM_MOVEABLE, GlobalSize(pl.hgl));
        memcpy(GlobalLock(m->hGlobal), GlobalLock(pl.hgl), GlobalSize(pl.hgl));
        GlobalUnlock(m->hGlobal); GlobalUnlock(pl.hgl);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_OUTOFMEMORY; }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* fe) override {
        return fe->cfFormat == CF_HDROP ? S_OK : DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override {
        out->ptd = nullptr; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dir, IEnumFORMATETC** e) override {
        FORMATETC ft = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return SHCreateStdEnumFmtEtc(1, &ft, e);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }
};

static void startFileDrag(int i) {
    if (i < 0 || i >= (int)S.pegs.size()) return;
    std::wstring path = S.pegs[i].path;
    if (path.empty() || !fileExists(path)) return;
    auto* ds = new TendederoDropSource();
    ds->setPath(path);
    ds->AddRef(); // DoDragDrop holds one ref
    DWORD effect = 0;
    g_dragging = true;
    DoDragDrop(ds, ds, DROPEFFECT_COPY | DROPEFFECT_MOVE, &effect);
    g_dragging = false;
    ds->Release();
    if (effect == DROPEFFECT_MOVE) {
        // moved away → the watcher/prune will see the file gone; drop it now.
        panelDrop(i, true);
    }
    panelRefresh();
}

static LRESULT CALLBACK panelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT:
        paintLayered(hwnd);
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT;                 // never own title bars/borders
    case WM_MOUSEMOVE: {
        if (g_downPeg >= 0 && !g_dragStarted && !g_holdFired) {
            POINT nowPt; GetCursorPos(&nowPt);
            if (abs(nowPt.x - g_downPt.x) + abs(nowPt.y - g_downPt.y) > 4) {
                int i = g_downPeg;
                g_dragStarted = true;
                ReleaseCapture();
                startFileDrag(i);
                g_downPeg = -1; S.pressPeg = -1; panelRefresh();
                return 0;
            }
        }
        POINT pt; GetCursorPos(&pt);
        bool onCross = false;
        int hit = panelHitTest(pt, &onCross);
        if (hit != S.hoverPeg) { S.hoverPeg = hit; panelRefresh(); }
        panelSetCursorShape(hit);
        // long-press timer → markup (0.45 s hold, no movement)
        if (g_downPeg >= 0 && !g_holdFired && !g_dragStarted) {
            if (GetTickCount() - g_downTime >= 450) {
                g_holdFired = true;
                panelMarkup(g_downPeg);
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt; GetCursorPos(&pt);
        bool onCross = false;
        int hit = panelHitTest(pt, &onCross);
        g_downPeg = hit; g_downOnCross = onCross;
        g_downPt = pt; g_downTime = GetTickCount();
        g_holdFired = false; g_dragStarted = false;
        if (hit >= 0) { S.pressPeg = hit; panelRefresh(); }
        SetCapture(hwnd);
        return 0;
    }
    case WM_LBUTTONUP: {
        ReleaseCapture();
        POINT pt; GetCursorPos(&pt);
        int hit = g_downPeg;
        S.pressPeg = -1;
        if (g_dragStarted) { g_dragStarted = false; g_downPeg = -1; panelRefresh(); return 0; }
        if (hit >= 0 && !g_holdFired) {
            DWORD nowT = GetTickCount();
            if (g_downOnCross) {
                panelDiscard(hit);
            } else if (nowT - g_lastClickTime <= GetDoubleClickTime() && g_lastClickPeg == hit) {
                panelOpen(hit);                      // double-click → open
                g_lastClickTime = 0;
            } else {
                panelCopy(hit);                      // single click → copy
                g_lastClickTime = nowT; g_lastClickPeg = hit;
            }
        }
        g_downPeg = -1;
        panelRefresh();
        return 0;
    }
    case WM_MBUTTONDOWN: {
        POINT pt; GetCursorPos(&pt);
        int hit = panelHitTest(pt, nullptr);
        if (hit >= 0) panelQuickLook(hit);           // force-click stand-in
        return 0;
    }
    case WM_RBUTTONDOWN: {
        POINT pt; GetCursorPos(&pt);
        int hit = panelHitTest(pt, nullptr);
        if (hit >= 0) showPegContextMenu(hit);
        return 0;
    }
    case WM_MOUSEWHEEL:
        panelScroll(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1);
        return 0;
    case WM_TIMER:
        if (wp == 1) panelTick();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

// ---------------------------------------------------------------- create ----
void panelCreate(HINSTANCE hi) {
    g_inst = hi;
    GdiplusStartupInput gsi; GdiplusStartup(&g_gdiToken, &gsi, nullptr);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = panelProc;
    wc.hInstance = hi;
    wc.lpszClassName = L"TendederoLinePanel";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    // Full monitor rect: when the taskbar is at the bottom (default), the
    // work area starts at y=0 anyway, but with a top taskbar we must still
    // be able to park the panel above y=0 and reveal over the screen edge.
    RECT sr = {0}; MONITORINFO mi = { sizeof(mi) };
    POINT origin = {0, 0};
    if (GetMonitorInfoW(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &mi)) sr = mi.rcMonitor;
    else { sr.right = GetSystemMetrics(SM_CXSCREEN); sr.bottom = GetSystemMetrics(SM_CYSCREEN); }
    int W = sr.right - sr.left;
    int H = (int)layoutPanelHeight(g.scale) + 24;
    g_panelW = W; g_panelH = H;

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"TendederoLinePanel", L"Tendedero",
        WS_POPUP, sr.left, sr.top - H /*hidden above*/, W, H,
        nullptr, nullptr, hi, nullptr);

    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA); // initial; ULW paints replace
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    SetTimer(g_hwnd, 1, kTickMs, nullptr);
    setClickThrough(true);
    S.revealAnim = 0; S.revealed = false;
}

void panelDestroy() {
    if (g_hwnd) DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
    if (g_gdiToken) { GdiplusShutdown(g_gdiToken); g_gdiToken = 0; }
}
