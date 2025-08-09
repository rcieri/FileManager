cmake_minimum_required(VERSION 3.15)
project(FileManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Fetch FTXUI ---
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(ftxui)

# --- Fetch nlohmann/json ---
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3   # pick a stable release instead of 'main'
)
FetchContent_MakeAvailable(json)

# --- Your executable ---
add_executable(FileManager
    src/FileManager.cpp
    src/Ui.cpp
    main.cpp
)

target_include_directories(FileManager PRIVATE include)

target_link_libraries(FileManager
    PRIVATE
        ftxui::screen
        ftxui::dom
        ftxui::component
        nlohmann_json::nlohmann_json
)

install(TARGETS FileManager
    RUNTIME DESTINATION bin
)
