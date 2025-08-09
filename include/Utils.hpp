#ifndef UTILS_HPP_
#define UTILS_HPP_

#define NOMINMAX

#include <codecvt>
#include <filesystem>
#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <iomanip>
#include <regex>
#include <shlobj.h>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;

inline void writeToAppDataRoamingFile(const std::string &changePath) {
    static std::string appDataDir = [] {
        PWSTR path = NULL;
        std::string result;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) {
            char buffer[MAX_PATH];
            size_t convertedChars = 0;
            wcstombs_s(&convertedChars, buffer, MAX_PATH, path, MAX_PATH - 1);
            CoTaskMemFree(path);
            result = std::string(buffer) + "\\FileManager";
            fs::create_directories(result);
        }
        return result;
    }();

    const std::string historyFile = appDataDir + "\\history.txt";

    std::ofstream outFile(historyFile, std::ios::app);
    outFile << changePath << '\n';
}

inline bool copyPathToClip(const std::string &utf8Path) {
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

inline std::string getFileTypeString(const fs::path &p) {
    std::error_code ec;

    if (!fs::exists(p, ec)) {
        if (fs::is_symlink(p, ec))
            return "brk"; // Broken symlink
        return "mis";     // Missing
    }

    fs::file_status status = fs::symlink_status(p, ec);

    if (status.type() == fs::file_type::regular) {
        auto ext = p.extension().string();
        if (ext.length() >= 2) { // at least '.' + 1 char
            ext = ext.substr(1); // remove dot
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext.length() > 3)
                ext = ext.substr(0, 3);
            return ext;
        }
        return "non"; // No extension
    }

    switch (status.type()) {
    case fs::file_type::directory:
        return "dir";
    case fs::file_type::symlink:
        return "sym";
    case fs::file_type::block:
        return "blk";
    case fs::file_type::character:
        return "chr";
    case fs::file_type::fifo:
        return "fif";
    case fs::file_type::socket:
        return "soc";
    case fs::file_type::unknown:
        return "unk";
    case fs::file_type::none:
        return "non";
    default:
        return "oth";
    }
}

inline std::string getFileSizeString(const fs::path &p) {
    try {
        if (fs::is_regular_file(p)) {
            auto size = fs::file_size(p);
            // Format size to human-readable string (e.g. KB, MB)
            const char *units[] = {"B", "KB", "MB", "GB", "TB"};
            size_t unitIndex = 0;
            double displaySize = static_cast<double>(size);
            while (displaySize >= 1024 && unitIndex < 4) {
                displaySize /= 1024;
                ++unitIndex;
            }
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << displaySize << " " << units[unitIndex];
            return oss.str();
        }
    } catch (...) {
        // Ignore errors like permission denied
    }
    return "";
}

#endif
