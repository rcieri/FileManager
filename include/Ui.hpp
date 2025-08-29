#ifndef UI_HPP_
#define UT_HPP_

#include "FileManager.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <regex>
#include <unordered_map>

class UI {
  public:
    UI(const FileManager &fm) : _fm(fm) {}

    ftxui::Element render(ftxui::ScreenInteractive &screen);

  private:
    const FileManager &_fm;

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

        static Layout compute(int screen_width, int max_expanded_depth) {
            Layout layout;
            layout.total_width = screen_width;
            layout.max_indent_width = indent_per_level * (max_expanded_depth + 1);

            int fixed_columns = layout.max_indent_width + icon_width + type_col_width +
                                size_col_width + spacing * 2;

            int available_for_name = screen_width - fixed_columns;
            int upper = std::max(20, available_for_name);
            layout.max_name_width = std::clamp(available_for_name, 10, upper);

            layout.type_column =
                layout.max_indent_width + icon_width + layout.max_name_width + spacing;

            int name_label_length = static_cast<int>(std::string("Name").length());
            layout.spacer_width = std::max(
                layout.type_column - (layout.max_indent_width + icon_width + name_label_length), 1);

            return layout;
        }
    };
    ftxui::Element createPromptBox(const ftxui::Element &main_view, const std::string &title,
                                   std::optional<ftxui::Element> body_opt = std::nullopt);

    ftxui::Element createErrorOverlay(const ftxui::Element &main_view);
    ftxui::Element createHelpOverlay(const ftxui::Element &main_view);
    ftxui::Element createDriveSelectOverlay(const ftxui::Element &main_view);
    ftxui::Element createHistoryOverlay(const ftxui::Element &main_view);
    ftxui::Element createOverlay(const ftxui::Element &main_view);

    ftxui::Element fileElement(const fs::path &p, bool isDir,
                               const std::set<fs::path> &expandedDirs);
};

#endif
