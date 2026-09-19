#include "Game.hpp"
#include "LevelIO.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
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

    if (argc >= 3 && std::string(argv[1]) == "--capture-ui") {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        sv::Game game;
        return game.captureUiSnapshots(argv[2]) ? 0 : 1;
    }

    // --level <path> launches an alternate map without replacing the gatehouse.
    std::string levelPath;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--level") {
            levelPath = argv[i + 1];
            break;
        }
    }

    sv::Game game{levelPath};
    game.run();
    return 0;
}
