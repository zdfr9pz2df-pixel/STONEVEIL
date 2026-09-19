#pragma once
#include <string>

namespace sv {

class Dungeon;
class PlayerState;

class Raycaster {
public:
    void setContentRoot(std::string root) { contentRoot_ = std::move(root); }
    void draw(const Dungeon& dungeon, const PlayerState& player) const;
private:
    std::string contentRoot_;
};

} // namespace sv
