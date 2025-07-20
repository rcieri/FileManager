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
        auto screen = ScreenInteractive::Fullscreen();
        auto renderer = ftxui::Renderer([this, &screen] { return render(screen); });
        auto interactive = CatchEvent(renderer, [&](Event e) {
            handleEvent(e, screen);
            return true;
        });
        screen.Loop(interactive);

        if (to_edit) {
            std::system(("hx \"" + *to_edit + "\"").c_str());
            to_edit.reset();
            refresh();
        } else if (to_open) {
            std::system(("start /B explorer.exe \"" + *to_open + "\"").c_str());
            to_open.reset();
        } else if (to_change_dir) {
            writeToAppDataRoamingFile(visible_entries[selected_index].path.string());
            to_change_dir = false;
            break;
        } else if (to_quit) {
            to_quit = false;
            break;
        } else {
            std::string cwd = ".";
            writeToAppDataRoamingFile(cwd);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return 0;
}

void FileManager::refresh() {
    visible_entries.clear();
    buildTree(root_path, 0);
    if (!visible_entries.empty())
        selected_index = std::min(selected_index, visible_entries.size() - 1);
}

void FileManager::buildTree(const fs::path &path, int depth) {
    try {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            return;
        }

        std::vector<fs::directory_entry> children;
        try {
            for (auto &e : fs::directory_iterator(path))
                children.push_back(e);
        } catch (const std::exception &e) {
            error_message = "Permission denied: " + path.string() + " - " + e.what();
            modal = Modal::Error;
            return;
        }

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
            visible_entries.push_back({e.path(), depth});
            if (fs::is_directory(e.path()) && expanded_dirs.count(e.path()))
                buildTree(e.path(), depth + 1);
        }
    } catch (const std::exception &e) {
        error_message = "Failed to build tree: " + std::string(e.what());
        modal = Modal::Error;
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

        if (modal == Modal::Error) {
            modal = Modal::None;
            return;
        }

        if (event == Event::Character("j"))
            moveSelection(1, screen);
        else if (event == Event::Character("k"))
            moveSelection(-1, screen);
        if (event == Event::Character("J"))
            moveSelection(4, screen);
        else if (event == Event::Character("K"))
            moveSelection(-4, screen);
        else if (event == Event::Character("h"))
            goToParent();
        else if (event == Event::Character("l"))
            openDir();
        else if (event == Event::Character("e"))
            editFile(screen);
        else if (event == Event::Character("o"))
            openFile(screen);
        else if (event == Event::Character("q"))
            quit(screen);
        else if (event == Event::Character("c"))
            changeDir(screen);
        else if (event == Event::Character("C"))
            changeDrive(screen);
        else if (event == Event::Character("?"))
            show_help = !show_help;
        else if (event == Event::Character(" "))
            toggleSelect();
        else if (event == Event::Character("r"))
            promptModal(Modal::Rename);
        else if (event == Event::Character("m"))
            promptModal(Modal::Move);
        else if (event == Event::Character("d"))
            promptModal(Modal::Delete);
        else if (event == Event::Character("n"))
            promptModal(Modal::NewFile);
        else if (event == Event::Character("N"))
            promptModal(Modal::NewDir);
        else if (event == Event::Character("y"))
            copy();
        else if (event == Event::Character("x"))
            cut();
        else if (event == Event::Character("p"))
            paste();
        else if (event == Event::Return)
            toggleExpand();
        // else if (event == Event::Character("c"))
        //     print_and_exit(screen);
    } catch (const std::exception &e) {
        error_message = "Error handling event: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::handleModalEvent(Event event) {
    try {
        if (modal == Modal::DriveSelect) {
            if (event == Event::ArrowUp || event == Event::Character("k")) {
                selected_drive_index =
                    (selected_drive_index - 1 + available_drives.size()) % available_drives.size();
            } else if (event == Event::ArrowDown || event == Event::Character("j")) {
                selected_drive_index = (selected_drive_index + 1) % available_drives.size();
            } else if (event == Event::Return) {
                root_path = fs::path(available_drives[selected_drive_index]);
                expanded_dirs.clear();
                expanded_dirs.insert(root_path);
                refresh();
                modal = Modal::None;
            } else if (event == Event::Escape) {
                modal = Modal::None;
            }
            return; // Important: don't fall through to other modal handling
        }

        // Default modal behavior (e.g. rename, move, etc.)
        if (event == Event::Return)
            onModalSwitch();
        else if (event == Event::Escape)
            modal = Modal::None;
        else
            modal_container->OnEvent(event);
    } catch (const std::exception &e) {
        error_message = "Error handling modal event: " + std::string(e.what());
        modal = Modal::Error;
    }
}
// --- Actions ---
void FileManager::moveSelection(int delta, ScreenInteractive &screen) {
    if (visible_entries.empty())
        return;

    int idx = (int)selected_index + delta;
    if (idx < 0)
        idx = visible_entries.size() - 1;
    if (idx >= (int)visible_entries.size())
        idx = 0;
    selected_index = idx;

    size_t max_height = screen.dimy() - 3; // Adjust for header or borders

    // Scroll up if selected above visible window
    if (selected_index < scroll_offset) {
        scroll_offset = selected_index;
    }
    // Scroll down if selected below visible window
    else if (selected_index >= scroll_offset + max_height) {
        scroll_offset = selected_index - max_height + 1;
    }
}
void FileManager::goToParent() {
    try {
        if (root_path.has_parent_path()) {
            root_path = root_path.parent_path();
            refresh();
        }
    } catch (const std::exception &e) {
        error_message = "Error navigating to parent: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::openDir() {
    try {
        auto &p = visible_entries[selected_index].path;
        if (fs::is_directory(p)) {
            root_path = p;
            refresh();
        }
    } catch (const std::exception &e) {
        error_message = "Error opening directory: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::toggleExpand() {
    try {
        auto &p = visible_entries[selected_index].path;
        if (!fs::is_directory(p))
            return;
        if (expanded_dirs.count(p))
            expanded_dirs.erase(p);
        else
            expanded_dirs.insert(p);
        refresh();
    } catch (const std::exception &e) {
        error_message = "Error toggling expand: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::editFile(ScreenInteractive &s) {
    try {
        to_edit = visible_entries[selected_index].path.string();
        s.ExitLoopClosure()();
    } catch (const std::exception &e) {
        error_message = "Error editing file: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::openFile(ScreenInteractive &s) {
    try {
        to_open = visible_entries[selected_index].path.string();
        s.ExitLoopClosure()();
    } catch (const std::exception &e) {
        error_message = "Error opening file: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::quit(ScreenInteractive &s) {
    try {
        to_quit = true;
        s.ExitLoopClosure()();
    } catch (const std::exception &e) {
        error_message = "Error quitting: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::changeDir(ScreenInteractive &s) {
    try {
        to_change_dir = true;
        s.ExitLoopClosure()();
    } catch (const std::exception &e) {
        error_message = "Error quitting: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::changeDrive(ScreenInteractive &s) {
    try {
        available_drives = listDrives();
        selected_drive_index = 0;
        modal = Modal::DriveSelect;
    } catch (const std::exception &e) {
        error_message = "Error listing drives: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::toggleSelect() {
    try {
        auto p = visible_entries[selected_index].path;
        if (selected_files.count(p))
            selected_files.erase(p);
        else
            selected_files.insert(p);
    } catch (const std::exception &e) {
        error_message = "Error toggling select: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::promptModal(Modal m) {
    try {
        modal = m;
        modal_target = visible_entries[selected_index].path;
        if (m == Modal::NewFile || m == Modal::NewDir) {
            if (!fs::is_directory(modal_target))
                modal_target = modal_target.parent_path();
            modal_input.clear();
        } else if (m == Modal::Rename) {
            modal_input = modal_target.filename().string();
            // } else if (m == Modal::DriveSelect) {
            //     modal_input = modal_target.filename().string();
        }

    } catch (const std::exception &e) {
        error_message = "Error prompting modal: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::copy() {
    try {
        clipboard_path = visible_entries[selected_index].path;
        clipboard_cut = false;
    } catch (const std::exception &e) {
        error_message = "Error copying file: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::cut() {
    try {
        clipboard_path = visible_entries[selected_index].path;
        clipboard_cut = true;
    } catch (const std::exception &e) {
        error_message = "Error cutting file: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::paste() {
    try {
        if (!clipboard_path)
            return;
        fs::path dest = root_path / clipboard_path->filename();
        try {
            if (clipboard_cut)
                fs::rename(*clipboard_path, dest);
            else if (fs::is_directory(*clipboard_path))
                fs::copy(*clipboard_path, dest, fs::copy_options::recursive);
            else
                fs::copy_file(*clipboard_path, dest);
            clipboard_path.reset();
            refresh();
        } catch (const std::exception &e) {
            error_message = "Error pasting file: " + std::string(e.what());
            modal = Modal::Error;
        }
    } catch (const std::exception &e) {
        error_message = "Error pasting file: " + std::string(e.what());
        modal = Modal::Error;
    }
}

void FileManager::onModalSwitch() {
    try {
        switch (modal) {
        case Modal::Rename:
            fs::rename(modal_target, modal_target.parent_path() / modal_input);
            break;
        case Modal::Move:
            fs::rename(modal_target, fs::path(modal_input) / modal_target.filename());
            break;
        case Modal::Delete:
            if (fs::is_directory(modal_target))
                fs::remove_all(modal_target);
            else
                fs::remove(modal_target);
            break;
        case Modal::NewFile:
            std::ofstream((modal_target / modal_input).string());
            break;
        case Modal::NewDir:
            fs::create_directory(modal_target / modal_input);
            break;
        default:
            break;
        }
        modal = Modal::None;
        refresh();
    } catch (const std::exception &e) {
        error_message = "Error confirming modal action: " + std::string(e.what());
        modal = Modal::Error;
    }
}

// --- Helper functions ---
Element FileManager::createModalBox(const Element &main_view, const std::string &title,
                                    Element body_content) {
    auto backdrop = main_view | color(Color::GrayLight);
    auto prompt = text(title) | bold | color(Color::White) | bgcolor(Color::Black);
    auto box = window(text(""),
                      vbox(Elements{prompt, separator() | bgcolor(Color::Black), body_content})) |
               bgcolor(Color::Black) | size(WIDTH, EQUAL, 50) | size(HEIGHT, LESS_THAN, 15) |
               center;
    return dbox(Elements{backdrop, box});
}

Element FileManager::createHelpOverlay(const Element &main_view) {
    std::vector<std::pair<std::string, std::string>> help_entries = {
        {"j", "up"},      {"k", "down"},   {"l", "open"}, {"h", "back"},       {"n", "new file"},
        {"N", "new dir"}, {"r", "rename"}, {"m", "move"}, {"d", "delete"},     {"y", "copy"},
        {"x", "cut"},     {"p", "paste"},  {"q", "quit"}, {"?", "toggle help"}};

    Elements help_rows;
    help_rows.push_back(text("[Key]  Description") | bold);
    help_rows.push_back(separator());
    for (auto &e : help_entries) {
        help_rows.push_back(hbox(Elements{text(e.first) | bold, text("     "), text(e.second)}));
    }
    auto help_box = window(text(""), vbox(help_rows));
    return dbox(Elements{main_view, vbox(Elements{filler(), hbox(Elements{filler(), help_box})})});
}

Element FileManager::createDriveSelect(const Element &main_view) {
    auto backdrop = main_view | color(Color::White);
    std::vector<Element> drive_rows;
    for (size_t i = 0; i < available_drives.size(); ++i) {
        auto drive = available_drives[i];
        auto drive_name = text(drive);
        if (i == selected_drive_index)
            drive_name = drive_name | inverted;
        drive_rows.push_back(hbox(Elements{drive_name}));
    }
    auto drive_list = vbox(drive_rows) | border;
    auto box = window(text("Select Drive"), drive_list) | size(WIDTH, EQUAL, 50) |
               size(HEIGHT, LESS_THAN, 15) | center;
    return dbox(Elements{backdrop, box});
}

// --- Main rendering function ---
Element FileManager::render(ScreenInteractive &screen) {
    auto cwd = text("Current Directory: " + root_path.string()) | bold | color(Color::Yellow);
    std::vector<Element> rows;

    size_t max_height = screen.dimy() - 3; // Reserve some rows for header, etc.

    // Clip visible_entries by scroll_offset and max_height
    size_t start = scroll_offset;
    size_t end = std::min(scroll_offset + max_height, visible_entries.size());

    for (size_t i = start; i < end; ++i) {
        auto [p, depth] = visible_entries[i];
        auto icon =
            text(fs::is_directory(p) ? (expanded_dirs.count(p) ? "📂 " : "📁 ") : getFileIcon(p));
        Element name = applyStyle(p, text(p.filename().string()));
        if (selected_files.count(p))
            name = name | bgcolor(Color::BlueLight);
        auto line = hbox(Elements{text(std::string(depth * 2, ' ')), icon, name});
        if (i == selected_index)
            line = line | inverted;
        rows.push_back(line);
    }

    auto main_view = vbox(Elements{cwd, vbox(rows) | border});
    // Modal overlay
    if (modal != Modal::None && modal != Modal::DriveSelect) {
        std::string title;
        Element body;

        switch (modal) {
        case Modal::Rename:
            title = "Rename to:";
            body = input_box->Render();
            break;
        case Modal::Move:
            title = "Move to folder:";
            body = input_box->Render();
            break;
        case Modal::NewFile:
            title = "New file name:";
            body = input_box->Render();
            break;
        case Modal::NewDir:
            title = "New directory name:";
            body = input_box->Render();
            break;
        case Modal::Delete:
            title = "Delete";
            body =
                hbox(Elements{text("Delete ") | bold | bgcolor(Color::Red),
                              text(modal_target.filename().string()) | bold | bgcolor(Color::Red)});
            break;
        default:
            break;
        }

        return createModalBox(main_view, title, body);
    }

    // Help overlay bottom-right
    if (show_help) {
        return createHelpOverlay(main_view);
    }

    // Drive selection modal
    if (modal == Modal::DriveSelect) {
        return createDriveSelect(main_view);
    }

    // Default view
    return main_view;
}

// File icon helper
std::string FileManager::getFileIcon(const fs::path &p) {
    auto ext = p.extension().string();
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

// Style helper
Element FileManager::applyStyle(const fs::path &p, Element e) {
    auto ext = p.extension().string();
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
