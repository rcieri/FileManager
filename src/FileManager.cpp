#include "FileManager.hpp"
#include "Ui.hpp"
#include "Utils.hpp"

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
        if (handleTermCmd(termCmd)) {
            break;
        } else {
            termCmd = FileManager::TermCmds::None;
            refresh();
        }
    }
    return 0;
}

void FileManager::refresh() {
    entries.clear();
    buildTree(cwd, 0);
    if (!entries.empty()) {
        selIdx = std::min(selIdx, entries.size() - 1);
        updateSelEntryPath();
    }
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
        entries.push_back({e.path(), depth});
        if (fs::is_directory(e.path()) && expandedDirs.count(e.path()))
            buildTree(e.path(), depth + 1);
    }
}

// --- Input handling ---
void FileManager::handleEvent(Event event, ScreenInteractive &screen) {
    try {
        if (prompt != Prompt::None) {
            handlePromptEvent(event, screen);
            return;
        }
        switch (mode) {
        case Mode::Normal:
            handleNormalEvent(event, screen);
            break;
        case Mode::Select:
            handleSelectEvent(event, screen);
            break;
        }
        // case Mode::Search:
        //     handleSearchEvent(event, screen);

    } catch (const std::exception &e) {
        error = "Error: " + std::string(e.what());
        prompt = Prompt::Error;
    }
}

void FileManager::handleNormalEvent(Event event, ScreenInteractive &screen) {
    if (event.is_character()) {
        const std::string &ch = event.character();
        if (ch.size() == 1) {
            switch (ch[0]) {
            case 'j':
                moveSelection(1, screen);
                break;
            case 'k':
                moveSelection(-1, screen);
                break;
            case 'J':
                moveSelection(4, screen);
                break;
            case 'K':
                moveSelection(-4, screen);
                break;
            case 'h':
                goToParent();
                break;
            case 'l':
                openDir();
                break;
            case 'e':
                termCmd = FileManager::TermCmds::Edit;
                screen.ExitLoopClosure()();
                break;
            case 'o':
                termCmd = FileManager::TermCmds::Open;
                screen.ExitLoopClosure()();
                break;
            case '\x03':
                termCmd = FileManager::TermCmds::Quit;
                screen.ExitLoopClosure()();
                break;
            case 'q':
                termCmd = FileManager::TermCmds::QuitToLast;
                screen.ExitLoopClosure()();
                break;
            case 'c':
                termCmd = FileManager::TermCmds::ChangeDir;
                screen.ExitLoopClosure()();
                break;
            case 'C':
                changeDrive(screen);
                break;
            case '?':
                promptUser(Prompt::Help);
                break;
            case 'R':
                termCmd = FileManager::TermCmds::Run;
                screen.ExitLoopClosure()();
                break;
            case ' ':
                promptUser(Prompt::FzfMenu);
                break;
            case 'r':
                promptUser(Prompt::Rename);
                break;
            case 'm':
                promptUser(Prompt::Move);
                break;
            case 'd':
                promptUser(Prompt::Delete);
                break;
            case 'n':
                promptUser(Prompt::NewFile);
                break;
            case 'N':
                promptUser(Prompt::NewDir);
                break;
            case 'y':
                copyPath = selEntryPath;
                break;
            case 'Y':
                termCmd = FileManager::TermCmds::CopyToSys;
                screen.ExitLoopClosure()();
                break;
            case 'x':
                cutPath = selEntryPath;
                break;
            case 'p':
                if (std::optional<Prompt> result = tryPaste()) { promptUser(*result); }
                break;
            case 'u':
                undo();
            case 'v':
                mode = Mode::Select;
                break;
            default:
                break;
            }
        }
    } else if (event == Event::Return) {
        toggleExpand();
    } else if (event == Event::Escape) {
        expandedDirs.clear();
        refresh();
    } else if (event == Event::ArrowUp) {
        changeDirFromHistory(screen);
    }
}

void FileManager::handleSelectEvent(Event event, ScreenInteractive &screen) {
    if (event.is_character()) {
        const std::string &ch = event.character();
        if (ch.size() == 1) {
            switch (ch[0]) {
            case 'j':
                moveSelection(1, screen);
                break;
            case 'k':
                moveSelection(-1, screen);
                break;
            case 'J':
                moveSelection(4, screen);
                break;
            case 'K':
                moveSelection(-4, screen);
                break;
            case ' ':
                toggleSelect();
                break;
            default:
                break;
            }
        }
    } else if (event == Event::Return) {
        toggleExpand();
    } else if (event == Event::Escape) {
        selItems.clear();
        mode = Mode::Normal;
    } else if (event == Event::ArrowUp) {
        changeDirFromHistory(screen);
    }
}

void FileManager::handlePromptEvent(Event event, ScreenInteractive &screen) {
    switch (prompt) {
    case Prompt::DriveSelect:
        if (event == Event::Character("k")) {
            selDriveIdx = (selDriveIdx - 1 + drives.size()) % drives.size();
        } else if (event == Event::Character("j")) {
            selDriveIdx = (selDriveIdx + 1) % drives.size();
        } else if (event == Event::Return) {
            cwd = fs::path(drives[selDriveIdx].path);
            expandedDirs.clear();
            expandedDirs.insert(cwd);
            refresh();
            prompt = Prompt::None;
        } else if (event == Event::Escape) {
            prompt = Prompt::None;
        }
        break;

    case Prompt::History:
        if (event == Event::Character("k")) {
            selHistIdx = (selHistIdx - 1 + history.size()) % history.size();
        } else if (event == Event::Character("j")) {
            selHistIdx = (selHistIdx + 1) % history.size();
        } else if (event == Event::Return) {
            cwd = fs::path(history[selHistIdx]);
            expandedDirs.clear();
            expandedDirs.insert(cwd);
            refresh();
            prompt = Prompt::None;
        } else if (event == Event::Escape) {
            prompt = Prompt::None;
        }
        break;

    case Prompt::Rename:
        if (event == Event::Return) {
            fs::path newPath = promptPath.parent_path() / promptInput;
            fs::rename(promptPath, newPath);
            Undo u = Undo{prompt, promptPath, newPath};
            undoStack.push(u);
            prompt = Prompt::None;
            refresh();
        } else if (event == Event::Escape) {
            prompt = Prompt::None;
        } else {
            promptContainer->OnEvent(event);
        }
        break;

    case Prompt::Move:
        if (event == Event::Return) {
            fs::path newPath = promptInput / promptPath.filename();
            fs::rename(promptPath, newPath);
            Undo u = Undo{prompt, promptPath, newPath};
            undoStack.push(u);
            prompt = Prompt::None;
            refresh();
        } else if (event == Event::Escape) {
            prompt = Prompt::None;
        } else {
            promptContainer->OnEvent(event);
        }
        break;

    case Prompt::Delete:
        if (event == Event::Return) {
            if (fs::is_regular_file(promptPath)) {
                Undo u{prompt, promptPath, {}, std::nullopt};
                std::ifstream in(promptPath, std::ios::binary);
                std::string data((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
                u.contents = data;
                undoStack.push(u);
            }
            deleteFilOrDir(promptPath);
            prompt = Prompt::None;
            refresh();
        } else if (event == Event::Escape) {
            prompt = Prompt::None;
        } else {
            promptContainer->OnEvent(event);
        }
        break;

    case Prompt::Replace:
        if (event == Event::Return) {
            deleteFilOrDir(promptPath);
            tryPaste();
            prompt = Prompt::None;
            refresh();
        } else if (event == Event::Escape) {
            prompt = Prompt::None;
        } else {
            promptContainer->OnEvent(event);
        }
        break;

    case Prompt::NewFile:
        if (event == Event::Return) {
            std::ofstream((promptPath / promptInput).string());
            Undo u = Undo{prompt, promptPath / promptInput, {}, std::nullopt};
            undoStack.push(u);
            prompt = Prompt::None;
            refresh();
        } else if (event == Event::Escape) {
            prompt = Prompt::None;
        } else {
            promptContainer->OnEvent(event);
        }
        break;

    case Prompt::NewDir:
        if (event == Event::Return) {
            fs::create_directory(promptPath / promptInput);
            Undo u = Undo{prompt, promptPath / promptInput, {}, std::nullopt};
            undoStack.push(u);
            prompt = Prompt::None;
            refresh();
        } else if (event == Event::Escape) {
            prompt = Prompt::None;
        } else {
            promptContainer->OnEvent(event);
        }
        break;

    case Prompt::FzfMenu:
        if (event.is_character()) {
            const std::string &ch = event.character();
            if (ch.size() == 1) {
                switch (ch[0]) {
                case 'c':
                    termCmd = TermCmds::FzfClipFile;
                    prompt = Prompt::None;
                    screen.ExitLoopClosure()();
                    break;
                case 'f':
                    termCmd = TermCmds::FzfHxFile;
                    prompt = Prompt::None;
                    screen.ExitLoopClosure()();
                    break;
                case 'o':
                    termCmd = TermCmds::FzfOpenFile;
                    prompt = Prompt::None;
                    screen.ExitLoopClosure()();
                    break;
                case 'e':
                    termCmd = TermCmds::FzfCdFile;
                    prompt = Prompt::None;
                    screen.ExitLoopClosure()();
                    break;
                case 'C':
                    termCmd = TermCmds::FzfClipCwd;
                    prompt = Prompt::None;
                    screen.ExitLoopClosure()();
                    break;
                case 'F':
                    termCmd = TermCmds::FzfHxCwd;
                    prompt = Prompt::None;
                    screen.ExitLoopClosure()();
                    break;
                case 'O':
                    termCmd = TermCmds::FzfOpenCwd;
                    prompt = Prompt::None;
                    screen.ExitLoopClosure()();
                    break;
                case 'E':
                    termCmd = TermCmds::FzfCdCwd;
                    prompt = Prompt::None;
                    screen.ExitLoopClosure()();
                    break;
                case '\x1b': // Escape
                    prompt = Prompt::None;
                    break;
                }
            }
        } else if (event == Event::Escape) {
            prompt = Prompt::None;
        }
        break;

    default:
        if (event == Event::Return || event == Event::Escape)
            prompt = Prompt::None;
        else
            promptContainer->OnEvent(event);
        break;
    }
}

bool FileManager::handleTermCmd(FileManager::TermCmds termCmd) {
    try {
        switch (termCmd) {
        case FileManager::TermCmds::Edit:
            return !edit(selEntryPath.string());
        case FileManager::TermCmds::Open:
            return !start(selEntryPath.string());
        case FileManager::TermCmds::CopyToSys:
            return !copyFileToClip(selEntryPath.string());
        case FileManager::TermCmds::ChangeDir:
            return changeDir(selEntryPath);
        case FileManager::TermCmds::QuitToLast:
            return true;
        case FileManager::TermCmds::Quit:
            writeToAppDataRoamingFile(std::string("."));
            return true;
        case FileManager::TermCmds::Run:
            return !runFileFromTerm(selEntryPath.string());
        case FileManager::TermCmds::FzfClipFile:
            if (std::optional<fs::path> selected = runFzf(selEntryPath)) {
                copyPathToClip(selected->string());
            }
            return false;
        case FileManager::TermCmds::FzfHxFile:
            if (std::optional<fs::path> selected = runFzf(selEntryPath)) {
                return !edit(selected->string());
            }
            return false;
        case FileManager::TermCmds::FzfOpenFile:
            if (std::optional<fs::path> selected = runFzf(selEntryPath)) {
                return !start(selected->string());
            }
            return false;
        case FileManager::TermCmds::FzfCdFile:
            if (std::optional<fs::path> selected = runFzf(selEntryPath)) {
                return !changeDir(selEntryPath);
            }
            return false;
        case FileManager::TermCmds::FzfClipCwd:
            if (std::optional<fs::path> selected = runFzf(cwd)) {
                return !copyFileToClip(selected->string());
            }
            return false;
        case FileManager::TermCmds::FzfHxCwd:
            if (std::optional<fs::path> selected = runFzf(cwd)) {
                return !edit(selected->string());
            }
            return false;
        case FileManager::TermCmds::FzfOpenCwd:
            if (std::optional<fs::path> selected = runFzf(cwd)) {
                return !start(selected->string());
            }
            return false;
        case FileManager::TermCmds::FzfCdCwd:
            return changeDir(selEntryPath);
        default:
            return true;
        }
    } catch (const std::exception &e) {
        error = "Error: " + std::string(e.what());
        prompt = Prompt::Error;
        return false;
    }
}

// --- Actions ---
void FileManager::moveSelection(int delta, ScreenInteractive &screen) {
    if (entries.empty()) return;

    int idx = static_cast<int>(selIdx) + delta;
    if (idx < 0) {
        idx = entries.size() - 1;
    } else if (idx >= static_cast<int>(entries.size())) {
        idx = 0;
    }
    selIdx = idx;
    updateSelEntryPath();

    size_t max_height = screen.dimy() - 6;

    if (selIdx < scrollOffset) {
        scrollOffset = selIdx;
    } else if (selIdx >= scrollOffset + max_height) {
        scrollOffset = selIdx - max_height + 1;
    }

    if (scrollOffset + max_height > entries.size()) {
        scrollOffset = entries.size() > max_height ? entries.size() - max_height : 0;
    }
}

void FileManager::goToParent() {
    if (!parentIdxs.empty()) {
        selIdx = parentIdxs.back();
        parentIdxs.pop_back();
    } else {
        selIdx = 0;
        expandedDirs.clear();
    }
    updateSelEntryPath();

    scrollOffset = 0;
    if (cwd.has_parent_path()) {
        cwd = cwd.parent_path();
        refresh();
    }
}

void FileManager::openDir() {
    parentIdxs.push_back(selIdx);

    if (fs::is_directory(selEntryPath)) {
        cwd = selEntryPath;
        refresh();
    }
    selIdx = 0;
    scrollOffset = 0;
    updateSelEntryPath();
}

void FileManager::toggleExpand() {
    if (!fs::is_directory(selEntryPath)) return;
    if (expandedDirs.count(selEntryPath))
        expandedDirs.erase(selEntryPath);
    else
        expandedDirs.insert(selEntryPath);
    refresh();
}

void FileManager::changeDrive(ScreenInteractive &) {
    drives = listDrives();
    selDriveIdx = 0;
    prompt = Prompt::DriveSelect;
}

void FileManager::changeDirFromHistory(ScreenInteractive &) {
    history = listHistory();
    selHistIdx = 0;
    prompt = Prompt::History;
}

void FileManager::toggleSelect() {
    if (selItems.count(selEntryPath))
        selItems.erase(selEntryPath);
    else
        selItems.insert(selEntryPath);
}

void FileManager::promptUser(Prompt m) {
    prompt = m;
    if (prompt != Prompt::Replace) { promptPath = selEntryPath; }
    if (prompt == Prompt::NewFile || prompt == Prompt::NewDir) {
        if (!fs::is_directory(promptPath)) {
            promptPath = promptPath.parent_path();
            promptInput.clear();
        }
    } else if (prompt == Prompt::Rename) {
        promptInput = promptPath.filename().string();
    } else if (prompt == Prompt::Move) {
        promptInput = promptPath.string();
    }
}

std::optional<FileManager::Prompt> FileManager::tryPaste() {
    if (!copyPath && !cutPath) {
        return std::nullopt;
    } else if (copyPath.has_value()) {
        if (fs::exists(*copyPath)) {
            fs::path dest = cwd / copyPath->filename();
            if (fs::exists(dest)) {
                promptPath = dest;
                return Prompt::Replace;
            } else if (fs::is_directory(*copyPath)) {
                fs::copy(*copyPath, dest, fs::copy_options::recursive);
            } else {
                fs::copy_file(*copyPath, dest);
                copyPath.reset();
            }
        }
    } else if (cutPath.has_value()) {
        fs::path dest = cwd / cutPath->filename();
        fs::rename(*cutPath, dest);
        cutPath.reset();
    }
    refresh();
    return std::nullopt;
}

int FileManager::maxExpandedDepth() const {
    int maxDepth = 0;
    for (auto &entry : entries) {
        if (fs::is_directory(entry.path) && expandedDirs.count(entry.path)) {
            maxDepth = std::max(maxDepth, entry.depth);
        }
    }
    return maxDepth;
}

void FileManager::undo() {
    if (undoStack.empty()) return;

    Undo action = undoStack.top();
    undoStack.pop();

    switch (action.type) {
    case Prompt::Rename:
        fs::rename(action.target, action.source);
        break;
    case Prompt::Move:
        fs::rename(action.target, action.source);
        break;
    case Prompt::Delete:
        if (action.contents) {
            std::ofstream out(action.source, std::ios::binary);
            out << *action.contents;
            out.close();
        }
        break;
    case Prompt::NewFile:
        deleteFilOrDir(action.source);
        break;
    case Prompt::NewDir:
        deleteFilOrDir(action.source);
        break;
    // case Prompt::Cut:
    // case Prompt::Copy:
    default:
        break;
    }
    refresh();
}

std::vector<fs::path> FileManager::entriesPaths() const {
    std::vector<fs::path> paths;
    for (auto &e : entries) paths.push_back(e.path);
    return paths;
}

void FileManager::updateSelEntryPath() { selEntryPath = entries[selIdx].path; }
