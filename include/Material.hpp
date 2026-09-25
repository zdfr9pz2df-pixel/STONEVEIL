#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sv {

enum class SurfaceKind {
    Wall,
    Floor,
    Ceiling,
};

enum class CeilingMode {
    Material,
    Sky,
};

struct MaterialSwatch {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
};

// A compact, renderer-independent surface description. These values are kept
// beside the stable material catalog entry so the CPU DDA renderer and later
// GPU post passes consume the same authored response instead of inventing
// effect-specific rules from material names.
struct MaterialSurfaceProperties {
    float roughness{0.85f};
    float metallic{0.0f};
    float emissive{0.0f};
    float opacity{1.0f};
    float transmission{0.0f};
    float reflectionStrength{0.04f};
    float wetness{0.0f};
    float normalStrength{1.0f};
};

struct MaterialDefinition {
    std::string id;
    std::string category;
    std::string name;
    SurfaceKind surface{SurfaceKind::Wall};
    MaterialSwatch swatch;
    std::string texturePath;
    MaterialSurfaceProperties properties{};
};

const std::vector<MaterialDefinition>& materialCatalog();
const MaterialDefinition* findMaterial(const std::string& id);
std::vector<const MaterialDefinition*> materialsFor(SurfaceKind surface);

} // namespace sv
