// Ui.hpp
#pragma once

#include "FileManager.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

class UI {
  public:
    UI(const FileManager &fm) : _fm(fm) {}

    struct Layout {
        int total_width;
        int max_indent_width;
        int max_name_width;
        int type_column;
        int spacer_width;

        static constexpr int indent_per_level = 2;
        static constexpr int icon_width = 2;
        static constexpr int type_col_width = 6;
        static constexpr int size_col_width = 9;
        static constexpr int spacing = 5;

        static Layout compute(int screen_width) {
            Layout layout;
            layout.total_width = screen_width;
            layout.max_indent_width = indent_per_level * 20;

            int fixed_columns = layout.max_indent_width + icon_width + type_col_width +
                                size_col_width + spacing * 2;

            layout.max_name_width = std::clamp(screen_width - fixed_columns, 10, 60);
            layout.type_column =
                layout.max_indent_width + icon_width + layout.max_name_width + spacing;

            int name_label_length = static_cast<int>(std::string("Name").length());
            layout.spacer_width = std::max(
                layout.type_column - (layout.max_indent_width + icon_width + name_label_length), 1);

            return layout;
        }
    };

    ftxui::Element render(ftxui::ScreenInteractive &screen);
    ftxui::Element createModalBox(const ftxui::Element &main_view, const std::string &title,
                                  ftxui::Element body);
    ftxui::Element createErrorOverlay(const ftxui::Element &main_view);
    ftxui::Element createHelpOverlay(const ftxui::Element &main_view);
    ftxui::Element createDriveSelect(const ftxui::Element &main_view);
    ftxui::Element applyStyle(const fs::path &p, ftxui::Element e);
    std::string getFileIcon(const fs::path &p);

  private:
    const FileManager &_fm;
};
