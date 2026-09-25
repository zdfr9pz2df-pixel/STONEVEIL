#pragma once

#include "Material.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sv {

class Dungeon;

enum class LightKind {
    Point,
    Spot,
    Directional,
};

enum class LightAnimation {
    Steady,
    Flicker,
    Pulse,
};

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
    LightKind kind{LightKind::Point};
    float directionX{1.0f};
    float directionY{0.0f};
    float innerConeDegrees{24.0f};
    float outerConeDegrees{38.0f};
    bool castsShadows{true};
    float volumetricContribution{1.0f};
    LightAnimation animation{LightAnimation::Steady};
    float animationAmplitude{0.0f};
    float animationFrequency{1.0f};
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

// Independent controls for the light accumulation pass. A later quality-tier
// UI can map to these without changing the renderer or authored level format.
struct LightingSettings {
    bool enabled{true};
    bool animateLights{true};
    bool shadows{true};
    std::size_t maxActiveLights{128};
    std::size_t maxShadowCastingLights{16};
    float shadowDistance{24.0f};
};

struct EvaluatedLight {
    int x{};
    int y{};
    const LightDefinition* definition{};
    float intensity{};
    bool shadowed{};
};

// Per-frame spatial light bins and cached grid visibility. Samples only visit
// lights that can influence their cell; shadow-casting lights perform their DDA
// visibility trace once per affected cell instead of once per rendered block.
class LightingFrame {
public:
    LightSample sampleAt(double pointX, double pointY, int viewCellX, int viewCellY) const;
    std::size_t activeLightCount() const { return lights_.size(); }
    std::size_t shadowCastingLightCount() const { return shadowCastingLightCount_; }
    std::size_t candidateCountAt(int cellX, int cellY) const;

private:
    friend class Lighting;

    struct CellInfluence {
        std::uint16_t lightIndex{};
        bool visible{true};
    };

    std::size_t cellIndex(int x, int y) const;

    int width_{};
    int height_{};
    std::size_t shadowCastingLightCount_{};
    std::vector<EvaluatedLight> lights_;
    std::vector<std::vector<CellInfluence>> cells_;
};

const std::vector<LightDefinition>& lightCatalog();
const LightDefinition* findLight(const std::string& id);
const std::string& defaultLightId();

class Lighting {
public:
    static constexpr std::size_t MaxLightsPerLevel = 512;

    // Normalized 0..1 contribution of a light at the given distance in cells.
    static float falloff(double distance, float radius);

    // Builds the scalable accumulation state once for a rendered frame.
    static LightingFrame buildFrame(const Dungeon& dungeon,
                                    double timeSeconds,
                                    const LightingSettings& settings = {});

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
