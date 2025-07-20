#include "Ui.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <regex>

using namespace ftxui;

// --- UI ---
Element UI::createModalBox(const Element &main_view, const std::string &title, Element body) {
    Element backdrop = main_view | dim;

    Element padded_body = vbox({
        filler(),
        hbox({filler(), body, filler()}),
        filler(),
    });

    auto modal_window =
        window(text(" " + title + " ") | bold | bgcolor(Color::DarkGreen) | color(Color::White),
               body) |
        size(WIDTH, EQUAL, 60) | size(HEIGHT, LESS_THAN, 15);

    return dbox({backdrop, center(modal_window)});
}

Element UI::createHelpOverlay(const Element &main_view) {
    std::vector<std::pair<std::string, std::string>> help_entries = {
        {"j", "up"},      {"k", "down"},   {"l", "open"}, {"h", "back"},       {"n", "new file"},
        {"N", "new dir"}, {"r", "rename"}, {"m", "move"}, {"d", "delete"},     {"y", "copy"},
        {"x", "cut"},     {"p", "paste"},  {"q", "quit"}, {"?", "toggle help"}};

    Elements help_rows = {hbox({text(" [Key] ") | bold | color(Color::GrayLight),
                                text("  Description") | bold | color(Color::GrayLight)}),
                          separator()};

    for (auto &[key, desc] : help_entries) {
        help_rows.push_back(hbox(
            {text(" " + key) | color(Color::Cyan), filler(), text(desc) | color(Color::White)}));
    }

    auto help_window =
        window(text(" Help ") | bold | bgcolor(Color::DarkGreen) | color(Color::White),
               vbox(help_rows)) |
        borderRounded | bgcolor(Color::Black) | size(WIDTH, EQUAL, 50) |
        size(HEIGHT, LESS_THAN, 20);

    return dbox({main_view | dim, center(help_window)});
}

Element UI::createDriveSelect(const Element &main_view) {
    Element backdrop = main_view | dim;

    Elements drive_rows;
    for (size_t i = 0; i < _fm.drives.size(); ++i) {
        auto drive = text(" " + _fm.drives[i] + " ");
        if (i == _fm.selectedDriveIndex) {
            drive = drive | bgcolor(Color::BlueLight) | color(Color::Black) | bold;
        }
        drive_rows.push_back(drive);
    }

    auto drive_window =
        window(text(" Drives ") | bold | bgcolor(Color::DarkGreen) | color(Color::White),
               vbox(drive_rows)) |
        size(WIDTH, EQUAL, 40) | size(HEIGHT, LESS_THAN, 15);

    return dbox({backdrop, center(drive_window)});
}

Element UI::render(ScreenInteractive &screen) {
    // Current directory display
    Elements rows;

    // Calculate visible range based on screen height and scroll offset
    size_t max_height = screen.dimy() - 3;
    size_t start = _fm.scrollOffset;
    size_t end = std::min(_fm.scrollOffset + max_height, _fm.visibleEntries.size());

    // Build file list rows
    for (size_t i = start; i < end; ++i) {
        auto [p, depth] = _fm.visibleEntries[i];
        auto icon = text(fs::is_directory(p) ? (_fm.expandedDirs.count(p) ? "📂 " : "📁 ")
                                             : UI::getFileIcon(p));
        Element name = UI::applyStyle(p, text(p.filename().string()));
        if (_fm.selectedFiles.count(p))
            name = name | bgcolor(Color::BlueLight);
        auto line = hbox({text(std::string(depth * 2, ' ')), icon, name});
        if (i == _fm.selectedIndex)
            line = line | inverted;
        rows.push_back(line);
    }

    Element fileList =
        vbox({text("Current Directory: " + _fm.rootPath.string()) | bold | color(Color::Yellow),
              separator(), vbox(rows) | flex | frame | borderRounded | bgcolor(Color::Black)});
    // size(WIDTH, LESS_THAN, 60);

    // Layout: file list left, preview right
    Element main_view = hbox({
                            fileList | flex,
                        }) |
                        size(HEIGHT, EQUAL, screen.dimy());

    // Modal dialog handling
    std::string title;
    Element body;

    if (_fm.modal != FileManager::Modal::None && _fm.modal != FileManager::Modal::DriveSelect) {
        switch (_fm.modal) {
        case FileManager::Modal::Rename:
            title = "Rename to:";
            body = _fm.inputBox->Render();
            break;
        case FileManager::Modal::Move:
            title = "Move to folder:";
            body = _fm.inputBox->Render();
            break;
        case FileManager::Modal::NewFile:
            title = "New file name:";
            body = _fm.inputBox->Render();
            break;
        case FileManager::Modal::NewDir:
            title = "New directory name:";
            body = _fm.inputBox->Render();
            break;
        case FileManager::Modal::Delete:
            title = "Delete";
            body = hbox({text("Delete ") | bold | bgcolor(Color::Red),
                         text(_fm.modalTarget.filename().string()) | bold | bgcolor(Color::Red)});
            break;
        default:
            break;
        }
    }

    // Stack overlays in priority order
    if (_fm._toShowHelp)
        main_view = createHelpOverlay(main_view);
    if (_fm.modal == FileManager::Modal::DriveSelect)
        main_view = createDriveSelect(main_view);
    else if (_fm.modal != FileManager::Modal::None)
        main_view = createModalBox(main_view, title, body);

    return main_view;
}

std::string UI::getFileIcon(const fs::path &p) {
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

Element UI::applyStyle(const fs::path &p, Element e) {
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
