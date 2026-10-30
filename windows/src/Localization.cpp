// Tendedero for Windows — localization. Port of Localization.swift + the
// 11 Localizable.strings catalogs (strings/*.txt, same "key = value;" syntax).
#include "Common.h"
#include <map>
#include <shlobj.h>

static std::map<std::string, std::wstring> g_table;

static std::wstring unescape(const std::wstring& s) {
    std::wstring out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        wchar_t c = s[i];
        if (c == L'\\' && i + 1 < s.size()) {
            wchar_t n = s[++i];
            switch (n) {
                case L'n': out += L'\n'; break;
                case L't': out += L'\t'; break;
                case L'"': out += L'"';  break;
                case L'\\': out += L'\\'; break;
                default: out += n; break;
            }
        } else out += c;
    }
    return out;
}

// Parse one catalog file ("key" = "value"; with /* */ comments), UTF-8.
static bool loadCatalog(const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(f, nullptr), got = 0;
    std::string raw(size ? size : 1, '\0');
    ReadFile(f, raw.data(), size, &got, nullptr);
    CloseHandle(f);
    std::wstring w = utf8to16(raw);

    // strip /* ... */ comments
    for (;;) {
        size_t a = w.find(L"/*"); if (a == std::wstring::npos) break;
        size_t b = w.find(L"*/", a); if (b == std::wstring::npos) { w.erase(a); break; }
        w.erase(a, b - a + 2);
    }
    size_t pos = 0;
    while ((pos = w.find(L'"', pos)) != std::wstring::npos) {
        size_t k1 = pos + 1;
        size_t k2 = k1; while (k2 < w.size() && !(w[k2] == L'"' && w[k2-1] != L'\\')) k2++;
        size_t eq = w.find(L'=', k2); size_t v1 = w.find(L'"', eq);
        if (v1 == std::wstring::npos) break;
        size_t v2 = v1 + 1; while (v2 < w.size() && !(w[v2] == L'"' && w[v2-1] != L'\\')) v2++;
        g_table[utf16to8(unescape(w.substr(k1, k2 - k1)))] = unescape(w.substr(v1 + 1, v2 - v1 - 1));
        pos = w.find(L';', v2); if (pos == std::wstring::npos) break; pos++;
    }
    return true;
}

static std::wstring currentLanguageTag() {
    if (!g.language.empty()) return g.language;
    static std::wstring detected;
    if (!detected.empty()) return detected;
    wchar_t localeName[80] = {};
    GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SNAME, localeName, 80);
    std::wstring loc(localeName);            // e.g. "pt-BR", "zh-Hans-CN", "az-Latn-AZ"
    static const wchar_t* tags[] = { L"az", L"en", L"es", L"fr", L"gl", L"hi",
                                     L"ja", L"ko", L"pt-BR", L"tr", L"zh-Hani-CN" };
    for (auto tag : tags) {
        if (_wcsicmp(loc.c_str(), tag) == 0) { detected = tag; return detected; }
        size_t tl = wcslen(tag);
        if (loc.size() > tl && loc[tl] == L'-' && _wcsnicmp(loc.c_str(), tag, tl) == 0) {
            detected = tag; return detected;
        }
    }
    // fall back to the neutral part (e.g. "zh" -> zh-Hani-CN handled above)
    detected = L"en";
    return detected;
}

void initLocalization() {
    std::wstring dir = exeDir() + L"\\strings";
    loadCatalog(dir + L"\\" + currentLanguageTag() + L".txt");
    loadCatalog(dir + L"\\en.txt");   // fallback for missing keys
}

std::wstring L(const char* key) {
    auto it = g_table.find(key);
    return it != g_table.end() ? it->second : utf8to16(key);
}

template <typename... Args>
std::wstring LF(const char* key, Args&&... args) {
    std::wstring fmt = L(key);
    return formatW(fmt.c_str(), std::forward<Args>(args)...);
}
// Explicit instantiations covering every call form in the port (lvalues and
// rvalues of int / const wchar_t*, 1..2 args):
template std::wstring LF<int>(const char*, int&&);
template std::wstring LF<int&>(const char*, int&);
template std::wstring LF<const wchar_t*>(const char*, const wchar_t*&&);
template std::wstring LF<const wchar_t*&>(const char*, const wchar_t*&);
template std::wstring LF<const wchar_t*&, const wchar_t*>(const char*, const wchar_t*&, const wchar_t*&&);
template std::wstring LF<const wchar_t*&, const wchar_t*&>(const char*, const wchar_t*&, const wchar_t*&);
template std::wstring LF<const wchar_t*, const wchar_t*>(const char*, const wchar_t*&&, const wchar_t*&&);
