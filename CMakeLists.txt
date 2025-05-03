cmake_minimum_required(VERSION 3.15)
project(FileManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where FTXUI was installed; adjust if you installed to a different prefix
set(FTXUI_DIR "C:/Program Files (x86)/ftxui/lib/cmake/ftxui")

# Tell CMake to find FTXUI via its config file
find_package(ftxui COMPONENTS screen dom component REQUIRED PATHS "${FTXUI_DIR}")

add_executable(FileManager
    main.cpp
)

# Link against the FTXUI libraries
target_link_libraries(FileManager
    PRIVATE
        ftxui::screen
        ftxui::dom
        ftxui::component
)

# Optionally install the binary
install(TARGETS FileManager
    RUNTIME DESTINATION bin
)

