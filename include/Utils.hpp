#include <filesystem>
#include <fstream>
#include <shlobj.h>
#include <string>
#include <windows.h>

namespace fs = std::filesystem;

void writeToAppDataRoamingFile(std::string changePath) {
    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &path))) {
        char buffer[MAX_PATH];
        size_t convertedChars = 0;
        wcstombs_s(&convertedChars, buffer, MAX_PATH, path, MAX_PATH - 1);
        CoTaskMemFree(path);
        std::string appDataDir = std::string(buffer) + "\\FileManager";
        fs::create_directories(appDataDir);
        std::string filePath = appDataDir + "\\fm.txt";
        std::ofstream outFile(filePath);
        outFile << changePath;
        outFile.close();
    }
}
