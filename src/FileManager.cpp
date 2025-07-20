#include "FileManager.hpp"
#include "Utils.hpp"

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
#include <thread>
#include <vector>

using namespace ftxui;

int FileManager::Run() {
    while (true) {
        ScreenInteractive screen = ScreenInteractive::Fullscreen();
        auto renderer = Renderer([this, &screen] { return render(screen); });
        auto interactive = CatchEvent(renderer, [&](Event e) {
            handleEvent(e, screen);
            return true;
        });
        screen.Loop(interactive);
        if (handleTermCommand(termCmd, visibleEntries[selectedIndex].path.string())) {
            break;
        } else {
            refresh();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}

void FileManager::refresh() {
    visibleEntries.clear();
    buildTree(rootPath, 0);
    if (!visibleEntries.empty())
        selectedIndex = std::min(selectedIndex, visibleEntries.size() - 1);
}

void FileManager::buildTree(const fs::path &path, int depth) {
    if (!fs::exists(path) || !fs::is_directory(path))
        return;

    std::vector<fs::directory_entry> children;
    for (auto &e : fs::directory_iterator(path))
        children.push_back(e);

    std::sort(children.begin(), children.end(), [](auto &a, auto &b) {
        bool da = fs::is_directory(a), db = fs::is_directory(b);
        if (da != db)
            return da > db;
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
        if (mask & 1)
            drives.push_back(std::string(1, drive) + ":\\");
        mask >>= 1;
        drive++;
    }
    return drives;
}

// --- Input handling ---
void FileManager::handleEvent(Event event, ScreenInteractive &screen) {
    try {
        if (modal != Modal::None && modal != Modal::Error) {
            handleModalEvent(event);
            return;
        }

        if (event.is_character()) {
            const std::string &ch = event.character();
            if (ch == "j")
                moveSelection(1, screen);
            else if (ch == "k")
                moveSelection(-1, screen);
            else if (ch == "J")
                moveSelection(4, screen);
            else if (ch == "K")
                moveSelection(-4, screen);
            else if (ch == "h")
                goToParent();
            else if (ch == "l")
                openDir();
            else if (ch == "e")
                editFile(screen);
            else if (ch == "o")
                openFile(screen);
            else if (ch == "q")
                quit(screen);
            else if (ch == "c")
                changeDir(screen);
            else if (ch == "C")
                changeDrive(screen);
            else if (ch == "?")
                _toShowHelp = !_toShowHelp;
            else if (ch == " ")
                toggleSelect();
            else if (ch == "r")
                promptModal(Modal::Rename);
            else if (ch == "m")
                promptModal(Modal::Move);
            else if (ch == "d")
                promptModal(Modal::Delete);
            else if (ch == "n")
                promptModal(Modal::NewFile);
            else if (ch == "N")
                promptModal(Modal::NewDir);
            else if (ch == "y")
                copy();
            else if (ch == "x")
                cut();
            else if (ch == "p")
                paste();
        } else if (event == Event::Return) {
            toggleExpand();
        }
    } catch (const std::exception &e) {
        error = "Error handling event: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::handleModalEvent(Event event) {
    if (modal == Modal::DriveSelect) {
        if (event == Event::ArrowUp || event == Event::Character("k"))
            selectedDriveIndex = (selectedDriveIndex - 1 + drives.size()) % drives.size();
        else if (event == Event::ArrowDown || event == Event::Character("j"))
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
        return;
    }

    if (event == Event::Return)
        onModalSwitch();
    else if (event == Event::Escape)
        modal = Modal::None;
    else
        modalContainer->OnEvent(event);
}

bool handleTermCommand(FileManager::TermCmds termCmd, const std::string path) {
    switch (termCmd) {
    case FileManager::TermCmds::Edit:
        std::system(("hx \"" + path + "\"").c_str());
        return false;
    case FileManager::TermCmds::Open:
        std::system(("start /B explorer.exe \"" + path + "\"").c_str());
        return false;
    case FileManager::TermCmds::ChangeDir:
        writeToAppDataRoamingFile(path);
        return true;
    case FileManager::TermCmds::Quit:
        return true;
    default:
        writeToAppDataRoamingFile(std::string("."));
        return true;
    }
}

// --- Actions ---
void FileManager::moveSelection(int delta, ScreenInteractive &screen) {
    if (visibleEntries.empty())
        return;

    int idx = (int)selectedIndex + delta;
    if (idx < 0)
        idx = visibleEntries.size() - 1;
    if (idx >= (int)visibleEntries.size())
        idx = 0;
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

void FileManager::openDir() {
    auto &p = visibleEntries[selectedIndex].path;
    if (fs::is_directory(p)) {
        rootPath = p;
        refresh();
    }
}

void FileManager::toggleExpand() {
    auto &p = visibleEntries[selectedIndex].path;
    if (!fs::is_directory(p))
        return;
    if (expandedDirs.count(p))
        expandedDirs.erase(p);
    else
        expandedDirs.insert(p);
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

void FileManager::changeDir(ScreenInteractive &s) {
    termCmd = FileManager::TermCmds::ChangeDir;
    s.ExitLoopClosure()();
}

void FileManager::changeDrive(ScreenInteractive &) {
    drives = listDrives();
    selectedDriveIndex = 0;
    modal = Modal::DriveSelect;
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
    if (m == Modal::NewFile || m == Modal::NewDir) {
        if (!fs::is_directory(modalTarget))
            modalTarget = modalTarget.parent_path();
        modalInput.clear();
    } else if (m == Modal::Rename) {
        modalInput = modalTarget.filename().string();
    }
}

void FileManager::copy() {
    clipPath = visibleEntries[selectedIndex].path;
    clipCut = false;
}

void FileManager::cut() {
    clipPath = visibleEntries[selectedIndex].path;
    clipCut = true;
}

void FileManager::paste() {
    if (!clipPath)
        return;
    fs::path dest = rootPath / clipPath->filename();
    if (clipCut)
        fs::rename(*clipPath, dest);
    else if (fs::is_directory(*clipPath))
        fs::copy(*clipPath, dest, fs::copy_options::recursive);
    else
        fs::copy_file(*clipPath, dest);
    clipPath.reset();
    refresh();
}

void FileManager::onModalSwitch() {
    switch (modal) {
    case Modal::Rename:
        fs::rename(modalTarget, modalTarget.parent_path() / modalInput);
        break;
    case Modal::Move:
        fs::rename(modalTarget, fs::path(modalInput) / modalTarget.filename());
        break;
    case Modal::Delete:
        if (fs::is_directory(modalTarget))
            fs::remove_all(modalTarget);
        else
            fs::remove(modalTarget);
        break;
    case Modal::NewFile:
        std::ofstream((modalTarget / modalInput).string());
        break;
    case Modal::NewDir:
        fs::create_directory(modalTarget / modalInput);
        break;
    default:
        break;
    }
    modal = Modal::None;
    refresh();
}

// --- UI ---
Element FileManager::createModalBox(const Element &main_view, const std::string &title,
                                    Element body) {
    auto backdrop = main_view | color(Color::GrayLight);
    auto prompt = text(title) | bold | color(Color::White) | bgcolor(Color::Black);
    auto box = window(text(""), vbox({prompt, separator() | bgcolor(Color::Black), body})) |
               bgcolor(Color::Black) | size(WIDTH, EQUAL, 50) | size(HEIGHT, LESS_THAN, 15) |
               center;
    return dbox({backdrop, box});
}

Element FileManager::createHelpOverlay(const Element &main_view) {
    std::vector<std::pair<std::string, std::string>> help_entries = {
        {"j", "up"},      {"k", "down"},   {"l", "open"}, {"h", "back"},       {"n", "new file"},
        {"N", "new dir"}, {"r", "rename"}, {"m", "move"}, {"d", "delete"},     {"y", "copy"},
        {"x", "cut"},     {"p", "paste"},  {"q", "quit"}, {"?", "toggle help"}};

    Elements help_rows = {text("[Key]  Description") | bold, separator()};
    for (auto &e : help_entries)
        help_rows.push_back(hbox({text(e.first) | bold, text("     "), text(e.second)}));
    auto help_box = window(text(""), vbox(help_rows));
    return dbox({main_view, vbox({filler(), hbox({filler(), help_box})})});
}

Element FileManager::createDriveSelect(const Element &main_view) {
    auto backdrop = main_view | color(Color::White);
    Elements drive_rows;
    for (size_t i = 0; i < drives.size(); ++i) {
        auto drive = text(drives[i]);
        if (i == selectedDriveIndex)
            drive = drive | inverted;
        drive_rows.push_back(hbox({drive}));
    }
    auto box = window(text("Select Drive"), vbox(drive_rows) | border) | size(WIDTH, EQUAL, 50) |
               size(HEIGHT, LESS_THAN, 15) | center;
    return dbox({backdrop, box});
}

Element FileManager::render(ScreenInteractive &screen) {
    auto cwd = text("Current Directory: " + rootPath.string()) | bold | color(Color::Yellow);
    Elements rows;

    size_t max_height = screen.dimy() - 3;
    size_t start = scrollOffset;
    size_t end = std::min(scrollOffset + max_height, visibleEntries.size());

    for (size_t i = start; i < end; ++i) {
        auto [p, depth] = visibleEntries[i];
        auto icon =
            text(fs::is_directory(p) ? (expandedDirs.count(p) ? "📂 " : "📁 ") : getFileIcon(p));
        Element name = applyStyle(p, text(p.filename().string()));
        if (selectedFiles.count(p))
            name = name | bgcolor(Color::BlueLight);
        auto line = hbox({text(std::string(depth * 2, ' ')), icon, name});
        if (i == selectedIndex)
            line = line | inverted;
        rows.push_back(line);
    }

    auto main_view = vbox({cwd, vbox(rows) | border});

    if (modal != Modal::None && modal != Modal::DriveSelect) {
        std::string title;
        Element body;

        switch (modal) {
        case Modal::Rename:
            title = "Rename to:";
            body = inputBox->Render();
            break;
        case Modal::Move:
            title = "Move to folder:";
            body = inputBox->Render();
            break;
        case Modal::NewFile:
            title = "New file name:";
            body = inputBox->Render();
            break;
        case Modal::NewDir:
            title = "New directory name:";
            body = inputBox->Render();
            break;
        case Modal::Delete:
            title = "Delete";
            body = hbox({text("Delete ") | bold | bgcolor(Color::Red),
                         text(modalTarget.filename().string()) | bold | bgcolor(Color::Red)});
            break;
        default:
            break;
        }

        return createModalBox(main_view, title, body);
    }

    if (_toShowHelp)
        return createHelpOverlay(main_view);
    if (modal == Modal::DriveSelect)
        return createDriveSelect(main_view);

    return main_view;
}

std::string FileManager::getFileIcon(const fs::path &p) {
    std::string ext = p.extension().string();
    if (ext == ".cpp" || ext == ".h" || ext == ".c")
        return "🧠 ";
    if (ext == ".md" || ext == ".txt")
        return "📝 ";
    if (ext == ".png" || ext == ".jpg")
        return "🖼️ ";
    if (ext == ".json" || ext == ".xml" || ext == ".yaml")
        return "📄 ";
    if (ext == ".pdf")
        return "📚 ";
    if (ext == ".csv")
        return "📈 ";
    if (ext == ".xlsx")
        return "📁 ";
    if (ext == ".py")
        return "🐍 ";
    if (ext == ".m")
        return "📐 ";
    if (ext == ".cs")
        return "💻 ";
    return "📃 ";
}

Element FileManager::applyStyle(const fs::path &p, Element e) {
    std::string ext = p.extension().string();
    if (ext == ".cpp" || ext == ".hpp" || ext == ".c" || ext == ".cc" || ext == ".h")
        return e | color(Color::Green);
    if (ext == ".md" || ext == ".txt")
        return e | color(Color::Yellow);
    if (ext == ".png" || ext == ".jpg")
        return e | color(Color::Magenta);
    if (ext == ".json" || ext == ".xml" || ext == ".yaml")
        return e | color(Color::Cyan);
    if (ext == ".pdf")
        return e | color(Color::Red);
    if (ext == ".csv")
        return e | color(Color::Blue);
    if (ext == ".xlsx")
        return e | color(Color::Green);
    if (ext == ".py")
        return e | color(Color::Purple);
    if (ext == ".m")
        return e | color(Color::DarkGreen);
    if (ext == ".cs")
        return e | color(Color::RedLight);
    return e;
}
