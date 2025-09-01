#ifndef UI_HPP_
#define UI_HPP_

#include "FileManager.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <filesystem>
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

        static Layout compute(int screen_width, int max_expanded_depth);
    };
    ftxui::Element createPromptBox(const ftxui::Element &main_view, const std::string &title,
                                   std::optional<ftxui::Element> body_opt = std::nullopt);

    ftxui::Element createErrorOverlay(const ftxui::Element &main_view);
    ftxui::Element createHelpOverlay(const ftxui::Element &main_view);
    ftxui::Element createDriveSelectOverlay(const ftxui::Element &main_view);
    ftxui::Element createHistoryOverlay(const ftxui::Element &main_view);
    ftxui::Element createOverlay(const ftxui::Element &main_view);

    ftxui::Element fileElement(const std::filesystem::path &p, bool isDir,
                               const std::set<std::filesystem::path> &expandedDirs);
};

#endif
