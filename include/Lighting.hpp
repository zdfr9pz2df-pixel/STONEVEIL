#pragma once

#include "Material.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace sv {

class Dungeon;

// A placeable light type. Definitions are data, not art: the tint is a
// functional placeholder in the same spirit as MaterialDefinition swatches, and
// authored levels only ever store the stable id.
struct LightDefinition {
    std::string id;
    std::string category;
    std::string name;
    MaterialSwatch tint;
    float intensity{1.0f};
    float radius{5.0f};
};

// An authored light occupying one logical cell. Cells use the same integer
// convention as spawns, pickups, and enemies; the emitter itself is treated as
// sitting at the center of that cell, (x + 0.5, y + 0.5).
struct LightPlacement {
    int x{};
    int y{};
    std::string lightId;
};

// Accumulated warm contribution at a sampled point, in 0..1 per channel.
struct LightSample {
    float red{0.0f};
    float green{0.0f};
    float blue{0.0f};

    bool lit() const { return red > 0.0f || green > 0.0f || blue > 0.0f; }
};

const std::vector<LightDefinition>& lightCatalog();
const LightDefinition* findLight(const std::string& id);
const std::string& defaultLightId();

class Lighting {
public:
    static constexpr std::size_t MaxLightsPerLevel = 512;

    // Normalized 0..1 contribution of a light at the given distance in cells.
    static float falloff(double distance, float radius);

    // Deterministic grid line-of-sight between two cells. The endpoints
    // themselves are never treated as occluders, so a sconce sitting in a wall
    // cell still lights the corridor in front of it.
    static bool reaches(const Dungeon& dungeon, int fromX, int fromY, int toX, int toY);

    // Warm light arriving at a point. `pointX`/`pointY` are continuous world
    // coordinates in cell units; `viewCellX`/`viewCellY` is the open cell the
    // point is seen from, which is what occlusion is traced against.
    static LightSample sampleAt(const Dungeon& dungeon,
                                double pointX,
                                double pointY,
                                int viewCellX,
                                int viewCellY);
};

} // namespace sv
