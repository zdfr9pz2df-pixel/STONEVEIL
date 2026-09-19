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

struct MaterialDefinition {
    std::string id;
    std::string category;
    std::string name;
    SurfaceKind surface{SurfaceKind::Wall};
    MaterialSwatch swatch;
    std::string texturePath;
};

const std::vector<MaterialDefinition>& materialCatalog();
const MaterialDefinition* findMaterial(const std::string& id);
std::vector<const MaterialDefinition*> materialsFor(SurfaceKind surface);

} // namespace sv
