#pragma once

namespace sv {

class Dungeon;
class PlayerState;

class Raycaster {
public:
    void draw(const Dungeon& dungeon, const PlayerState& player) const;
};

} // namespace sv
