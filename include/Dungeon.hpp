#pragma once

#include <array>
#include <string>
#include <vector>

namespace sv {

enum class Tile : int {
    Floor = 0,
    Wall = 1,
    DoorClosed = 2,
    DoorOpen = 3,
    Exit = 4,
};

struct Pickup {
    enum class Type { Key, Potion };
    Type type{};
    int x{};
    int y{};
    bool taken{false};
};

struct Enemy {
    int x{};
    int y{};
    int hp{18};
    bool alive{true};
    float attackCooldown{0.0f};
};

class Dungeon {
public:
    static constexpr int Width = 16;
    static constexpr int Height = 16;

    Dungeon();

    Tile tile(int x, int y) const;
    bool inBounds(int x, int y) const;
    bool blocksMovement(int x, int y) const;
    bool blocksSight(int x, int y) const;
    bool openDoor(int x, int y, bool hasKey);

    std::vector<Pickup>& pickups() { return pickups_; }
    const std::vector<Pickup>& pickups() const { return pickups_; }
    std::vector<Enemy>& enemies() { return enemies_; }
    const std::vector<Enemy>& enemies() const { return enemies_; }

private:
    std::array<std::array<Tile, Width>, Height> tiles_{};
    std::vector<Pickup> pickups_;
    std::vector<Enemy> enemies_;
};

} // namespace sv
