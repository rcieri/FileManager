// Ui.hpp
#pragma once

#include "FileManager.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

class UI {
  public:
    UI(const FileManager &fm) : _fm(fm) {}
    ftxui::Element render(ftxui::ScreenInteractive &screen);
    ftxui::Element createModalBox(const ftxui::Element &main_view, const std::string &title,
                                  ftxui::Element body);
    ftxui::Element createHelpOverlay(const ftxui::Element &main_view);
    ftxui::Element createDriveSelect(const ftxui::Element &main_view);
    ftxui::Element applyStyle(const fs::path &p, ftxui::Element e);
    std::string getFileIcon(const fs::path &p);

  private:
    const FileManager &_fm;
};
