#ifndef UTILS_HPP_
#define UTILS_HPP_

#include <codecvt>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <regex>
#include <shlobj.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

inline std::string getAppDataDir() {
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
    return appDataDir;
}

inline void writeToAppDataRoamingFile(const std::string &changePath) {
    static const std::string appDataDir = getAppDataDir();
    static const std::string historyFile = appDataDir + "\\history.txt";
    static const std::string historyJsonFile = appDataDir + "\\history.json";

    {
        std::ofstream outFile(historyFile);
        outFile << changePath;
    }

    json historyData;

    if (fs::exists(historyJsonFile)) {
        std::ifstream inJson(historyJsonFile);
        try {
            inJson >> historyData;
        } catch (...) { historyData = json::object(); }
    }

    if (!historyData.is_object()) historyData = json::object();

    if (historyData.contains(changePath))
        historyData[changePath] = historyData[changePath].get<int>() + 1;
    else
        historyData[changePath] = 1;

    {
        std::ofstream outJson(historyJsonFile);
        outJson << historyData.dump(4);
    }
}

inline bool edit(const std::string &arg) {
    std::system(("hx \"" + arg + "\"").c_str());
    return true;
}

inline bool start(const std::string &arg) {
    std::system(("start /B explorer \"" + arg + "\"").c_str());
    return true;
}

inline bool changeDir(const fs::path &arg) {
    if (fs::is_directory(arg)) {
        writeToAppDataRoamingFile(arg.string());
        return true;

    } else {
        writeToAppDataRoamingFile(arg.parent_path().string());
        return true;
    }
    return false;
}

inline bool copyPathToClip(const std::string &utf8Path) {
    if (!OpenClipboard(nullptr)) { return false; }
    EmptyClipboard();
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, utf8Path.size() + 1);
    if (!hg) {
        CloseClipboard();
        return false;
    }
    memcpy(GlobalLock(hg), utf8Path.c_str(), utf8Path.size() + 1);
    GlobalUnlock(hg);
    SetClipboardData(CF_TEXT, hg);
    CloseClipboard();
    GlobalFree(hg);
    return true;
}

inline std::string getFileTypeString(const fs::path &p) {
    std::error_code ec;

    if (!fs::exists(p, ec)) {
        if (fs::is_symlink(p, ec)) return "brk";
        return "mis";
    }

    fs::file_status status = fs::symlink_status(p, ec);

    if (status.type() == fs::file_type::regular) {
        auto ext = p.extension().string();
        if (ext.length() >= 2) {
            ext = ext.substr(1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext.length() > 3) ext = ext.substr(0, 3);
            return ext;
        }
        return "non";
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
    } catch (...) {}
    return "";
}

inline void deleteFilOrDir(const fs::path &p) {
    if (fs::is_directory(p))
        fs::remove_all(p);
    else
        fs::remove(p);
}

inline bool runFileFromTerm(const fs::path &path) {
    if (!fs::exists(path)) { return true; }

    std::string cmd;
    std::string ext = path.extension().string();

    if (ext == ".py") {
        cmd = "start cmd /C python \"" + path.string() + "\"";
    } else {
        cmd = "start \"\" \"" + path.string() + "\"";
    }
    std::system(cmd.c_str());
    return true;
}

inline std::optional<fs::path> runFzf(const std::vector<fs::path> &entries, const fs::path &base) {
    if (entries.empty()) { return std::nullopt; }

    const std::string fzfInputFile = getAppDataDir() + "\\fzf_input.txt";
    std::ofstream outFile(fzfInputFile);
    if (!outFile) { return std::nullopt; }

    for (const auto &p : entries) {
        std::error_code ec;
        fs::path rel = fs::relative(p, base, ec);
        if (ec) {
            outFile << p.string() << "\n"; // fallback to absolute
        } else {
            outFile << rel.string() << "\n"; // relative to base directory
        }
    }
    outFile.close();

    std::string cmd = "type \"" + fzfInputFile + "\" | fzf";
    FILE *pipe = _popen(cmd.c_str(), "r");
    if (!pipe) {
        fs::remove(fzfInputFile);
        return std::nullopt;
    }

    char buffer[1024];
    std::string selected;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) { selected = buffer; }

    _pclose(pipe);
    fs::remove(fzfInputFile);

    if (!selected.empty()) {
        if (selected.back() == '\n') { selected.pop_back(); }
        if (!selected.empty()) {
            // join base + relative
            return base / selected;
        }
    }

    return std::nullopt;
}

inline std::optional<fs::path> runFzf(const fs::path &path) {
    fs::path target = path;

    // If not a directory, use parent
    if (!fs::exists(target) || !fs::is_directory(target)) {
        target = target.parent_path();
        if (target.empty() || !fs::exists(target)) return std::nullopt;
    }

    std::vector<fs::path> entries;
    for (auto &p : fs::recursive_directory_iterator(target)) { entries.push_back(p.path()); }

    // Pass target as the base for relative paths
    return runFzf(entries, target);
}

namespace fs = std::filesystem;

inline std::string formatHistoryPath(const fs::path &absPath, const fs::path &cwd) {
    try {
        // 1. Try relative to cwd
        fs::path relToCwd = fs::relative(absPath, cwd);
        if (!relToCwd.empty() && relToCwd.string().find("..") != 0) { return relToCwd.string(); }
    } catch (const fs::filesystem_error &) {}

    try {
        const char *homeEnv = std::getenv("USERPROFILE");
        if (homeEnv) {
            fs::path home(homeEnv);
            fs::path relToHome = fs::relative(absPath, home);
            if (!relToHome.empty() && relToHome.string().find("..") != 0) {
                return std::string("~\\") + relToHome.string();
            }
        }
    } catch (const fs::filesystem_error &) {}

    return absPath.string();
}

#endif
