#ifndef FILEMANAGER_HPP_
#define FILEMANAGER_HPP_

#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

class FileManager {
  public:
    FileManager() : cwd(fs::current_path()) {
        expandedDirs.insert(cwd);
        refresh();
        inputBox = ftxui::Input(&promptInput, "");
        promptContainer = ftxui::Container::Vertical({inputBox});
        history = listHistory();
    }

    enum class TermCmds {
        None,
        ChangeDir,
        Quit,
        QuitToLast,
        Edit,
        Open,
        CopyToSys,
        Run
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
        History
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

    fs::path cwd, promptPath;
    std::string promptInput, error;
    ftxui::Component inputBox, promptContainer;
    std::vector<Entry> visibleEntries;
    std::set<fs::path> expandedDirs, selItems;
    std::vector<std::string> drives, history;
    size_t selIdx = 0;
    std::vector<size_t> parentIdxs;
    size_t scrollOffset = 0;
    int selDriveIdx = 0;
    int selHistIdx = 0;
    TermCmds termCmd = TermCmds::None;
    Prompt prompt = Prompt::None;
    std::optional<fs::path> copyPath, cutPath;
    bool clipCut = false;
    std::stack<Undo> undoStack;

    // Core methods
    int Run();
    void refresh();
    void buildTree(const fs::path &, int);
    std::vector<std::string> listDrives();
    std::vector<std::string> listHistory();

    // Input handlers
    void handleEvent(ftxui::Event, ftxui::ScreenInteractive &);
    void handlePromptEvent(ftxui::Event);
    bool handleTermCmd(TermCmds, const std::string);
    void moveSelection(int delta, ftxui::ScreenInteractive &);
    void goToParent();
    void openDir();
    void toggleExpand();
    void collapseAll();
    void editFile(ftxui::ScreenInteractive &);
    void openFile(ftxui::ScreenInteractive &);
    void quit(ftxui::ScreenInteractive &);
    void quitToLast(ftxui::ScreenInteractive &);
    void changeDir(ftxui::ScreenInteractive &);
    void changeDrive(ftxui::ScreenInteractive &);
    void changeDirFromHistory(ftxui::ScreenInteractive &);
    void toggleSelect();
    void promptUser(Prompt);
    void copy();
    void copyToSys(ftxui::ScreenInteractive &);
    void runFile(ftxui::ScreenInteractive &);
    void cut();
    std::optional<Prompt> tryPaste();
    int maxExpandedDepth() const;
    void undo();
};

#endif
