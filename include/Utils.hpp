#include <codecvt>
#include <filesystem>
#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <regex>
#include <shlobj.h>
#include <string>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;

void writeToAppDataRoamingFile(std::string changePath) {
    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) {
        char buffer[MAX_PATH];
        size_t convertedChars = 0;
        wcstombs_s(&convertedChars, buffer, MAX_PATH, path, MAX_PATH - 1);
        CoTaskMemFree(path);
        std::string appDataDir = std::string(buffer) + "\\FileManager";
        fs::create_directories(appDataDir);
        std::string filePath = appDataDir + "\\fm.txt";
        std::ofstream outFile(filePath);
        outFile << changePath;
        outFile.close();
    }
}

bool copyPathToClip(const std::string &utf8Path) {
    // Convert std::string (UTF-8) to std::wstring (UTF-16)
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring widePath = converter.from_bytes(utf8Path);

    size_t totalSize = sizeof(DROPFILES) +
                       (widePath.length() + 2) * sizeof(wchar_t); // +1 for null, +1 for double-null

    HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, totalSize);
    if (!hGlobal)
        return false;

    DROPFILES *df = static_cast<DROPFILES *>(GlobalLock(hGlobal));
    if (!df) {
        GlobalFree(hGlobal);
        return false;
    }

    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;

    wchar_t *p = reinterpret_cast<wchar_t *>(reinterpret_cast<BYTE *>(df) + sizeof(DROPFILES));
    wcscpy_s(p, widePath.length() + 1, widePath.c_str());
    p[widePath.length() + 1] = L'\0'; // second null terminator

    GlobalUnlock(hGlobal);

    if (!OpenClipboard(nullptr)) {
        GlobalFree(hGlobal);
        return false;
    }

    EmptyClipboard();
    SetClipboardData(CF_HDROP, hGlobal);
    CloseClipboard();

    return true;
}
