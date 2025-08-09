#include "FileManager.hpp"
#include "Ui.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <optional>
#include <set>
#include <shlobj.h>
#include <string>
#include <vector>

using namespace ftxui;

int FileManager::Run() {
    UI ui(*this);
    while (true) {
        ScreenInteractive screen = ScreenInteractive::Fullscreen();
        auto renderer = Renderer([&ui, &screen] { return ui.render(screen); });
        auto interactive = CatchEvent(renderer, [&](Event e) {
            handleEvent(e, screen);
            return true;
        });
        screen.Loop(interactive);
        if (handleTermCommand(termCmd, visibleEntries[selectedIndex].path.string())) {
            break;
        } else {
            termCmd = FileManager::TermCmds::None;
            refresh();
        }
    }
    return 0;
}

void FileManager::refresh() {
    visibleEntries.clear();
    buildTree(rootPath, 0);
    if (!visibleEntries.empty()) selectedIndex = std::min(selectedIndex, visibleEntries.size() - 1);
}

void FileManager::buildTree(const fs::path &path, int depth) {
    if (!fs::exists(path) || !fs::is_directory(path)) return;

    std::vector<fs::directory_entry> children;
    for (auto &e : fs::directory_iterator(path)) children.push_back(e);

    std::sort(children.begin(), children.end(), [](auto &a, auto &b) {
        bool da = fs::is_directory(a), db = fs::is_directory(b);
        if (da != db) return da > db;
        auto an = a.path().filename().string();
        auto bn = b.path().filename().string();
        std::transform(an.begin(), an.end(), an.begin(), ::tolower);
        std::transform(bn.begin(), bn.end(), bn.begin(), ::tolower);
        return an < bn;
    });

    for (auto &e : children) {
        visibleEntries.push_back({e.path(), depth});
        if (fs::is_directory(e.path()) && expandedDirs.count(e.path()))
            buildTree(e.path(), depth + 1);
    }
}

std::vector<std::string> FileManager::listDrives() {
    std::vector<std::string> drives;
    DWORD mask = GetLogicalDrives();
    char drive = 'A';
    while (mask) {
        if (mask & 1) drives.push_back(std::string(1, drive) + ":\\");
        mask >>= 1;
        drive++;
    }
    return drives;
}

std::vector<std::string> FileManager::listHistory() {
    std::vector<std::string> history;

    PWSTR path = NULL;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) { return history; }

    char buffer[MAX_PATH];
    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, buffer, MAX_PATH, path, MAX_PATH - 1);
    CoTaskMemFree(path);

    std::string appDataDir = std::string(buffer) + "\\FileManager";
    std::string historyFile = appDataDir + "\\history.json";

    std::ifstream inFile(historyFile);
    if (!inFile.is_open()) { return history; }

    try {
        json j;
        inFile >> j;

        if (!j.is_object()) { return history; }

        std::unordered_map<std::string, int> counts = j.get<std::unordered_map<std::string, int>>();

        std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());

        std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first < b.first;
        });

        for (size_t i = 0; i < sorted.size() && i < 5; ++i) { history.push_back(sorted[i].first); }
    } catch (const std::exception &e) { return history; }

    return history;
}

// --- Input handling ---
void FileManager::handleEvent(Event event, ScreenInteractive &screen) {
    try {
        if (modal != Modal::None) {
            handleModalEvent(event);
            return;
        }

        if (event.is_character()) {
            const std::string &ch = event.character();
            if (ch.size() == 1) {
                switch (ch[0]) {
                case 'j': moveSelection(1, screen); break;
                case 'k': moveSelection(-1, screen); break;
                case 'J': moveSelection(4, screen); break;
                case 'K': moveSelection(-4, screen); break;
                case 'h': goToParent(); break;
                case 'l': openDir(); break;
                case 'e': editFile(screen); break;
                case 'o': openFile(screen); break;
                case '\x03': quit(screen); break;
                case 'q': quitToLast(screen); break;
                case 'c': changeDir(screen); break;
                case 'C': changeDrive(screen); break;
                case '?': promptModal(Modal::Help); break;
                case ' ': toggleSelect(); break;
                case 'r': promptModal(Modal::Rename); break;
                case 'm': promptModal(Modal::Move); break;
                case 'd': promptModal(Modal::Delete); break;
                case 'n': promptModal(Modal::NewFile); break;
                case 'N': promptModal(Modal::NewDir); break;
                case 'y': copy(); break;
                case 'Y': copyToSys(screen); break;
                case 'x': cut(); break;
                case 'p': paste(); break;
                default: break;
                }
            }
        } else if (event == Event::Return) {
            toggleExpand();
        } else if (event == Event::Escape) {
            collapseAll();
        } else if (event == Event::ArrowUp) {
            changeDirFromHistory(screen);
        }
    } catch (const std::exception &e) {
        error = "Error handling event: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::handleModalEvent(Event event) {
    switch (modal) {
    case Modal::DriveSelect:
        if (event == Event::Character("k"))
            selectedDriveIndex = (selectedDriveIndex - 1 + drives.size()) % drives.size();
        else if (event == Event::Character("j"))
            selectedDriveIndex = (selectedDriveIndex + 1) % drives.size();
        else if (event == Event::Return) {
            rootPath = fs::path(drives[selectedDriveIndex]);
            expandedDirs.clear();
            expandedDirs.insert(rootPath);
            refresh();
            modal = Modal::None;
        } else if (event == Event::Escape) {
            modal = Modal::None;
        }
        break;

    case Modal::History:
        if (event == Event::Character("k"))
            selectedHistoryIndex = (selectedHistoryIndex - 1 + history.size()) % history.size();
        else if (event == Event::Character("j"))
            selectedHistoryIndex = (selectedHistoryIndex + 1) % history.size();
        else if (event == Event::Return) {
            rootPath = fs::path(history[selectedHistoryIndex]);
            expandedDirs.clear();
            expandedDirs.insert(rootPath);
            refresh();
            modal = Modal::None;
        } else if (event == Event::Escape) {
            modal = Modal::None;
        }
        break;

    case Modal::Rename:
        if (event == Event::Return) {
            fs::rename(modalTarget, modalTarget.parent_path() / modalInput);
            modal = Modal::None;
            refresh();
        } else if (event == Event::Escape) {
            modal = Modal::None;
        } else {
            modalContainer->OnEvent(event);
        }
        break;

    case Modal::Move:
        if (event == Event::Return) {
            fs::rename(modalTarget, fs::path(modalInput) / modalTarget.filename());
            modal = Modal::None;
            refresh();
        } else if (event == Event::Escape) {
            modal = Modal::None;
        } else {
            modalContainer->OnEvent(event);
        }
        break;

    case Modal::Delete:
        if (event == Event::Return) {
            if (fs::is_directory(modalTarget))
                fs::remove_all(modalTarget);
            else
                fs::remove(modalTarget);
            modal = Modal::None;
            refresh();
        } else if (event == Event::Escape) {
            modal = Modal::None;
        } else {
            modalContainer->OnEvent(event);
        }
        break;

    case Modal::NewFile:
        if (event == Event::Return) {
            std::ofstream((modalTarget / modalInput).string());
            modal = Modal::None;
            refresh();
        } else if (event == Event::Escape) {
            modal = Modal::None;
        } else {
            modalContainer->OnEvent(event);
        }
        break;

    case Modal::NewDir:
        if (event == Event::Return) {
            fs::create_directory(modalTarget / modalInput);
            modal = Modal::None;
            refresh();
        } else if (event == Event::Escape) {
            modal = Modal::None;
        } else {
            modalContainer->OnEvent(event);
        }
        break;

    default:
        if (event == Event::Return || event == Event::Escape)
            modal = Modal::None;
        else
            modalContainer->OnEvent(event);
        break;
    }
}

bool FileManager::handleTermCommand(FileManager::TermCmds termCmd, const std::string path) {
    switch (termCmd) {
    case FileManager::TermCmds::Edit: std::system(("hx \"" + path + "\"").c_str()); return false;
    case FileManager::TermCmds::Open:
        std::system(("start /B explorer \"" + path + "\"").c_str());
        return false;
    case FileManager::TermCmds::CopyToSys: copyPathToClip(path); return false;
    case FileManager::TermCmds::ChangeDir: writeToAppDataRoamingFile(path); return true;
    case FileManager::TermCmds::QuitToLast: return true;
    case FileManager::TermCmds::Quit: writeToAppDataRoamingFile(std::string(".")); return true;
    default: return true;
    }
}

// --- Actions ---
void FileManager::moveSelection(int delta, ScreenInteractive &screen) {
    if (visibleEntries.empty()) return;

    int idx = (int)selectedIndex + delta;
    if (idx < 0) idx = visibleEntries.size() - 1;
    if (idx >= (int)visibleEntries.size()) idx = 0;
    selectedIndex = idx;

    size_t max_height = screen.dimy() - 3;
    if (selectedIndex < scrollOffset)
        scrollOffset = selectedIndex;
    else if (selectedIndex >= scrollOffset + max_height)
        scrollOffset = selectedIndex - max_height + 1;
}

void FileManager::goToParent() {
    if (rootPath.has_parent_path()) {
        rootPath = rootPath.parent_path();
        refresh();
    }
}

// add a path that updates for each function here to track going back and forth
void FileManager::openDir() {
    auto &p = visibleEntries[selectedIndex].path;
    if (fs::is_directory(p)) {
        rootPath = p;
        refresh();
    }
}

void FileManager::toggleExpand() {
    auto &p = visibleEntries[selectedIndex].path;
    if (!fs::is_directory(p)) return;
    if (expandedDirs.count(p))
        expandedDirs.erase(p);
    else
        expandedDirs.insert(p);
    refresh();
}

void FileManager::collapseAll() {
    expandedDirs.clear();
    refresh();
}

void FileManager::editFile(ScreenInteractive &s) {
    termCmd = FileManager::TermCmds::Edit;
    s.ExitLoopClosure()();
}

void FileManager::openFile(ScreenInteractive &s) {
    termCmd = FileManager::TermCmds::Open;
    s.ExitLoopClosure()();
}

void FileManager::quit(ScreenInteractive &s) {
    termCmd = FileManager::TermCmds::Quit;
    s.ExitLoopClosure()();
}

void FileManager::quitToLast(ScreenInteractive &s) {
    termCmd = FileManager::TermCmds::QuitToLast;
    s.ExitLoopClosure()();
}

void FileManager::changeDir(ScreenInteractive &s) {
    termCmd = FileManager::TermCmds::ChangeDir;
    s.ExitLoopClosure()();
}

void FileManager::changeDrive(ScreenInteractive &) {
    drives = listDrives();
    selectedDriveIndex = 0;
    modal = Modal::DriveSelect;
}

void FileManager::changeDirFromHistory(ScreenInteractive &) {
    selectedHistoryIndex = 0;
    modal = Modal::History;
}

void FileManager::toggleSelect() {
    auto p = visibleEntries[selectedIndex].path;
    if (selectedFiles.count(p))
        selectedFiles.erase(p);
    else
        selectedFiles.insert(p);
}

void FileManager::promptModal(Modal m) {
    modal = m;
    modalTarget = visibleEntries[selectedIndex].path;
    if (modal == Modal::NewFile || modal == Modal::NewDir) {
        if (!fs::is_directory(modalTarget)) modalTarget = modalTarget.parent_path();
        modalInput.clear();
    } else if (modal == Modal::Rename) {
        modalInput = modalTarget.filename().string();
    }
}

void FileManager::copy() { copyPath = visibleEntries[selectedIndex].path; }

void FileManager::copyToSys(ScreenInteractive &s) {
    termCmd = FileManager::TermCmds::CopyToSys;
    s.ExitLoopClosure()();
}

void FileManager::cut() { cutPath = visibleEntries[selectedIndex].path; }

void FileManager::paste() {
    if (!copyPath && !cutPath) {
        return;
    } else if (copyPath.has_value()) {
        fs::path dest = rootPath / copyPath->filename();
        if (fs::is_directory(*copyPath))
            fs::copy(*copyPath, dest, fs::copy_options::recursive);
        else
            fs::copy_file(*copyPath, dest);
        copyPath.reset();
    } else if (cutPath.has_value()) {
        fs::path dest = rootPath / cutPath->filename();
        fs::rename(*cutPath, dest);
        cutPath.reset();
    }
    refresh();
}
