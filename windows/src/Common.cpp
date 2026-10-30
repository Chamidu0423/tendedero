// Tendedero for Windows — app-wide state, settings persistence, helpers.
#include "Common.h"
#include <shlobj.h>
#include <cstdarg>
#include <cstdio>

Settings g;

std::wstring utf8to16(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}
std::string utf16to8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring regStr(HKEY root, const wchar_t* sub, const wchar_t* name, const std::wstring& def) {
    HKEY k; if (RegOpenKeyExW(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return def;
    wchar_t buf[4096]; DWORD cb = sizeof(buf), ty = 0;
    std::wstring out = def;
    if (RegQueryValueExW(k, name, nullptr, &ty, (BYTE*)buf, &cb) == ERROR_SUCCESS && ty == REG_SZ)
        out = buf;
    RegCloseKey(k);
    return out;
}
void setRegStr(HKEY root, const wchar_t* sub, const wchar_t* name, const std::wstring& v) {
    HKEY k; if (RegCreateKeyExW(root, sub, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)v.data(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}
DWORD regDword(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD def) {
    HKEY k; if (RegOpenKeyExW(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return def;
    DWORD v = def, cb = sizeof(v), ty = 0;
    if (RegQueryValueExW(k, name, nullptr, &ty, (BYTE*)&v, &cb) != ERROR_SUCCESS) v = def;
    RegCloseKey(k);
    return v;
}
void setRegDword(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD v) {
    HKEY k; if (RegCreateKeyExW(root, sub, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    RegCloseKey(k);
}

bool fileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}
std::wstring exeDir() {
    wchar_t buf[MAX_PATH]; GetModuleFileNameW(nullptr, buf, MAX_PATH);
    PathRemoveFileSpecW(buf); return buf;
}
std::wstring envPath(const wchar_t* var, const std::wstring& fallback) {
    wchar_t buf[32768]; DWORD n = GetEnvironmentVariableW(var, buf, 32767);
    return n && n < 32767 ? std::wstring(buf) : fallback;
}
static std::wstring knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &p)) || !p) return {};
    std::wstring s(p); CoTaskMemFree(p); return s;
}
std::wstring desktopDir()     { auto d = knownFolder(FOLDERID_Desktop);  return d.empty() ? envPath(L"USERPROFILE", L"C:") + L"\\Desktop" : d; }
std::wstring picturesDir()    { auto d = knownFolder(FOLDERID_Pictures); return d.empty() ? envPath(L"USERPROFILE", L"C:") + L"\\Pictures" : d; }
std::wstring screenshotsDir() { return picturesDir() + L"\\Screenshots"; }
std::wstring tendederoInboxDir() { return envPath(L"LOCALAPPDATA", L".") + L"\\Tendedero\\Inbox"; }

std::wstring formatW(const wchar_t* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    wchar_t buf[4096]; int n = _vsnwprintf_s(buf, 4096, _TRUNCATE, fmt, ap);
    va_end(ap);
    return n < 0 ? std::wstring() : std::wstring(buf, n);
}
long long nowMs() { return (long long)GetTickCount64(); }

// Settings persistence ------------------------------------------------------
static std::wstring joinFolders(const std::vector<std::wstring>& v) {
    std::wstring s; for (size_t i = 0; i < v.size(); i++) { if (i) s += L"|"; s += v[i]; } return s;
}
static std::vector<std::wstring> splitFolders(const std::wstring& s) {
    std::vector<std::wstring> out; size_t pos = 0;
    while (pos <= s.size()) {
        size_t next = s.find(L'|', pos);
        std::wstring part = s.substr(pos, next == std::wstring::npos ? std::wstring::npos : next - pos);
        if (!part.empty()) out.push_back(part);
        if (next == std::wstring::npos) break;
        pos = next + 1;
    }
    return out;
}
void Settings::load() {
    hideLine          = regDword(HKEY_CURRENT_USER, kAppRegKey, L"HideLine", 0) != 0;
    sounds            = regDword(HKEY_CURRENT_USER, kAppRegKey, L"Sounds", 1) != 0;
    keepOnActiveSpace = regDword(HKEY_CURRENT_USER, kAppRegKey, L"KeepOnLine", 1) != 0;
    handleScreenshots = regDword(HKEY_CURRENT_USER, kAppRegKey, L"Inbox", 1) != 0;
    scale             = (int)regDword(HKEY_CURRENT_USER, kAppRegKey, L"Scale", 1);
    maxItems          = (int)regDword(HKEY_CURRENT_USER, kAppRegKey, L"MaxItems", 0);
    extraFolders      = splitFolders(regStr(HKEY_CURRENT_USER, kAppRegKey, L"Folders"));
    hotkey            = regStr(HKEY_CURRENT_USER, kAppRegKey, L"Hotkey");
    language          = regStr(HKEY_CURRENT_USER, kAppRegKey, L"Language");
    if (scale < 0 || scale > 2) scale = 1;
}
void Settings::save() const {
    setRegDword(HKEY_CURRENT_USER, kAppRegKey, L"HideLine", hideLine);
    setRegDword(HKEY_CURRENT_USER, kAppRegKey, L"Sounds", sounds);
    setRegDword(HKEY_CURRENT_USER, kAppRegKey, L"KeepOnLine", keepOnActiveSpace);
    setRegDword(HKEY_CURRENT_USER, kAppRegKey, L"Inbox", handleScreenshots);
    setRegDword(HKEY_CURRENT_USER, kAppRegKey, L"Scale", (DWORD)scale);
    setRegDword(HKEY_CURRENT_USER, kAppRegKey, L"MaxItems", (DWORD)maxItems);
    setRegStr(HKEY_CURRENT_USER, kAppRegKey, L"Folders", joinFolders(extraFolders));
    setRegStr(HKEY_CURRENT_USER, kAppRegKey, L"Hotkey", hotkey);
    setRegStr(HKEY_CURRENT_USER, kAppRegKey, L"Language", language);
}
