#define NOMINMAX
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace ftxui;

class FileManager {
  public:
    FileManager() : root_path(fs::current_path()) {
        expanded_dirs.insert(root_path);
        refresh_entries();
        input_box = Input(&modal_input, "");
        modal_container = Container::Vertical({input_box});
    }

    int Run() {
        while (true) {
            auto screen = ScreenInteractive::Fullscreen();
            auto renderer = Renderer([&] { return render(screen); });
            auto interactive = CatchEvent(renderer, [&](Event e) {
                handle_event(e, screen);
                return true;
            });
            screen.Loop(interactive);

            // rest of your code
            if (to_edit) {
                std::system(("hx \"" + *to_edit + "\"").c_str());
                to_edit.reset();
                refresh_entries();
            } else if (to_open) {
                std::system(("start /B explorer.exe \"" + *to_open + "\"").c_str());
                to_open.reset();
            } else if (to_change_dir) {
                std::ofstream outFile("C:/tmp/fm.txt");
                outFile << visible_entries[selected_index].path;
                outFile.close();
                to_change_dir = false;
                break;
            } else if (to_quit) {
                to_quit = false;
                break;
            }
        }
        return 0;
    }

  private:
    // --- State and helpers ---
    enum class Modal {
        None,
        Rename,
        Move,
        Delete,
        NewFile,
        NewDir,
        Error,
        DriveSelect
    } modal = Modal::None;

    fs::path root_path, modal_target;
    std::string modal_input, error_message;
    Component input_box, modal_container;

    struct Entry {
        fs::path path;
        int depth;
    };
    std::vector<Entry> visible_entries;
    std::set<fs::path> expanded_dirs, selected_files;
    size_t selected_index = 0;
    size_t scroll_offset = 0;
    bool show_help = false;
    std::optional<std::string> to_edit, to_open;
    bool to_change_dir = false;
    bool to_quit = false;

    std::vector<std::string> available_drives;
    int selected_drive_index = 0;

    // Clipboard for copy/cut
    std::optional<fs::path> clipboard_path;
    bool clipboard_cut = false;

    void refresh_entries() {
        visible_entries.clear();
        build_tree(root_path, 0);
        if (!visible_entries.empty())
            selected_index = std::min(selected_index, visible_entries.size() - 1);
    }

    void build_tree(const fs::path &path, int depth) {
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
                    build_tree(e.path(), depth + 1);
            }
        } catch (const std::exception &e) {
            error_message = "Failed to build tree: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    std::vector<std::string> list_drives() {
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
    void handle_event(Event event, ScreenInteractive &screen) {
        try {
            if (modal != Modal::None && modal != Modal::Error) {
                handle_modal_event(event);
                return;
            }

            if (modal == Modal::Error) {
                modal = Modal::None;
                return;
            }

            if (event == Event::Character("j"))
                move_selection(1, screen);
            else if (event == Event::Character("k"))
                move_selection(-1, screen);
            if (event == Event::Character("J"))
                move_selection(4, screen);
            else if (event == Event::Character("K"))
                move_selection(-4, screen);
            else if (event == Event::Character("h"))
                go_parent();
            else if (event == Event::Character("l"))
                open_dir();
            else if (event == Event::Character("e"))
                edit(screen);
            else if (event == Event::Character("o"))
                open_file(screen);
            else if (event == Event::Character("q"))
                quit(screen);
            else if (event == Event::Character("c"))
                change_dir(screen);
            else if (event == Event::Character("C"))
                change_drive(screen);
            else if (event == Event::Character("?"))
                show_help = !show_help;
            else if (event == Event::Character(" "))
                toggle_select();
            else if (event == Event::Character("r"))
                prompt_modal(Modal::Rename);
            else if (event == Event::Character("m"))
                prompt_modal(Modal::Move);
            else if (event == Event::Character("d"))
                prompt_modal(Modal::Delete);
            else if (event == Event::Character("n"))
                prompt_modal(Modal::NewFile);
            else if (event == Event::Character("N"))
                prompt_modal(Modal::NewDir);
            else if (event == Event::Character("y"))
                copy();
            else if (event == Event::Character("x"))
                cut();
            else if (event == Event::Character("p"))
                paste();
            else if (event == Event::Return)
                toggle_expand();
            // else if (event == Event::Character("c"))
            //     print_and_exit(screen);
        } catch (const std::exception &e) {
            error_message = "Error handling event: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void handle_modal_event(Event event) {
        try {
            if (modal == Modal::DriveSelect) {
                if (event == Event::ArrowUp || event == Event::Character("k")) {
                    selected_drive_index = (selected_drive_index - 1 + available_drives.size()) %
                                           available_drives.size();
                } else if (event == Event::ArrowDown || event == Event::Character("j")) {
                    selected_drive_index = (selected_drive_index + 1) % available_drives.size();
                } else if (event == Event::Return) {
                    root_path = fs::path(available_drives[selected_drive_index]);
                    expanded_dirs.clear();
                    expanded_dirs.insert(root_path);
                    refresh_entries();
                    modal = Modal::None;
                } else if (event == Event::Escape) {
                    modal = Modal::None;
                }
                return; // Important: don't fall through to other modal handling
            }

            // Default modal behavior (e.g. rename, move, etc.)
            if (event == Event::Return)
                on_modal_ok();
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
    void move_selection(int delta, ScreenInteractive &screen) {
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
    void go_parent() {
        try {
            if (root_path.has_parent_path()) {
                root_path = root_path.parent_path();
                refresh_entries();
            }
        } catch (const std::exception &e) {
            error_message = "Error navigating to parent: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void open_dir() {
        try {
            auto &p = visible_entries[selected_index].path;
            if (fs::is_directory(p)) {
                root_path = p;
                refresh_entries();
            }
        } catch (const std::exception &e) {
            error_message = "Error opening directory: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void toggle_expand() {
        try {
            auto &p = visible_entries[selected_index].path;
            if (!fs::is_directory(p))
                return;
            if (expanded_dirs.count(p))
                expanded_dirs.erase(p);
            else
                expanded_dirs.insert(p);
            refresh_entries();
        } catch (const std::exception &e) {
            error_message = "Error toggling expand: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void edit(ScreenInteractive &s) {
        try {
            to_edit = visible_entries[selected_index].path.string();
            s.ExitLoopClosure()();
        } catch (const std::exception &e) {
            error_message = "Error editing file: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void open_file(ScreenInteractive &s) {
        try {
            to_open = visible_entries[selected_index].path.string();
            s.ExitLoopClosure()();
        } catch (const std::exception &e) {
            error_message = "Error opening file: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void quit(ScreenInteractive &s) {
        try {
            to_quit = true;
            s.ExitLoopClosure()();
        } catch (const std::exception &e) {
            error_message = "Error quitting: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void change_dir(ScreenInteractive &s) {
        try {
            to_change_dir = true;
            s.ExitLoopClosure()();
        } catch (const std::exception &e) {
            error_message = "Error quitting: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void change_drive(ScreenInteractive &s) {
        try {
            available_drives = list_drives();
            selected_drive_index = 0;
            modal = Modal::DriveSelect;
        } catch (const std::exception &e) {
            error_message = "Error listing drives: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void toggle_select() {
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

    void prompt_modal(Modal m) {
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

    void copy() {
        try {
            clipboard_path = visible_entries[selected_index].path;
            clipboard_cut = false;
        } catch (const std::exception &e) {
            error_message = "Error copying file: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void cut() {
        try {
            clipboard_path = visible_entries[selected_index].path;
            clipboard_cut = true;
        } catch (const std::exception &e) {
            error_message = "Error cutting file: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void paste() {
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
                refresh_entries();
            } catch (const std::exception &e) {
                error_message = "Error pasting file: " + std::string(e.what());
                modal = Modal::Error;
            }
        } catch (const std::exception &e) {
            error_message = "Error pasting file: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    void on_modal_ok() {
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
            refresh_entries();
        } catch (const std::exception &e) {
            error_message = "Error confirming modal action: " + std::string(e.what());
            modal = Modal::Error;
        }
    }

    // --- Helper functions ---
    auto create_modal_box(const Element &main_view, const std::string &title,
                          Element body_content) {
        auto backdrop = main_view | color(Color::GrayLight);
        auto prompt = text(title) | bold | color(Color::White) | bgcolor(Color::Black);
        auto box =
            window(text(""),
                   vbox(Elements{prompt, separator() | bgcolor(Color::Black), body_content})) |
            bgcolor(Color::Black) | size(WIDTH, EQUAL, 50) | size(HEIGHT, LESS_THAN, 15) | center;
        return dbox(Elements{backdrop, box});
    }

    auto create_help_overlay(const Element &main_view) {
        std::vector<std::pair<std::string, std::string>> help_entries = {
            {"j", "up"},       {"k", "down"},       {"l", "open"},   {"h", "back"},
            {"n", "new file"}, {"N", "new dir"},    {"r", "rename"}, {"m", "move"},
            {"d", "delete"},   {"y", "copy"},       {"x", "cut"},    {"p", "paste"},
            {"q", "quit"},     {"?", "toggle help"}};

        Elements help_rows;
        help_rows.push_back(text("[Key]  Description") | bold);
        help_rows.push_back(separator());
        for (auto &e : help_entries) {
            help_rows.push_back(
                hbox(Elements{text(e.first) | bold, text("     "), text(e.second)}));
        }
        auto help_box = window(text(""), vbox(help_rows));
        return dbox(
            Elements{main_view, vbox(Elements{filler(), hbox(Elements{filler(), help_box})})});
    }

    auto create_drive_select_modal(const Element &main_view) {
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
    Element render(ScreenInteractive &screen) {
        auto cwd = text("Current Directory: " + root_path.string()) | bold | color(Color::Yellow);
        std::vector<Element> rows;

        size_t max_height = screen.dimy() - 3; // Reserve some rows for header, etc.

        // Clip visible_entries by scroll_offset and max_height
        size_t start = scroll_offset;
        size_t end = std::min(scroll_offset + max_height, visible_entries.size());

        for (size_t i = start; i < end; ++i) {
            auto [p, depth] = visible_entries[i];
            auto icon =
                text(fs::is_directory(p) ? (expanded_dirs.count(p) ? "📂 " : "📁 ") : file_icon(p));
            auto name = apply_style(p, text(p.filename().string()));
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
                body = hbox(
                    Elements{text("Delete ") | bold | bgcolor(Color::Red),
                             text(modal_target.filename().string()) | bold | bgcolor(Color::Red)});
                break;
            default:
                break;
            }

            return create_modal_box(main_view, title, body);
        }

        // Help overlay bottom-right
        if (show_help) {
            return create_help_overlay(main_view);
        }

        // Drive selection modal
        if (modal == Modal::DriveSelect) {
            return create_drive_select_modal(main_view);
        }

        // Default view
        return main_view;
    }

    // File icon helper
    std::string file_icon(const fs::path &p) {
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
    Element apply_style(const fs::path &p, Element e) {
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
};

int main() {
    FileManager manager;
    return manager.Run();
}
