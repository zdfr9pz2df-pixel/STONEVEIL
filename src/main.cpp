#include "Game.hpp"
#include "LevelIO.hpp"
#include "Project.hpp"

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "--validate-project") {
        sv::ProjectDocument project;
        std::string error;
        if (!project.open(argv[2], error)) { std::cerr << error << '\n'; return 1; }
        bool blocked = false;
        for (const auto& issue : project.validate()) {
            std::cout << (issue.severity == sv::IssueSeverity::Error ? "ERROR: " : "WARNING: ") << issue.message << '\n';
            blocked = blocked || issue.severity == sv::IssueSeverity::Error;
        }
        return blocked ? 1 : 0;
    }
    if (argc >= 3 && std::string(argv[1]) == "--validate") {
        sv::LevelDefinition level;
        std::string error;
        if (!sv::LevelIO::load(argv[2], level, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        std::cout << "Valid STONEVEIL level: " << level.name << '\n';
        return 0;
    }

    // --level <path> launches an alternate map without replacing the gatehouse.
    std::string levelPath;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--level") {
            levelPath = argv[i + 1];
            break;
        }
    }

    std::string projectFile;
    bool runtimeOnly = false;
    const auto executableDirectory = std::filesystem::path{GetApplicationDirectory()};
    std::ifstream marker(executableDirectory / "stoneveil.game");
    if (marker) {
        std::string relative;
        std::getline(marker, relative);
        if (!sv::ProjectDocument::resolveInside(executableDirectory.string(), relative, projectFile)) {
            std::cerr << "Invalid exported-game project path.\n"; return 1;
        }
        runtimeOnly = true;
    } else {
        for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == "--project") projectFile = argv[i + 1];
        if (projectFile.empty() && levelPath.empty() && std::filesystem::exists(executableDirectory / "stoneveil.stoneveil"))
            projectFile = (executableDirectory / "stoneveil.stoneveil").string();
    }
    if (!projectFile.empty()) {
        sv::ProjectDocument project;
        std::string error;
        if (!project.open(projectFile, error)) { std::cerr << error << '\n'; return 1; }
    }
    const bool capture = argc >= 3 && std::string(argv[1]) == "--capture-ui";
    if (capture) SetConfigFlags(FLAG_WINDOW_HIDDEN);
    sv::Game game{levelPath, projectFile, runtimeOnly};
    if (capture) return game.captureUiSnapshots(argv[2]) ? 0 : 1;
    game.run();
    return 0;
}
