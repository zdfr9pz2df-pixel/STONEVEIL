#include "Material.hpp"

#include <algorithm>

namespace sv {

const std::vector<MaterialDefinition>& materialCatalog() {
    static const std::vector<MaterialDefinition> materials = {
        {"material.wall.cave.mossy-rock", "Cave", "Mossy Rock", SurfaceKind::Wall, {77, 105, 75},
         "content/textures/walls/cave/mossy-rock.png"},
        {"material.wall.cave.mossy-rock-root-trace", "Cave", "Mossy Rock - Root Trace", SurfaceKind::Wall,
         {72, 99, 68}, "content/textures/walls/cave/mossy-rock-root-trace.png"},
        {"material.wall.cave.mossy-rock-vines", "Cave", "Mossy Rock - Vines", SurfaceKind::Wall, {69, 101, 65},
         "content/textures/walls/cave/mossy-rock-vines.png"},
        {"material.wall.cave.mossy-rock-soft-lichen", "Cave", "Mossy Rock - Soft Lichen", SurfaceKind::Wall,
         {84, 106, 80}, "content/textures/walls/cave/mossy-rock-soft-lichen.png"},
        {"material.wall.cave.mossy-rock-lichen", "Cave", "Mossy Rock - Lichen", SurfaceKind::Wall, {91, 109, 84},
         "content/textures/walls/cave/mossy-rock-lichen.png"},
        {"material.wall.cave.mossy-rock-damp-moss", "Cave", "Mossy Rock - Damp Moss", SurfaceKind::Wall,
         {61, 96, 61}, "content/textures/walls/cave/mossy-rock-damp-moss.png",
         {0.52f, 0.0f, 0.0f, 1.0f, 0.0f, 0.16f, 0.55f, 0.85f}},
        {"material.wall.cave.mossy-rock-wet", "Cave", "Mossy Rock - Wet", SurfaceKind::Wall, {55, 89, 59},
         "content/textures/walls/cave/mossy-rock-wet.png",
         {0.32f, 0.0f, 0.0f, 1.0f, 0.0f, 0.28f, 0.88f, 0.75f}},
        {"material.wall.cave.wet-limestone", "Cave", "Wet Limestone", SurfaceKind::Wall, {103, 119, 124},
         "content/textures/walls/cave/wet-limestone.png",
         {0.28f, 0.0f, 0.0f, 1.0f, 0.0f, 0.32f, 0.92f, 0.72f}},
        {"material.wall.cave.cracked-basalt", "Cave", "Cracked Basalt", SurfaceKind::Wall, {61, 64, 70},
         "content/textures/walls/cave/cracked-basalt.png"},
        {"material.wall.portal.stone-arch-doorway", "Portal", "Stone Arch Doorway", SurfaceKind::Wall, {102, 112, 101},
         "content/textures/walls/portal/stone-arch-doorway.png"},
        {"material.wall.portal.iron-gatehouse-surround", "Portal", "Iron Gatehouse Surround", SurfaceKind::Wall,
         {81, 76, 69}, "content/textures/walls/portal/iron-gatehouse-surround.png"},
        {"material.wall.portal.rusted-iron-gate", "Portal", "Rusted Iron Gate", SurfaceKind::Wall, {72, 61, 52},
         "content/textures/doors/rusted-iron-gate.png",
         {0.58f, 0.72f, 0.0f, 1.0f, 0.0f, 0.42f, 0.08f, 0.92f}},
        {"material.wall.fortress.dressed-stone", "Fortress", "Dressed Stone", SurfaceKind::Wall, {118, 112, 103},
         "content/textures/walls/fortress/dressed-stone.png"},
        {"material.wall.timber.hewn-planks", "Timber", "Hewn Planks", SurfaceKind::Wall, {119, 83, 57},
         "content/textures/walls/timber/hewn-planks.png"},
        {"material.floor.cave.uneven-stone", "Cave", "Uneven Stone", SurfaceKind::Floor, {77, 72, 65},
         "content/textures/floors/cave/uneven-stone.png"},
        {"material.floor.cave.mud", "Cave", "Mud", SurfaceKind::Floor, {92, 70, 48},
         "content/textures/floors/cave/mud.png",
         {0.68f, 0.0f, 0.0f, 1.0f, 0.0f, 0.09f, 0.30f, 0.55f}},
        {"material.floor.cave.shallow-water", "Cave", "Shallow Water", SurfaceKind::Floor, {55, 102, 119},
         "content/textures/floors/cave/shallow-water.png",
         {0.08f, 0.0f, 0.0f, 0.78f, 0.72f, 0.68f, 1.0f, 0.65f}},
        {"material.floor.timber.planks", "Timber", "Floor Planks", SurfaceKind::Floor, {109, 75, 50},
         "content/textures/floors/timber/planks.png"},
        {"material.ceiling.cave.rocky", "Cave", "Rocky", SurfaceKind::Ceiling, {52, 55, 62},
         "content/textures/ceilings/cave/rocky.png"},
        {"material.ceiling.cave.timber-supports", "Cave", "Timber Supports", SurfaceKind::Ceiling, {84, 65, 51},
         "content/textures/ceilings/cave/timber-supports.png"},
        {"material.ceiling.fortress.vaulted-stone", "Fortress", "Vaulted Stone", SurfaceKind::Ceiling, {85, 84, 85},
         "content/textures/ceilings/fortress/vaulted-stone.png"},
        {"material.ceiling.timber.rafters", "Timber", "Rafters", SurfaceKind::Ceiling, {94, 65, 46},
         "content/textures/ceilings/timber/rafters.png"},
    };
    return materials;
}

const MaterialDefinition* findMaterial(const std::string& id) {
    const auto& materials = materialCatalog();
    const auto found = std::find_if(materials.begin(), materials.end(), [&id](const MaterialDefinition& material) {
        return material.id == id;
    });
    return found == materials.end() ? nullptr : &*found;
}

std::vector<const MaterialDefinition*> materialsFor(SurfaceKind surface) {
    std::vector<const MaterialDefinition*> matches;
    for (const auto& material : materialCatalog()) {
        if (material.surface == surface) matches.push_back(&material);
    }
    return matches;
}

} // namespace sv
