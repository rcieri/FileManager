#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

class FileManager {
  public:
    FileManager() : root_path(fs::current_path()) {
        expanded_dirs.insert(root_path);
        refresh_entries();

        // Modal UI components
        input_box = Input(&modal_input, "");
        modal_container = Container::Vertical({input_box});
    }

    int Run() {
        while (true) {
            auto screen = ScreenInteractive::Fullscreen();
            auto renderer = Renderer([&] { return render(); });
            auto interactive = CatchEvent(renderer, [&](Event e) {
                handle_event(e, screen);
                return true;
            });
            screen.Loop(interactive);

            if (to_edit) {
                std::string cmd = "hx \"" + *to_edit + "\"";
                std::system(cmd.c_str());
                to_edit.reset();   // Clear the edit target
                refresh_entries(); // In case the file was renamed/edited
            } else {
                break; // User pressed 'q' or exited without edit
            }
        }
        return 0;
    }

  private:
    enum class Modal { None, Rename, Move, Delete } modal = Modal::None;
    fs::path modal_target;
    std::string modal_input;
    Component input_box, btn_ok, btn_cancel, modal_container;

    fs::path root_path;
    struct Entry {
        fs::path path;
        int depth;
    };
    std::vector<Entry> visible_entries;
    std::set<fs::path> expanded_dirs, selected_files;
    size_t selected_index = 0;
    bool show_help = false;
    std::optional<std::string> to_edit;

    void refresh_entries() {
        visible_entries.clear();
        build_tree(root_path, 0);
        if (!visible_entries.empty())
            selected_index = std::min(selected_index, visible_entries.size() - 1);
    }

    void build_tree(const fs::path &path, int depth) {
        if (!fs::exists(path) || !fs::is_directory(path))
            return;
        std::vector<fs::directory_entry> children;
        for (auto &e : fs::directory_iterator(path))
            children.push_back(e);
        std::sort(children.begin(), children.end(), [](auto const &a, auto const &b) {
            return a.path().filename().string() < b.path().filename().string();
        });
        for (auto &e : children) {
            visible_entries.push_back({e.path(), depth});
            if (fs::is_directory(e.path()) && expanded_dirs.count(e.path()))
                build_tree(e.path(), depth + 1);
        }
    }

    void handle_event(Event event, ScreenInteractive &screen) {
        if (modal != Modal::None) {
            handle_modal_event(event);
            return;
        }

        if (event == Event::Character("j")) {
            move_selection_down();
        } else if (event == Event::Character("k")) {
            move_selection_up();
        } else if (event == Event::Character("h")) {
            go_to_parent_directory();
        } else if (event == Event::Character("l")) {
            open_directory();
        } else if (event == Event::Character("e")) {
            edit_file(screen);
        } else if (event == Event::Character("q")) {
            quit_program(screen);
        } else if (event == Event::Character("?")) {
            toggle_help();
        } else if (event == Event::Character(" ")) {
            toggle_file_selection();
        } else if (event == Event::Character("r")) {
            prompt_rename();
        } else if (event == Event::Character("m")) {
            prompt_move();
        } else if (event == Event::Character("d")) {
            prompt_delete();
        } else if (event == Event::Return) {
            toggle_directory_expansion();
        }
    }

    void handle_modal_event(Event event) {
        if (event == Event::Return) {
            on_modal_ok();
        } else if (event == Event::Escape) {
            modal = Modal::None;
        } else {
            modal_container->OnEvent(event);
        }
    }
    void move_selection_down() { selected_index = (selected_index + 1) % visible_entries.size(); }

    void move_selection_up() {
        selected_index = selected_index == 0 ? visible_entries.size() - 1 : selected_index - 1;
    }

    void go_to_parent_directory() {
        if (root_path.has_parent_path()) {
            root_path = root_path.parent_path();
            refresh_entries();
        }
    }

    void open_directory() {
        auto sel = visible_entries[selected_index].path;
        if (fs::is_directory(sel)) {
            root_path = sel;
            refresh_entries();
        }
    }

    void edit_file(ScreenInteractive &screen) {
        to_edit = visible_entries[selected_index].path.string();
        screen.ExitLoopClosure()();
    }

    void quit_program(ScreenInteractive &screen) { screen.ExitLoopClosure()(); }

    void toggle_help() { show_help = !show_help; }

    void toggle_file_selection() {
        auto sel = visible_entries[selected_index].path;
        if (selected_files.count(sel))
            selected_files.erase(sel);
        else
            selected_files.insert(sel);
    }

    void prompt_rename() {
        modal = Modal::Rename;
        modal_target = visible_entries[selected_index].path;
        modal_input = "";
    }

    void prompt_move() {
        modal = Modal::Move;
        modal_target = visible_entries[selected_index].path;
        modal_input.clear();
    }

    void prompt_delete() {
        modal = Modal::Delete;
        modal_target = visible_entries[selected_index].path;
    }

    void toggle_directory_expansion() {
        auto sel = visible_entries[selected_index].path;
        if (fs::is_directory(sel)) {
            if (expanded_dirs.count(sel))
                expanded_dirs.erase(sel);
            else
                expanded_dirs.insert(sel);
            refresh_entries();
        }
    }

    Element render() {
        if (show_help) {
            return window(vbox({text("Help - FileManager") | bold | center, separator(),
                                text("j/k: Navigate up/down"), text("Enter: Expand/Collapse"),
                                text("h: Parent directory"), text("l: Open directory"),
                                text("e: Edit"), text("r: Rename"), text("m: Move"),
                                text("d: Delete"), text("Space: Select"), text("q: Quit"),
                                text("?: Toggle help") | dim | center}),
                          text("Help")) |
                   border;
        }

        std::vector<Element> lines;
        for (size_t i = 0; i < visible_entries.size(); ++i) {
            auto [path, depth] = visible_entries[i];
            std::string indent(depth * 2, ' ');
            auto icon = text(fs::is_directory(path) ? (expanded_dirs.count(path) ? "📂 " : "📁 ")
                                                    : file_icon(path));
            auto name = apply_style(path, text(path.filename().string()));
            if (selected_files.count(path))
                name = name | bgcolor(Color::BlueLight);

            auto line = hbox({text(indent), icon, name});
            if (i == selected_index)
                line = line | inverted;
            lines.push_back(line);
        }

        auto main_view = vbox({vbox(lines) | border});

        if (modal != Modal::None) {
            // Apply the dim effect only to the backdrop
            auto backdrop = main_view | color(Color::GrayLight);

            std::string title;
            if (modal == Modal::Rename)
                title = "Rename to:";
            else if (modal == Modal::Move)
                title = "Move to folder:";
            else
                title = "Confirm delete?";

            // Title with bold white text, increased size for prominence (no dimming)
            auto prompt = text(title) | bold | color(Color::White) | bgcolor(Color::Black);
            auto sep = separator() | bgcolor(Color::Black);

            // Declare body before using it
            ftxui::Element body;

            if (modal == Modal::Delete) {
                body = hbox(
                    Elements{text("Delete ") | bold | bgcolor(Color::Red),
                             text(modal_target.filename().string()) | bold | bgcolor(Color::Red)});
            } else {
                body = input_box->Render();
            }

            auto box_content = vbox(Elements{prompt, sep, body});
            auto box = window(text(""), box_content) | bgcolor(Color::Black) |
                       size(WIDTH, EQUAL, 50) | size(HEIGHT, LESS_THAN, 15) | center;

            return dbox(Elements{backdrop, box});
        }
        return main_view;
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
            default:
                break; // If modal is None or any other undefined state
            }
        } catch (const std::exception &) {
            // TODO: capture error for UI feedback
        }
        modal = Modal::None;
        refresh_entries();
    }
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

    Element apply_style(const fs::path &p, Element e) {
        auto ext = p.extension().string();
        if (ext == ".cpp" || ext == ".hpp" || ext == ".c" || ext == ".cc" || ext == ".h")
            return e | color(Color::Green); // C++ files
        if (ext == ".md" || ext == ".txt")
            return e | color(Color::Yellow); // Markdown and Text files
        if (ext == ".png" || ext == ".jpg")
            return e | color(Color::Magenta); // Image files
        if (ext == ".json" || ext == ".xml" || ext == ".yaml")
            return e | color(Color::Cyan); // JSON, XML, YAML files
        if (ext == ".pdf")
            return e | color(Color::Red); // PDF files
        if (ext == ".csv")
            return e | color(Color::Blue); // CSV files
        if (ext == ".xlsx")
            return e | color(Color::Green); // Excel files
        if (ext == ".py")
            return e | color(Color::Purple); // Python files
        if (ext == ".m")
            return e | color(Color::DarkGreen); // MATLAB files
        if (ext == ".cs")
            return e | color(Color::RedLight); // C# files
        return e;
    }
};

int main() {
    FileManager manager;
    return manager.Run();
}
