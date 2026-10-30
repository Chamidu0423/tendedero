// Tendedero for Windows — shared constants, settings and small helpers.
// Port of the macOS app (Sources/Tendedero). See PORTING.md for differences.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <knownfolders.h>   // FOLDERID_Desktop / Pictures via shell32 uuid
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>

// USING_ALIASES: bring the few std names the port uses unqualified into scope
using std::min; using std::max; using std::fabs;
inline double floor_(double v){return std::floor(v);}

// App identity -------------------------------------------------------------
inline const wchar_t* kAppRegKey      = L"Software\\Tendedero";
inline const wchar_t* kAppName        = L"Tendedero";
inline const char*    kUpdaterRepo    = "alperoeke/tendedero"; // GitHub owner/repo, same as macOS

// Timing constants — identical numbers to AppDelegate.swift ----------------
inline constexpr UINT  kTickMs            = 33;    // 30 Hz cursor poll
inline constexpr int   kMenuBarBandPx     = 48;    // reveal band at top of screen
inline constexpr double kRevealDelayS     = 0.25;  // dwell in band before drop
inline constexpr double kRetractDelayS    = 0.5;   // outside before pull-up
inline constexpr double kDoubleClickExtraS = 0.02; // tiny grace over system dbl-click

// Layout — the exact numbers of enum Layout in LineView.swift ---------------
// Size: small/medium/large -> scale 0.8 / 1 / 1.35
inline double layoutSizeScale(int scale) { return scale == 0 ? 0.8 : scale == 2 ? 1.35 : 1.0; }
inline double layoutPhotoMaxHeight(int scale) { return std::floor(104 * layoutSizeScale(scale) + 0.5); }
inline double layoutCardWidth(int scale)       { return std::floor(150 * layoutSizeScale(scale) + 0.5); }
inline double layoutSpacing(int scale)         { return layoutCardWidth(scale) + 24; }
inline double layoutPanelHeight(int scale)     { return layoutPhotoMaxHeight(scale) + 106; }
inline constexpr double kRopeTop   = 10;   // Layout.ropeTop
inline constexpr double kPinAbove  = 9.5;  // Layout.pinAbove
inline constexpr double kFrameRadius = 16; // Frame.radius
inline constexpr double kFrameInset  = 4;  // Frame.inset
inline double layoutSag(double width)        { return std::min(30.0, width * 0.018); }
inline double layoutRopeY(double x, double width) {
    if (width <= 0) return kRopeTop;
    double f = x / width;
    return kRopeTop + 4 * layoutSag(width) * f * (1 - f);
}
// Breeze period, like scheduleGust(): random 7...16 seconds.
inline constexpr double kGustEveryMinS = 7.0;
inline constexpr double kGustEveryMaxS = 16.0;
// Random tilt per photo, like Pegged.tilt: -2.5...2.5 degrees.

// Localization — port of Localization.swift + Localizable.strings ----------
void         initLocalization();                  // Localization.cpp
std::wstring L(const char* key);                 // Localization.cpp
template <typename... Args>
std::wstring LF(const char* key, Args&&... args);// printf-style formatting

// Settings (HKCU\Software\Tendedero) ---------------------------------------
struct Settings {
    bool     hideLine       = false;
    bool     sounds         = true;
    bool     keepOnActiveSpace = true;   // "Keep on line when switching desktops"
    bool     handleScreenshots = true;   // inbox mode ON by default on Windows
    int      scale          = 1;         // 0 small, 1 medium, 2 large
    int      maxItems       = 0;         // 0 = as many as fit, else cap
    std::vector<std::wstring> extraFolders;
    std::wstring hotkey;                 // serialized: "ctrl+alt+shift+win+a"
    std::wstring language;               // "" = follow system

    void load();
    void save() const;
};
extern Settings g;

// Small helpers -------------------------------------------------------------
std::wstring utf8to16(const std::string& s);
std::string  utf16to8(const std::wstring& s);
std::wstring regStr(HKEY root, const wchar_t* sub, const wchar_t* name, const std::wstring& def = L"");
void         setRegStr(HKEY root, const wchar_t* sub, const wchar_t* name, const std::wstring& v);
DWORD        regDword(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD def = 0);
void         setRegDword(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD v);
bool         fileExists(const std::wstring& p);
std::wstring exeDir();
std::wstring envPath(const wchar_t* var, const std::wstring& fallback);
std::wstring desktopDir();
std::wstring picturesDir();
std::wstring screenshotsDir();           // %USERPROFILE%\Pictures\Screenshots
std::wstring tendederoInboxDir();        // %LOCALAPPDATA%\Tendedero\Inbox
std::wstring formatW(const wchar_t* fmt, ...);
long long    nowMs();                    // steady ms for animation clocks
