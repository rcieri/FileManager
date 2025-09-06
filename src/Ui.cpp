#include "Ui.hpp"
#include "Utils.hpp"

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
    size_t end = std::min(_fm.scrollOffset + max_height, _fm.entries.size());

    for (size_t i = start; i < end; ++i) {
        auto [p, depth] = _fm.entries[i];
        bool isDir = fs::is_directory(p);
        Element fileElem = UI::fileElement(p, isDir, _fm.expandedDirs);

        // Highlight selected items
        if (_fm.selItems.count(p)) { fileElem = fileElem | bgcolor(Color::BlueLight); }
        std::string typeStr = getFileTypeString(p);
        std::string sizeStr = getFileSizeString(p);

        int indent_spaces = std::min(depth * layout.indent_per_level, layout.max_indent_width);
        int icon_and_indent_width = indent_spaces + layout.icon_width;
        int actual_name_len = std::min((int)p.filename().string().length(), layout.max_name_width);
        int name_block_width = icon_and_indent_width + actual_name_len;
        int spacer_width = std::max(layout.type_column - name_block_width, 1);

        auto line = hbox({
            text(std::string(indent_spaces, ' ')),
            fileElem | size(WIDTH, LESS_THAN, layout.max_name_width),
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

    return createOverlay(main_view);
}

Element UI::createOverlay(const Element &main_view) {
    Element backdrop = main_view | dim;

    auto promptBox = [&](const std::string &title, std::optional<Element> body = std::nullopt) {
        return createPromptBox(main_view, title, body);
    };

    switch (_fm.prompt) {
    case FileManager::Prompt::Rename:
        return promptBox("Rename to:");
    case FileManager::Prompt::Move:
        return promptBox("Move to folder:");
    case FileManager::Prompt::NewFile:
        return promptBox("New file name:");
    case FileManager::Prompt::NewDir:
        return promptBox("New directory name:");
    case FileManager::Prompt::Delete:
        return promptBox("Delete?", hbox({text(_fm.promptPath.filename().string()) | bold}));
    case FileManager::Prompt::Replace:
        return promptBox("Replace Existing File/Dir?",
                         hbox({text(_fm.promptPath.filename().string()) | bold}));
    case FileManager::Prompt::DriveSelect:
        return createDriveSelectOverlay(backdrop);
    case FileManager::Prompt::History:
        return createHistoryOverlay(backdrop);
    case FileManager::Prompt::Error:
        return createErrorOverlay(backdrop);
    case FileManager::Prompt::Help:
        return createHelpOverlay(backdrop);
    case FileManager::Prompt::FzfMenu:
        return createFzfMenuOverlay(main_view);
    case FileManager::Prompt::None:
    default:
        return main_view;
    }
}

Element UI::createPromptBox(const Element &main_view, const std::string &title,
                            std::optional<Element> body_opt) {
    Element body = body_opt.value_or(_fm.inputBox->Render());
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

Element UI::createDriveSelectOverlay(const Element &main_view) {
    Element backdrop = main_view | dim;

    Elements drive_rows;
    for (size_t i = 0; i < _fm.drives.size(); ++i) {
        auto drive = text(" " + _fm.drives[i].path + " " + _fm.drives[i].name + " ");
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

Element UI::createHistoryOverlay(const Element &main_view) {
    Element backdrop = main_view | dim;

    Elements history_rows;
    for (size_t i = 0; i < _fm.history.size(); ++i) {
        fs::path absPath = _fm.history[i];
        std::string display = formatHistoryPath(absPath, _fm.cwd);

        auto h = text(" " + display + " ");
        if (i == _fm.selHistIdx) { h = h | bgcolor(Color::BlueLight) | color(Color::Black) | bold; }
        history_rows.push_back(h);
    }

    auto history_window =
        window(text(" History ") | bold | bgcolor(Color::DarkGreen) | color(Color::White),
               vbox(history_rows));

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
        {"r", "rename"},
        {"R", "run from term"},
        {"m", "move"},
        {"d", "delete"},
        {"n", "new file"},
        {"N", "new directory"},
        {"u", "undo"},
        {"y", "copy"},
        {"Y", "copy to system"},
        {"x", "cut"},
        {"p", "paste"},
        {"c", "change dir"},
        {"C", "change drive"},
        {"space", "file/dir-picker"},
        {"Return", "expand/collapse"},
        {"Esc", "collapse all"},
        {"q", "quit to last"},
        {"^", "history"},
        {"Ctrl+c", "quit"},
        {"?", "show help"},
    };

    Elements help_rows;

    for (auto &[key, desc] : help_entries) {
        help_rows.push_back(hbox(
            {text(" " + key) | color(Color::Cyan), filler(), text(desc) | color(Color::White)}));
    }

    auto help_window =
        window(text(" Help ") | bold | bgcolor(Color::DarkGreen) | color(Color::White),
               vbox(help_rows)) |
        bgcolor(Color::Black) | size(WIDTH, EQUAL, 50);

    return dbox({main_view | dim, center(help_window)});
}

Element UI::createFzfMenuOverlay(const Element &main_view) {
    std::vector<std::pair<std::string, std::string>> fzf_entries = {
        {"f/F (file/cwd)", "file-picker (edit)"},
        {"e/E (file/cwd)", "dir-picker (cd)"},
        {"o/O (file/cwd)", "file-picker (open)"},
        {"c/C (file/cwd)", "file-picker (copy)"},
    };

    Elements fzf_rows;

    for (auto &[key, desc] : fzf_entries) {
        fzf_rows.push_back(hbox(
            {text(" " + key) | color(Color::Cyan), filler(), text(desc) | color(Color::White)}));
    }

    auto fzf_window =
        window(text(" FZF Menu ") | bold | bgcolor(Color::DarkGreen) | color(Color::White),
               vbox(fzf_rows)) |
        bgcolor(Color::Black) | size(WIDTH, EQUAL, 50);

    return dbox({main_view | dim, center(fzf_window)});
}

Element UI::fileElement(const fs::path &p, bool isDir, const std::set<fs::path> &expandedDirs) {
    // Combined icon + color map
    static const std::unordered_map<std::string, std::pair<std::string, Color>> fileMap = {
        // C / C++ / C# / Obj-C
        {".c", {"\uE61E ", Color::Green}},
        {".cpp", {"\uE61E ", Color::Green}},
        {".cc", {"\uE61E ", Color::Green}},
        {".h", {"\uE61F ", Color::Green}},
        {".hpp", {"\uE61F ", Color::Green}},
        {".cs", {"\uE7A8 ", Color::BlueLight}},

        // Scripts
        {".py", {"\uE606 ", Color(75, 139, 190)}},
        {".sh", {"\uE795 ", Color::LightGreen}},
        {".bat", {"\uE7B5 ", Color::Yellow}},
        {".m", {"\uE6A9 ", Color::DarkGreen}},
        {".js", {" ", Color(240, 230, 140)}},
        {".ts", {"\uE628 ", Color::Blue}},
        {".rb", {"\uE21E ", Color::Red}},
        {".ps1", {"󰞷 ", Color(0, 183, 255)}},

        // Web / markup
        {".html", {"\uE736 ", Color::Red}},
        {".css", {"\uE749 ", Color::Blue}},
        {".php", {"\uE73D ", Color::Purple}},
        {".xml", {"\uF1C3 ", Color::Cyan}},
        {".json", {" ", Color(255, 215, 0)}},
        {".yaml", {"\uF1C3 ", Color::Cyan}},
        {".md", {"󰪷 ", Color(0, 184, 218)}},
        {".txt", {"󰈙 ", Color(0, 206, 255)}},

        // Documents / Office
        {".pdf", {"\uF1C1 ", Color::Red}},
        {".doc", {"\uF1C2 ", Color::Blue}},
        {".docx", {"\uF1C2 ", Color::Blue}},
        {".xls", {"\uF1C8 ", Color::Green}},
        {".xlsx", {"\uF1C8 ", Color::Green}},
        {".csv", {"\uF1C1 ", Color(145, 193, 47)}},
        {".ppt", {"\uF1C4 ", Color::Red}},
        {".pptx", {"\uF1C4 ", Color::Red}},

        // Images
        {".png", {"\uF1C5 ", Color::Magenta}},
        {".jpg", {"\uF1C5 ", Color::Magenta}},
        {".jpeg", {"\uF1C5 ", Color::Magenta}},
        {".gif", {"\uF1C5 ", Color::Magenta}},
        {".bmp", {"\uF1C5 ", Color::Magenta}},
        {".svg", {"\uF1C5 ", Color::Magenta}},

        // Audio / Video
        {".mp3", {"\uF001 ", Color::Purple}},
        {".wav", {"\uF001 ", Color::Purple}},
        {".ogg", {"\uF001 ", Color::Purple}},
        {".flac", {"\uF001 ", Color::Purple}},
        {".mp4", {"\uF03D ", Color::Cyan}},
        {".mkv", {"\uF03D ", Color::Cyan}},
        {".avi", {"\uF03D ", Color::Cyan}},

        // Archives / compressed
        {".zip", {"\uF410 ", Color::Yellow}},
        {".tar", {"\uF410 ", Color::Yellow}},
        {".gz", {"\uF410 ", Color::Yellow}},
        {".bz2", {"\uF410 ", Color::Yellow}},
        {".rar", {"\uF410 ", Color::Yellow}},
        {".7z", {"\uF410 ", Color::Yellow}},

        // Others
        {".iso", {"\uF7C2 ", Color::BlueLight}},
        {".exe", {"\uF013 ", Color(0, 250, 154)}}

        // // Defaults (folders and files)
        // {"folder_closed", {"\uF07B ", Color::Blue}}, // closed folder
        // {"folder_open", {"\uF07C ", Color::Blue}},   // open folder
        // {"file", {"\uF016 ", Color::White}}          // generic file
    };

    std::string iconStr;
    Color col;

    if (isDir) {
        iconStr = expandedDirs.count(p) ? "📂 " : "📁 "; // open/closed folder
        return text(iconStr + p.filename().string());
    } else {
        std::string ext = p.has_extension() ? p.extension().string() : "";
        auto it = fileMap.find(ext);
        if (it != fileMap.end()) {
            iconStr = it->second.first;
            col = it->second.second;
        } else {
            iconStr = "\uF016 "; // default file icon
            col = Color::White;
        }
    }
    return text(iconStr + p.filename().string()) | color(col);
}
UI::Layout UI::Layout::compute(int screen_width, int max_expanded_depth) {
    Layout layout;
    layout.total_width = screen_width;
    layout.max_indent_width = indent_per_level * (max_expanded_depth + 1);

    int fixed_columns =
        layout.max_indent_width + icon_width + type_col_width + size_col_width + spacing * 2;

    int available_for_name = screen_width - fixed_columns;
    int upper = std::max(20, available_for_name);
    layout.max_name_width = std::clamp(available_for_name, 10, upper);

    layout.type_column = layout.max_indent_width + icon_width + layout.max_name_width + spacing;

    int name_label_length = static_cast<int>(std::string("Name").length());
    layout.spacer_width = std::max(
        layout.type_column - (layout.max_indent_width + icon_width + name_label_length), 1);

    return layout;
}
