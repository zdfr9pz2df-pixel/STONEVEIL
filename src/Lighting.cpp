#include "Lighting.hpp"

#include "Dungeon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace sv {
namespace {

double cellCenter(int coordinate) {
    return static_cast<double>(coordinate) + 0.5;
}

float saturate(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float degreesToCosine(float degrees) {
    constexpr float Pi = 3.14159265358979323846f;
    return std::cos(degrees * Pi / 180.0f);
}

float coneAttenuation(const EvaluatedLight& light, double offsetX, double offsetY) {
    if (light.definition->kind != LightKind::Spot) return 1.0f;
    const double distance = std::sqrt(offsetX * offsetX + offsetY * offsetY);
    if (distance <= 1.0e-8) return 1.0f;

    const float directionLength = std::sqrt(light.definition->directionX * light.definition->directionX +
                                            light.definition->directionY * light.definition->directionY);
    if (directionLength <= 1.0e-5f) return 1.0f;
    const float lightToPointX = static_cast<float>(-offsetX / distance);
    const float lightToPointY = static_cast<float>(-offsetY / distance);
    const float cosine = lightToPointX * (light.definition->directionX / directionLength) +
                         lightToPointY * (light.definition->directionY / directionLength);
    const float inner = degreesToCosine(light.definition->innerConeDegrees);
    const float outer = degreesToCosine(light.definition->outerConeDegrees);
    if (inner <= outer) return cosine >= inner ? 1.0f : 0.0f;
    const float t = saturate((cosine - outer) / (inner - outer));
    return t * t * (3.0f - 2.0f * t);
}

float animationPhase(const LightPlacement& placement) {
    std::uint32_t hash = 2166136261u;
    const auto mix = [&hash](std::uint32_t value) {
        hash ^= value;
        hash *= 16777619u;
    };
    mix(static_cast<std::uint32_t>(placement.x));
    mix(static_cast<std::uint32_t>(placement.y));
    for (const unsigned char value : placement.lightId) mix(value);
    return static_cast<float>(hash & 0xffffu) * (6.28318530718f / 65535.0f);
}

float animatedIntensity(const LightPlacement& placement,
                        const LightDefinition& definition,
                        double timeSeconds,
                        bool animate) {
    if (!animate || definition.animation == LightAnimation::Steady || definition.animationAmplitude <= 0.0f) {
        return definition.intensity;
    }

    const float phase = animationPhase(placement);
    const float time = static_cast<float>(timeSeconds) * definition.animationFrequency * 6.28318530718f;
    float wave = std::sin(time + phase);
    if (definition.animation == LightAnimation::Flicker) {
        wave = 0.55f * wave + 0.30f * std::sin(time * 2.37f + phase * 1.71f) +
               0.15f * std::sin(time * 5.11f + phase * 0.43f);
    }
    return std::max(0.0f, definition.intensity * (1.0f + definition.animationAmplitude * wave));
}

void accumulate(const EvaluatedLight& light,
                double pointX,
                double pointY,
                bool visible,
                LightSample& sample) {
    if (!visible || light.definition == nullptr || light.intensity <= 0.0f) return;
    const double offsetX = cellCenter(light.x) - pointX;
    const double offsetY = cellCenter(light.y) - pointY;
    const double distanceSquared = offsetX * offsetX + offsetY * offsetY;
    const double distance = std::sqrt(distanceSquared);
    const float attenuation = light.definition->kind == LightKind::Directional
        ? 1.0f
        : Lighting::falloff(distance, light.definition->radius);
    if (attenuation <= 0.0f) return;
    const float cone = coneAttenuation(light, offsetX, offsetY);
    if (cone <= 0.0f) return;

    const float strength = attenuation * cone * light.intensity;
    sample.red += strength * (static_cast<float>(light.definition->tint.r) / 255.0f);
    sample.green += strength * (static_cast<float>(light.definition->tint.g) / 255.0f);
    sample.blue += strength * (static_cast<float>(light.definition->tint.b) / 255.0f);
}

void clampSample(LightSample& sample) {
    sample.red = saturate(sample.red);
    sample.green = saturate(sample.green);
    sample.blue = saturate(sample.blue);
}

} // namespace

const std::vector<LightDefinition>& lightCatalog() {
    static const std::vector<LightDefinition> lights = {
        {"light.torch.iron-sconce", "Torch", "Iron Sconce", {255, 176, 92}, 1.00f, 5.5f,
         LightKind::Point, 1.0f, 0.0f, 24.0f, 38.0f, true, 0.85f,
         LightAnimation::Flicker, 0.12f, 1.7f},
        {"light.torch.pitch-brazier", "Torch", "Pitch Brazier", {255, 148, 68}, 1.25f, 7.5f,
         LightKind::Point, 1.0f, 0.0f, 24.0f, 38.0f, true, 1.0f,
         LightAnimation::Flicker, 0.16f, 1.25f},
        {"light.torch.tallow-candle", "Torch", "Tallow Candle", {255, 208, 150}, 0.55f, 3.0f,
         LightKind::Point, 1.0f, 0.0f, 24.0f, 38.0f, true, 0.45f,
         LightAnimation::Flicker, 0.08f, 2.1f},
        {"light.magic.arcane-wisp", "Magic", "Arcane Wisp", {105, 151, 255}, 0.85f, 6.0f,
         LightKind::Point, 1.0f, 0.0f, 24.0f, 38.0f, false, 1.0f,
         LightAnimation::Pulse, 0.18f, 0.55f},
    };
    return lights;
}

const LightDefinition* findLight(const std::string& id) {
    const auto& lights = lightCatalog();
    const auto found = std::find_if(lights.begin(), lights.end(), [&id](const LightDefinition& light) {
        return light.id == id;
    });
    return found == lights.end() ? nullptr : &*found;
}

const std::string& defaultLightId() {
    static const std::string fallback;
    const auto& lights = lightCatalog();
    return lights.empty() ? fallback : lights.front().id;
}

float Lighting::falloff(double distance, float radius) {
    if (radius <= 0.0f) return 0.0f;
    if (distance <= 0.0) return 1.0f;
    const double normalized = distance / static_cast<double>(radius);
    if (normalized >= 1.0) return 0.0f;

    // Stable inverse-square-like attenuation with a smooth finite-radius
    // window. The denominator prevents the singular source hotspot while the
    // quartic window reaches zero without a hard light boundary.
    const double inverseSquare = 1.0 / (1.0 + 0.35 * distance * distance);
    const double window = 1.0 - normalized * normalized * normalized * normalized;
    return static_cast<float>(inverseSquare * window * window);
}

std::size_t LightingFrame::cellIndex(int x, int y) const {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}

std::size_t LightingFrame::candidateCountAt(int cellX, int cellY) const {
    if (cellX < 0 || cellY < 0 || cellX >= width_ || cellY >= height_) return 0;
    return cells_[cellIndex(cellX, cellY)].size();
}

LightSample LightingFrame::sampleAt(double pointX, double pointY, int viewCellX, int viewCellY) const {
    LightSample sample;
    if (viewCellX < 0 || viewCellY < 0 || viewCellX >= width_ || viewCellY >= height_) return sample;
    for (const auto& influence : cells_[cellIndex(viewCellX, viewCellY)]) {
        if (influence.lightIndex >= lights_.size()) continue;
        accumulate(lights_[influence.lightIndex], pointX, pointY, influence.visible, sample);
    }
    clampSample(sample);
    return sample;
}

LightingFrame Lighting::buildFrame(const Dungeon& dungeon,
                                   double timeSeconds,
                                   const LightingSettings& settings) {
    LightingFrame frame;
    frame.width_ = dungeon.width();
    frame.height_ = dungeon.height();
    frame.cells_.resize(static_cast<std::size_t>(frame.width_) * static_cast<std::size_t>(frame.height_));
    if (!settings.enabled || settings.maxActiveLights == 0) return frame;

    struct RankedLight {
        EvaluatedLight light;
        float importance{};
    };
    std::vector<RankedLight> ranked;
    ranked.reserve(dungeon.lights().size());
    for (const auto& placement : dungeon.lights()) {
        const auto* definition = findLight(placement.lightId);
        if (definition == nullptr || definition->intensity <= 0.0f ||
            (definition->kind != LightKind::Directional && definition->radius <= 0.0f)) continue;
        const float evaluatedIntensity = animatedIntensity(placement, *definition, timeSeconds, settings.animateLights);
        const float coverage = definition->kind == LightKind::Directional
            ? static_cast<float>(dungeon.width() * dungeon.height())
            : definition->radius * definition->radius;
        ranked.push_back({{placement.x, placement.y, definition, evaluatedIntensity, false},
                          evaluatedIntensity * coverage});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const RankedLight& left, const RankedLight& right) {
        return left.importance > right.importance;
    });
    const std::size_t activeLimit = std::min<std::size_t>({settings.maxActiveLights, ranked.size(), 65535u});
    frame.lights_.reserve(activeLimit);
    for (std::size_t i = 0; i < activeLimit; ++i) {
        auto light = ranked[i].light;
        light.shadowed = settings.shadows && light.definition->castsShadows &&
                         frame.shadowCastingLightCount_ < settings.maxShadowCastingLights;
        if (light.shadowed) ++frame.shadowCastingLightCount_;
        frame.lights_.push_back(light);
    }

    for (std::size_t lightIndex = 0; lightIndex < frame.lights_.size(); ++lightIndex) {
        const auto& light = frame.lights_[lightIndex];
        const int radius = light.definition->kind == LightKind::Directional
            ? std::max(frame.width_, frame.height_)
            : static_cast<int>(std::ceil(light.definition->radius));
        const int minX = light.definition->kind == LightKind::Directional ? 0 : std::max(0, light.x - radius);
        const int maxX = light.definition->kind == LightKind::Directional ? frame.width_ - 1 : std::min(frame.width_ - 1, light.x + radius);
        const int minY = light.definition->kind == LightKind::Directional ? 0 : std::max(0, light.y - radius);
        const int maxY = light.definition->kind == LightKind::Directional ? frame.height_ - 1 : std::min(frame.height_ - 1, light.y + radius);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                if (light.definition->kind != LightKind::Directional) {
                    const double dx = cellCenter(light.x) - cellCenter(x);
                    const double dy = cellCenter(light.y) - cellCenter(y);
                    const double expandedRadius = static_cast<double>(light.definition->radius) + 0.72;
                    if (dx * dx + dy * dy >= expandedRadius * expandedRadius) continue;
                }
                bool visible = true;
                if (light.shadowed && light.definition->kind != LightKind::Directional) {
                    const double dx = static_cast<double>(light.x - x);
                    const double dy = static_cast<double>(light.y - y);
                    const double shadowDistance = std::sqrt(dx * dx + dy * dy);
                    if (shadowDistance <= settings.shadowDistance) {
                        visible = reaches(dungeon, light.x, light.y, x, y);
                    }
                }
                frame.cells_[frame.cellIndex(x, y)].push_back(
                    {static_cast<std::uint16_t>(lightIndex), visible});
            }
        }
    }
    return frame;
}

bool Lighting::reaches(const Dungeon& dungeon, int fromX, int fromY, int toX, int toY) {
    if (fromX == toX && fromY == toY) return true;

    constexpr double CornerEpsilon = 1.0e-12;
    const int stepX = toX > fromX ? 1 : (toX < fromX ? -1 : 0);
    const int stepY = toY > fromY ? 1 : (toY < fromY ? -1 : 0);
    const double distanceX = std::abs(static_cast<double>(toX - fromX));
    const double distanceY = std::abs(static_cast<double>(toY - fromY));
    const double infinity = std::numeric_limits<double>::infinity();
    const double deltaX = stepX == 0 ? infinity : 1.0 / distanceX;
    const double deltaY = stepY == 0 ? infinity : 1.0 / distanceY;
    double boundaryX = stepX == 0 ? infinity : 0.5 / distanceX;
    double boundaryY = stepY == 0 ? infinity : 0.5 / distanceY;
    int cellX = fromX;
    int cellY = fromY;

    const auto blocksIntermediateCell = [&](int x, int y) {
        if ((x == fromX && y == fromY) || (x == toX && y == toY)) return false;
        return dungeon.blocksSight(x, y);
    };

    while (cellX != toX || cellY != toY) {
        if (boundaryX + CornerEpsilon < boundaryY) {
            cellX += stepX;
            boundaryX += deltaX;
        } else if (boundaryY + CornerEpsilon < boundaryX) {
            cellY += stepY;
            boundaryY += deltaY;
        } else {
            // At an exact grid corner, check both cells touched by the ray.
            // This prevents light leaking through a diagonal crack between
            // two solid cells while still exempting the authored endpoints.
            const int nextX = cellX + stepX;
            const int nextY = cellY + stepY;
            if (blocksIntermediateCell(nextX, cellY) || blocksIntermediateCell(cellX, nextY)) return false;
            cellX = nextX;
            cellY = nextY;
            boundaryX += deltaX;
            boundaryY += deltaY;
        }
        if (blocksIntermediateCell(cellX, cellY)) return false;
    }
    return true;
}

LightSample Lighting::sampleAt(const Dungeon& dungeon,
                               double pointX,
                               double pointY,
                               int viewCellX,
                               int viewCellY) {
    LightSample sample;
    for (const auto& placement : dungeon.lights()) {
        const auto* definition = findLight(placement.lightId);
        if (definition == nullptr || definition->intensity <= 0.0f ||
            (definition->kind != LightKind::Directional && definition->radius <= 0.0f)) continue;
        const EvaluatedLight evaluated{placement.x, placement.y, definition, definition->intensity,
                                       definition->castsShadows && definition->kind != LightKind::Directional};
        const bool visible = !evaluated.shadowed || reaches(dungeon, placement.x, placement.y, viewCellX, viewCellY);
        accumulate(evaluated, pointX, pointY, visible, sample);
    }
    clampSample(sample);
    return sample;
}

} // namespace sv
