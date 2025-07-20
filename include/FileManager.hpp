#pragma once

#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <optional>
#include <set>
#include <string>
#include <vector>

class FileManager {
  public:
    FileManager() : root_path(std::filesystem::current_path()) {
        expanded_dirs.insert(root_path);
        refresh();
        input_box = ftxui::Input(&modal_input, "");
        modal_container = ftxui::Container::Vertical({input_box});
    }

    int Run();

  private:
    enum class Modal { None, Rename, Move, Delete, NewFile, NewDir, Error, DriveSelect };

    struct Entry {
        std::filesystem::path path;
        int depth;
    };

    std::filesystem::path root_path, modal_target;
    std::string modal_input, error_message;
    ftxui::Component input_box, modal_container;

    std::vector<Entry> visible_entries;
    std::set<std::filesystem::path> expanded_dirs, selected_files;
    std::vector<std::string> available_drives;

    size_t selected_index = 0;
    size_t scroll_offset = 0;
    int selected_drive_index = 0;
    bool show_help = false;
    bool to_change_dir = false;
    bool to_quit = false;
    Modal modal = Modal::None;

    std::optional<std::string> to_edit, to_open;
    std::optional<std::filesystem::path> clipboard_path;
    bool clipboard_cut = false;

    // Core methods
    void refresh();
    void buildTree(const std::filesystem::path &, int);
    std::vector<std::string> listDrives();

    // Input handlers
    void handleEvent(ftxui::Event, ftxui::ScreenInteractive &);
    void handleModalEvent(ftxui::Event);
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
    std::string getFileIcon(const std::filesystem::path &);
    ftxui::Element applyStyle(const std::filesystem::path &, ftxui::Element);
};
