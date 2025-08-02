#include "Ui.hpp"
#include "Utils.hpp"
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
    Elements rows;

    // Layout constants
    size_t max_height = screen.dimy() - 3;
    size_t start = _fm.scrollOffset;
    size_t end = std::min(_fm.scrollOffset + max_height, _fm.visibleEntries.size());

    const int indent_per_level = 2;
    const int max_indent_spaces = 20;
    const int icon_width = 2; // Emoji + space
    const int type_col_width = 6;
    const int size_col_width = 9;
    const int spacing = 5;

    int total_width = screen.dimx();
    int max_indent_width = indent_per_level * max_indent_spaces;

    int fixed_columns_width =
        max_indent_width + icon_width + type_col_width + size_col_width + spacing * 2;

    int max_name_width = total_width - fixed_columns_width;
    if (max_name_width < 10)
        max_name_width = 10;
    else if (max_name_width > 40)
        max_name_width = 40;

    int indent_spaces = 0;
    int icon_and_indent_width = max_indent_width + icon_width;
    int type_column = max_indent_width + icon_width + max_name_width + spacing;
    int spacer_width =
        std::max(type_column - icon_and_indent_width - (int)std::string("Name").length(), 1);

    rows.push_back(hbox({
        text("NAME") | bold | size(WIDTH, LESS_THAN, max_name_width),
        text("   "),
        text(std::string(max_indent_width, ' ')), // reserve space for max indent
        text(std::string(spacer_width, ' ')),
        text("TYPE") | bold | size(WIDTH, EQUAL, type_col_width),
        text(std::string(spacing, ' ')),
        text("SIZE") | bold | size(WIDTH, EQUAL, size_col_width),
    }));
    auto line = text(std::string(total_width, '-'));

    rows.push_back(hbox({line}));

    for (size_t i = start; i < end; ++i) {
        auto [p, depth] = _fm.visibleEntries[i];
        bool isDir = fs::is_directory(p);
        auto icon = text(isDir ? (_fm.expandedDirs.count(p) ? "📂 " : "📁 ") : UI::getFileIcon(p));
        Element name = UI::applyStyle(p, text(p.filename().string()));

        if (_fm.selectedFiles.count(p))
            name = name | bgcolor(Color::BlueLight);

        auto typeStr = getFileTypeString(p);
        auto sizeStr = getFileSizeString(p);

        int indent_spaces = std::min(depth * indent_per_level, max_indent_width);
        int icon_and_indent_width = indent_spaces + icon_width;

        std::string filenameStr = p.filename().string();
        int actual_name_len = std::min((int)filenameStr.length(), max_name_width);
        int name_block_width = icon_and_indent_width + actual_name_len;

        // Align type/size starting at a consistent column (right after name field)
        int type_column = max_indent_width + icon_width + max_name_width + spacing;

        int spacer_width = std::max(type_column - name_block_width, 1);
        auto dynamic_spacer = text(std::string(spacer_width, ' '));

        auto indent = text(std::string(indent_spaces, ' '));
        auto nameElem = name | size(WIDTH, LESS_THAN, max_name_width);

        auto typeElem = text(typeStr) | dim | size(WIDTH, EQUAL, type_col_width);
        auto sizeElem = text(sizeStr) | dim | size(WIDTH, EQUAL, size_col_width);

        auto line = hbox({
            indent,
            icon,
            nameElem,
            dynamic_spacer,
            typeElem,
            text(std::string(spacing, ' ')),
            sizeElem,
        });

        if (i == _fm.selectedIndex)
            line = line | inverted;

        rows.push_back(line);
    }

    // File list box
    Element fileList = vbox({
        text("Current Directory: " + _fm.rootPath.string()) | bold | color(Color::Yellow),
        separator(),
        vbox(rows) | flex | frame | borderRounded | bgcolor(Color::Black),
    });

    Element main_view = hbox({fileList | flex}) | size(HEIGHT, EQUAL, screen.dimy());

    // Modal handling
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
            body = hbox({
                text("Delete ") | bold | bgcolor(Color::Red),
                text(_fm.modalTarget.filename().string()) | bold | bgcolor(Color::Red),
            });
            break;
        default:
            break;
        }
    }

    // Stack overlays
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
