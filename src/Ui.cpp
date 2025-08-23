#include "Ui.hpp"
#include "Utils.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <regex>

using namespace ftxui;

// --- UI ---
Element UI::render(ScreenInteractive &screen) {
    Elements rows;
    Layout layout = Layout::compute(screen.dimx(), _fm.maxExpandedDepth());

    // Header row
    rows.push_back(hbox({
        text("NAME") | bold | size(WIDTH, LESS_THAN, layout.max_name_width),
        text("   "),
        text(std::string(layout.max_indent_width, ' ')),
        text(std::string(layout.spacer_width, ' ')),
        text("TYPE") | bold | size(WIDTH, EQUAL, layout.type_col_width),
        text(std::string(layout.spacing, ' ')),
        text("SIZE") | bold | size(WIDTH, EQUAL, layout.size_col_width),
    }));
    rows.push_back(hbox({text(std::string(layout.total_width, '-'))}));

    // File list rows
    size_t max_height = screen.dimy() - 3;
    size_t start = _fm.scrollOffset;
    size_t end = std::min(_fm.scrollOffset + max_height, _fm.visibleEntries.size());

    for (size_t i = start; i < end; ++i) {
        auto [p, depth] = _fm.visibleEntries[i];
        bool isDir = fs::is_directory(p);
        auto icon = text(isDir ? (_fm.expandedDirs.count(p) ? "📂 " : "📁 ") : UI::getFileIcon(p));
        Element name = UI::applyStyle(p, text(p.filename().string()));
        if (_fm.selItems.count(p)) name = name | bgcolor(Color::BlueLight);

        auto typeStr = getFileTypeString(p);
        auto sizeStr = getFileSizeString(p);

        int indent_spaces = std::min(depth * layout.indent_per_level, layout.max_indent_width);
        int icon_and_indent_width = indent_spaces + layout.icon_width;
        int actual_name_len = std::min((int)p.filename().string().length(), layout.max_name_width);
        int name_block_width = icon_and_indent_width + actual_name_len;
        int spacer_width = std::max(layout.type_column - name_block_width, 1);

        auto line = hbox({
            text(std::string(indent_spaces, ' ')),
            icon,
            name | size(WIDTH, LESS_THAN, layout.max_name_width),
            text(std::string(spacer_width, ' ')),
            text(typeStr) | dim | size(WIDTH, EQUAL, layout.type_col_width),
            text(std::string(layout.spacing, ' ')),
            text(sizeStr) | dim | size(WIDTH, EQUAL, layout.size_col_width),
        });

        if (i == _fm.selIdx) line = line | inverted;

        rows.push_back(line);
    }

    // File list box
    Element fileList = vbox({
        text("Current Directory: " + _fm.cwd.string()) | bold | color(Color::Yellow),
        separator(),
        vbox(rows) | flex | frame | borderRounded | bgcolor(Color::Black),
    });

    Element main_view = hbox({fileList | flex}) | size(HEIGHT, EQUAL, screen.dimy());

    std::string title;
    Element body;
    if (_fm.prompt != FileManager::Prompt::None) {
        switch (_fm.prompt) {
        case FileManager::Prompt::Rename:
            title = "Rename to:";
            body = _fm.inputBox->Render();
            break;
        case FileManager::Prompt::Move:
            title = "Move to folder:";
            body = _fm.inputBox->Render();
            break;
        case FileManager::Prompt::NewFile:
            title = "New file name:";
            body = _fm.inputBox->Render();
            break;
        case FileManager::Prompt::NewDir:
            title = "New directory name:";
            body = _fm.inputBox->Render();
            break;
        case FileManager::Prompt::Delete:
            title = "Delete?";
            body = hbox({text(_fm.promptPath.filename().string()) | bold | bgcolor(Color::Red)});
        case FileManager::Prompt::Replace:
            title = "Replace Existing File/Dir?";
            body = hbox({text(_fm.promptPath.filename().string()) | bold});
            break;
        default:
            break;
        }
    }

    if (_fm.prompt == FileManager::Prompt::Help)
        main_view = createHelpOverlay(main_view);
    else if (_fm.prompt == FileManager::Prompt::DriveSelect)
        main_view = createDriveSelect(main_view);
    else if (_fm.prompt == FileManager::Prompt::History)
        main_view = createHistorySelect(main_view);
    else if (_fm.prompt == FileManager::Prompt::Error)
        main_view = createErrorOverlay(main_view);
    else if (_fm.prompt != FileManager::Prompt::None)
        main_view = createPromptBox(main_view, title, body);

    return main_view;
}

Element UI::createPromptBox(const Element &main_view, const std::string &title, Element body) {
    Element backdrop = main_view | dim;

    Element padded_body = vbox({
        filler(),
        hbox({filler(), body, filler()}),
        filler(),
    });

    auto prompt_window =
        window(text(" " + title + " ") | bold | bgcolor(Color::DarkGreen) | color(Color::White),
               body) |
        size(WIDTH, EQUAL, 60) | size(HEIGHT, LESS_THAN, 15);

    return dbox({backdrop, center(prompt_window)});
}

Element UI::createDriveSelect(const Element &main_view) {
    Element backdrop = main_view | dim;

    Elements drive_rows;
    for (size_t i = 0; i < _fm.drives.size(); ++i) {
        auto drive = text(" " + _fm.drives[i] + " ");
        if (i == _fm.selDriveIdx) {
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

Element UI::createHistorySelect(const Element &main_view) {
    Element backdrop = main_view | dim;

    Elements history_rows;
    for (size_t i = 0; i < _fm.history.size(); ++i) {
        auto h = text(" " + _fm.history[i] + " ");
        if (i == _fm.selHistIdx) { h = h | bgcolor(Color::BlueLight) | color(Color::Black) | bold; }
        history_rows.push_back(h);
    }

    auto history_window =
        window(text(" History ") | bold | bgcolor(Color::DarkGreen) | color(Color::White),
               vbox(history_rows)) |
        size(WIDTH, EQUAL, 60) | size(HEIGHT, LESS_THAN, 15);

    return dbox({backdrop, center(history_window)});
}

Element UI::createErrorOverlay(const Element &main_view) {
    std::string errorMessage = _fm.error;

    // Break errorMessage into lines to measure width and height dynamically
    std::istringstream stream(errorMessage);
    std::vector<std::string> lines;
    std::string line;
    size_t max_line_length = 0;
    while (std::getline(stream, line)) {
        lines.push_back(line);
        max_line_length = std::max(max_line_length, line.size());
    }

    const int horizontal_padding = 4;
    const int vertical_padding = 2;
    int box_width = static_cast<int>(max_line_length) + horizontal_padding;
    int box_height = static_cast<int>(lines.size()) + vertical_padding + 3;

    Elements message_lines;
    for (auto &l : lines) { message_lines.push_back(text(l) | color(Color::RedLight) | center); }

    Element backdrop = main_view | dim;

    auto error_window =
        vbox({
            text(" ERROR ") | bold | bgcolor(Color::Red) | color(Color::White) | center,
            separator(),
            vbox(message_lines),
            filler(),
        }) |
        borderRounded | bgcolor(Color::Black) | size(WIDTH, EQUAL, box_width) |
        size(HEIGHT, EQUAL, box_height) | center;

    return dbox({backdrop, center(error_window)});
}

Element UI::createHelpOverlay(const Element &main_view) {
    std::vector<std::pair<std::string, std::string>> help_entries = {
        {"j", "move down"},
        {"k", "move up"},
        {"J", "jump down"},
        {"K", "jump up"},
        {"h", "go to parent"},
        {"l", "enter dir"},
        {"e", "edit file/dir (helix)"},
        {"o", "open file"},
        {"space", "run from term "},
        {"r", "rename"},
        {"m", "move"},
        {"d", "delete"},
        {"n", "new file"},
        {"N", "new directory"},
        {"y", "copy"},
        {"Y", "copy to system"},
        {"x", "cut"},
        {"p", "paste"},
        {"c", "change dir"},
        {"C", "change drive"},
        {"Return", "expand/collapse"},
        {"Esc", "collapse all"},
        {"q", "quit to last"},
        {"^", "history"},
        {"Ctrl+c", "quit"},
        {"?", "show help"},
    };

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
        borderRounded | bgcolor(Color::Black) | size(WIDTH, EQUAL, 50);

    return dbox({main_view | dim, center(help_window)});
}

std::string UI::getFileIcon(const fs::path &p) {
    std::string ext = p.extension().string();
    if (ext == ".cpp" || ext == ".h" || ext == ".c") return "🧠 ";
    if (ext == ".md" || ext == ".txt") return "📝 ";
    if (ext == ".png" || ext == ".jpg") return "🖼️ ";
    if (ext == ".json" || ext == ".xml" || ext == ".yaml") return "📄 ";
    if (ext == ".pdf") return "📚 ";
    if (ext == ".csv") return "📈 ";
    if (ext == ".xlsx") return "📁 ";
    if (ext == ".py") return "🐍 ";
    if (ext == ".m") return "📐 ";
    if (ext == ".cs") return "💻 ";
    return "📃 ";
}

Element UI::applyStyle(const fs::path &p, Element e) {
    std::string ext = p.extension().string();
    if (ext == ".cpp" || ext == ".hpp" || ext == ".c" || ext == ".cc" || ext == ".h")
        return e | color(Color::Green);
    if (ext == ".md" || ext == ".txt") return e | color(Color::Yellow);
    if (ext == ".png" || ext == ".jpg") return e | color(Color::Magenta);
    if (ext == ".json" || ext == ".xml" || ext == ".yaml") return e | color(Color::Cyan);
    if (ext == ".pdf") return e | color(Color::Red);
    if (ext == ".csv") return e | color(Color::Blue);
    if (ext == ".xlsx") return e | color(Color::Green);
    if (ext == ".py") return e | color(Color::Purple);
    if (ext == ".m") return e | color(Color::DarkGreen);
    if (ext == ".cs") return e | color(Color::RedLight);
    return e;
}
