#ifndef FILEMANAGER_HPP_
#define FILEMANAGER_HPP_

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
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

class FileManager {
  public:
    FileManager() : cwd(fs::current_path()) {
        expandedDirs.insert(cwd);
        inputBox = ftxui::Input(&promptInput, "");
        promptContainer = ftxui::Container::Vertical({inputBox});
        refresh();
    }

    enum class TermCmds {
        None,
        ChangeDir,
        Quit,
        QuitToLast,
        Edit,
        Open,
        CopyToSys,
        Run,
        FzfHxFile,
        FzfClipFile,
        FzfOpenFile,
        FzfCdFile,
        FzfHxCwd,
        FzfClipCwd,
        FzfOpenCwd,
        FzfCdCwd
    };

    enum class Prompt {
        None,
        Rename,
        Replace,
        Move,
        Delete,
        NewFile,
        NewDir,
        Error,
        DriveSelect,
        Help,
        History,
        FzfMenu,
    };

    enum class Mode {
        Normal,
        Select
    };

    struct Undo {
        Prompt type;
        fs::path source;
        fs::path target;
        std::optional<std::string> contents;
    };

    struct Entry {
        fs::path path;
        int depth;
    };

    struct Drive {
        std::string path;
        std::string name;
    };

    fs::path cwd, promptPath;
    std::vector<Entry> entries;
    fs::path selEntryPath;
    std::string promptInput, error;
    Component inputBox, promptContainer;
    std::set<fs::path> expandedDirs, selItems;
    std::vector<std::string> history;
    std::vector<Drive> drives;
    std::vector<size_t> parentIdxs;
    size_t selIdx = 0;
    size_t scrollOffset = 0;
    int selDriveIdx = 0;
    int selHistIdx = 0;
    TermCmds termCmd = TermCmds::None;
    Prompt prompt = Prompt::None;
    Mode mode = Mode::Normal;
    std::optional<fs::path> copyPath, cutPath;
    std::stack<Undo> undoStack;
    bool clipCut = false;

    // Core methods
    int Run();
    void refresh();
    void buildTree(const fs::path &, int);
    std::vector<fs::path> entriesPaths() const;
    int maxExpandedDepth() const;

    // Input handlers
    void handleEvent(Event, ScreenInteractive &);
    void handleNormalEvent(Event, ScreenInteractive &);
    void handleSelectEvent(Event, ScreenInteractive &);
    void handlePromptEvent(Event, ScreenInteractive &);
    bool handleTermCmd(TermCmds);
    void moveSelection(int delta, ScreenInteractive &);
    void goToParent();
    void openDir();
    void toggleExpand();
    void changeDrive(ScreenInteractive &);
    void changeDirFromHistory(ScreenInteractive &);
    void toggleSelect();
    void promptUser(Prompt);
    std::optional<Prompt> tryPaste();
    void undo();
    void updateSelEntryPath();
};

#endif
