#pragma once

#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class FileManager {
  public:
    FileManager() : rootPath(fs::current_path()) {
        expandedDirs.insert(rootPath);
        refresh();
        inputBox = ftxui::Input(&modalInput, "");
        modalContainer = ftxui::Container::Vertical({inputBox});
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
    ftxui::Component inputBox, modalContainer;

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
    void handleEvent(ftxui::Event, ftxui::ScreenInteractive &);
    void handleModalEvent(ftxui::Event);
    bool handleTermCommand(TermCmds, const std::string);
    void moveSelection(int delta, ftxui::ScreenInteractive &);
    void goToParent();
    void openDir();
    void toggleExpand();
    void editFile(ftxui::ScreenInteractive &);
    void openFile(ftxui::ScreenInteractive &);
    void quit(ftxui::ScreenInteractive &);
    void changeDir(ftxui::ScreenInteractive &);
    void changeDrive(ftxui::ScreenInteractive &);
    void toggleSelect();
    void promptModal(Modal);
    void copy();
    void cut();
    void paste();
    void onModalSwitch();

    // Rendering helpers
    ftxui::Element createModalBox(const ftxui::Element &, const std::string &, ftxui::Element);
    ftxui::Element createHelpOverlay(const ftxui::Element &);
    ftxui::Element createDriveSelect(const ftxui::Element &);
    ftxui::Element render(ftxui::ScreenInteractive &);

    // Visual helpers
    std::string getFileIcon(const fs::path &);
    ftxui::Element applyStyle(const fs::path &, ftxui::Element);
};
