cmake_minimum_required(VERSION 3.15)
project(FileManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically fetch and include FTXUI
include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        main  # You can pin to a specific tag or commit if needed
)

FetchContent_MakeAvailable(ftxui)

add_executable(FileManager
    main.cpp
    FileManager.cpp
)

target_link_libraries(FileManager
    PRIVATE
        ftxui::screen
        ftxui::dom
        ftxui::component
)

install(TARGETS FileManager
    RUNTIME DESTINATION bin
)
