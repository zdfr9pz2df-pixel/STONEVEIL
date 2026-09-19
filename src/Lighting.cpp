#include "Lighting.hpp"

#include "Dungeon.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sv {
namespace {

double cellCenter(int coordinate) {
    return static_cast<double>(coordinate) + 0.5;
}

} // namespace

const std::vector<LightDefinition>& lightCatalog() {
    static const std::vector<LightDefinition> lights = {
        {"light.torch.iron-sconce", "Torch", "Iron Sconce", {255, 176, 92}, 1.00f, 5.5f},
        {"light.torch.pitch-brazier", "Torch", "Pitch Brazier", {255, 148, 68}, 1.25f, 7.5f},
        {"light.torch.tallow-candle", "Torch", "Tallow Candle", {255, 208, 150}, 0.55f, 3.0f},
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

    // Torches should feel hot near the source, then fade through a broad
    // penumbra instead of dropping as a hard circular spotlight. Keeping a
    // small full-strength core also makes sconces read clearly on adjacent
    // wall faces while the smoothstep tail preserves a dark outer edge.
    constexpr double HotCore = 0.08;
    if (normalized <= HotCore) return 1.0f;
    const double t = (normalized - HotCore) / (1.0 - HotCore);
    const double smooth = 1.0 - (t * t * (3.0 - 2.0 * t));
    const double remaining = 1.0 - normalized;
    return static_cast<float>(remaining * (0.35 + 0.65 * smooth));
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
        if (definition == nullptr || definition->intensity <= 0.0f || definition->radius <= 0.0f) continue;

        const double offsetX = cellCenter(placement.x) - pointX;
        const double offsetY = cellCenter(placement.y) - pointY;
        const double distanceSquared = offsetX * offsetX + offsetY * offsetY;
        const double radiusSquared = static_cast<double>(definition->radius) * definition->radius;
        if (distanceSquared >= radiusSquared) continue;
        const double distance = std::sqrt(distanceSquared);
        const float attenuation = falloff(distance, definition->radius);
        if (attenuation <= 0.0f) continue;
        if (!reaches(dungeon, placement.x, placement.y, viewCellX, viewCellY)) continue;

        const float strength = attenuation * definition->intensity;
        sample.red += strength * (static_cast<float>(definition->tint.r) / 255.0f);
        sample.green += strength * (static_cast<float>(definition->tint.g) / 255.0f);
        sample.blue += strength * (static_cast<float>(definition->tint.b) / 255.0f);
    }

    sample.red = std::min(sample.red, 1.0f);
    sample.green = std::min(sample.green, 1.0f);
    sample.blue = std::min(sample.blue, 1.0f);
    return sample;
}

} // namespace sv
