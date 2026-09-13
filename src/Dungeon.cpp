#include "Dungeon.hpp"

namespace sv {

Dungeon::Dungeon() {
    for (auto& row : tiles_) row.fill(Tile::Wall);

    const std::array<std::string, Height> map = {
        "################",
        "#......#.......#",
        "#......#.......#",
        "#..##..#..###..#",
        "#..##..D..#....#",
        "#......#..#....#",
        "###.####..#....#",
        "#............#.#",
        "#............#.#",
        "#.######.#####.#",
        "#.#....#.......#",
        "#.#....#..###..#",
        "#...##....#....#",
        "#...##....#..E.#",
        "#..............#",
        "################"
    };

    for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x) {
            switch (map[y][x]) {
                case '#': tiles_[y][x] = Tile::Wall; break;
                case 'D': tiles_[y][x] = Tile::DoorClosed; break;
                case 'E': tiles_[y][x] = Tile::Exit; break;
                default: tiles_[y][x] = Tile::Floor; break;
            }
        }
    }

    pickups_.push_back({Pickup::Type::Key, 5, 2, false});
    pickups_.push_back({Pickup::Type::Potion, 10, 8, false});
    pickups_.push_back({Pickup::Type::Potion, 3, 13, false});

    enemies_.push_back({10, 2, 18, true, 0.0f});
    enemies_.push_back({11, 8, 24, true, 0.0f});
    enemies_.push_back({6, 13, 30, true, 0.0f});
}

Tile Dungeon::tile(int x, int y) const {
    if (!inBounds(x, y)) return Tile::Wall;
    return tiles_[y][x];
}

bool Dungeon::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < Width && y < Height;
}

bool Dungeon::blocksMovement(int x, int y) const {
    const auto t = tile(x, y);
    return t == Tile::Wall || t == Tile::DoorClosed;
}

bool Dungeon::blocksSight(int x, int y) const {
    const auto t = tile(x, y);
    return t == Tile::Wall || t == Tile::DoorClosed;
}

bool Dungeon::openDoor(int x, int y, bool hasKey) {
    if (!inBounds(x, y) || tiles_[y][x] != Tile::DoorClosed || !hasKey) return false;
    tiles_[y][x] = Tile::DoorOpen;
    return true;
}

} // namespace sv
