#pragma once

#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace ftxui;
namespace fs = std::filesystem;

class FileManager {
  public:
    FileManager() : rootPath(fs::current_path()) {
        expandedDirs.insert(rootPath);
        refresh();
        inputBox = Input(&modalInput, "");
        modalContainer = Container::Vertical({inputBox});
    }

    int Run();

    enum class TermCmds { None, ChangeDir, Quit, Edit, Open };

  private:
    enum class Modal { None, Rename, Move, Delete, NewFile, NewDir, Error, DriveSelect };

    struct Entry {
        fs::path path;
        int depth;
    };

    fs::path rootPath, modalTarget;
    std::string modalInput, error;
    Component inputBox, modalContainer;

    std::vector<Entry> visibleEntries;
    std::set<fs::path> expandedDirs, selectedFiles;
    std::vector<std::string> drives;

    size_t selectedIndex = 0;
    size_t scrollOffset = 0;
    int selectedDriveIndex = 0;
    bool _toShowHelp = false;
    TermCmds termCmd = TermCmds::None;
    Modal modal = Modal::None;

    std::optional<fs::path> clipPath;
    bool clipCut = false;

    // Core methods
    void refresh();
    void buildTree(const fs::path &, int);
    std::vector<std::string> listDrives();

    // Input handlers
    void handleEvent(Event, ScreenInteractive &);
    void handleModalEvent(Event);
    bool handleTermCommand(TermCmds, const std::string);
    void moveSelection(int delta, ScreenInteractive &);
    void goToParent();
    void openDir();
    void toggleExpand();
    void editFile(ScreenInteractive &);
    void openFile(ScreenInteractive &);
    void quit(ScreenInteractive &);
    void changeDir(ScreenInteractive &);
    void changeDrive(ScreenInteractive &);
    void toggleSelect();
    void promptModal(Modal);
    void copy();
    void cut();
    void paste();
    void onModalSwitch();

    // Rendering helpers
    Element createModalBox(const Element &, const std::string &, Element);
    Element createHelpOverlay(const Element &);
    Element createDriveSelect(const Element &);
    Element render(ScreenInteractive &);

    // Visual helpers
    std::string getFileIcon(const fs::path &);
    Element applyStyle(const fs::path &, Element);
};
