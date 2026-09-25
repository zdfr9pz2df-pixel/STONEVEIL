#pragma once
#include "Lighting.hpp"

#include <string>

namespace sv {

class Dungeon;
class PlayerState;

class Raycaster {
public:
    void setContentRoot(std::string root) { contentRoot_ = std::move(root); }
    void setLightingSettings(LightingSettings settings) { lightingSettings_ = settings; }
    const LightingSettings& lightingSettings() const { return lightingSettings_; }
    void draw(const Dungeon& dungeon, const PlayerState& player) const;
private:
    std::string contentRoot_;
    LightingSettings lightingSettings_{};
};

} // namespace sv
