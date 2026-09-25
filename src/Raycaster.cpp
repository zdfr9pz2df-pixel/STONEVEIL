#include "Raycaster.hpp"

#include "Dungeon.hpp"
#include "Lighting.hpp"
#include "Material.hpp"
#include "Player.hpp"
#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef STONEVEIL_SOURCE_DIR
#define STONEVEIL_SOURCE_DIR ""
#endif

namespace sv {
namespace {
constexpr int ViewX = 24;
constexpr int ViewY = 36;
constexpr int ViewW = 930;
constexpr int ViewH = 560;
constexpr double FovScale = 0.66;
constexpr int SurfaceBlock = 3;
constexpr float WallAmbient = 0.38f;
constexpr float FloorAmbient = 0.48f;
constexpr float CeilingAmbient = 0.42f;
constexpr float SkyAmbient = 0.82f;
constexpr float TorchGain = 1.15f;

struct TextureAsset {
    Texture2D texture{};
    int width{};
    int height{};
    std::vector<Color> pixels;
};

Color materialColor(const std::string& materialId, Color fallback) {
    const auto* material = findMaterial(materialId);
    if (material == nullptr) return fallback;
    return {material->swatch.r, material->swatch.g, material->swatch.b, 255};
}

std::vector<std::filesystem::path> candidateAssetPaths(const std::string& relativePath, const std::string& root) {
    std::vector<std::filesystem::path> paths;
    const auto addCandidate = [&paths, &relativePath](const std::filesystem::path& base) {
        if (!base.empty()) paths.push_back(base / relativePath);
    };

    if (!root.empty()) { addCandidate(root); return paths; }
    addCandidate(GetApplicationDirectory());
    std::error_code error;
    const auto workingDirectory = std::filesystem::current_path(error);
    if (!error) addCandidate(workingDirectory);
    addCandidate(STONEVEIL_SOURCE_DIR);
    return paths;
}

TextureAsset* textureForPath(const std::string& texturePath, const std::string& root) {
    if (texturePath.empty()) return nullptr;
    static std::unordered_map<std::string, TextureAsset> textures;
    const std::string cacheKey = root + "|" + texturePath;
    const auto found = textures.find(cacheKey);
    if (found != textures.end()) {
        return found->second.texture.id == 0 ? nullptr : &found->second;
    }

    TextureAsset asset{};
    std::vector<std::filesystem::path> searched;
    for (const auto& path : candidateAssetPaths(texturePath, root)) {
        searched.push_back(path);
        std::error_code error;
        if (!std::filesystem::exists(path, error)) continue;

        const std::string nativePath = path.string();
        Image image = LoadImage(nativePath.c_str());
        if (image.data == nullptr) continue;

        asset.width = image.width;
        asset.height = image.height;
        if (Color* colors = LoadImageColors(image)) {
            asset.pixels.assign(colors, colors + static_cast<std::size_t>(image.width) *
                                static_cast<std::size_t>(image.height));
            UnloadImageColors(colors);
        }
        asset.texture = LoadTextureFromImage(image);
        if (asset.texture.id != 0) {
            SetTextureFilter(asset.texture, TEXTURE_FILTER_POINT);
            TraceLog(LOG_INFO, "STONEVEIL: loaded texture '%s' from '%s'", texturePath.c_str(), nativePath.c_str());
        }
        UnloadImage(image);
        break;
    }

    if (asset.texture.id == 0 || asset.pixels.empty()) {
        TraceLog(LOG_WARNING, "STONEVEIL: missing texture '%s'; falling back to material swatch", texturePath.c_str());
        for (const auto& path : searched) {
            const std::string nativePath = path.string();
            TraceLog(LOG_WARNING, "STONEVEIL: searched '%s'", nativePath.c_str());
        }
    }

    auto inserted = textures.emplace(cacheKey, std::move(asset));
    return inserted.first->second.texture.id == 0 || inserted.first->second.pixels.empty() ? nullptr : &inserted.first->second;
}

TextureAsset* materialTexture(const MaterialDefinition& material, const std::string& root) {
    return textureForPath(material.texturePath, root);
}

TextureAsset* doorTexture(Tile tile, const std::string& root) {
    if (tile == Tile::DoorClosed) {
        return textureForPath("content/textures/doors/iron-banded-wooden-door.png", root);
    }
    if (tile == Tile::SecretDoorClosed) {
        return textureForPath("content/textures/doors/secret-stone-door.png", root);
    }
    return nullptr;
}

Color shadeMaterial(Color base,
                    const MaterialDefinition* material,
                    float ambient,
                    const LightSample& light,
                    float shade = 1.0f,
                    float fresnel = 0.0f) {
    const MaterialSurfaceProperties properties = material == nullptr
        ? MaterialSurfaceProperties{}
        : material->properties;
    const float wetness = std::clamp(properties.wetness, 0.0f, 1.0f);
    const float roughness = std::clamp(properties.roughness * (1.0f - 0.72f * wetness), 0.04f, 1.0f);
    const float diffuseScale = 1.0f - 0.16f * wetness;
    const float lightPeak = std::max({light.red, light.green, light.blue});
    const float dielectric = std::clamp(properties.reflectionStrength, 0.0f, 1.0f);
    const float specular = lightPeak * dielectric * (1.0f - roughness) * (1.0f - roughness) *
                           (0.18f + 0.82f * std::clamp(fresnel, 0.0f, 1.0f));
    const float metallic = std::clamp(properties.metallic, 0.0f, 1.0f);

    const auto litChannel = [ambient, shade, diffuseScale, specular, metallic, &properties]
                            (unsigned char channel, float amount) {
        const float baseChannel = static_cast<float>(channel);
        const float diffuse = baseChannel * diffuseScale * shade * (ambient + TorchGain * amount);
        const float ember = 22.0f * amount;
        const float specularColor = (255.0f * (1.0f - metallic) + baseChannel * metallic) * specular;
        const float emission = baseChannel * std::max(0.0f, properties.emissive);
        const float lit = diffuse + ember + specularColor + emission;
        return static_cast<unsigned char>(std::clamp(lit, 0.0f, 255.0f));
    };
    return {litChannel(base.r, light.red), litChannel(base.g, light.green), litChannel(base.b, light.blue), base.a};
}

Color shadeColor(Color base, float ambient, const LightSample& light, float shade = 1.0f) {
    return shadeMaterial(base, nullptr, ambient, light, shade);
}

Color textureTint(const MaterialDefinition* material,
                  float ambient,
                  const LightSample& light,
                  float shade = 1.0f,
                  float fresnel = 0.0f) {
    return shadeMaterial(WHITE, material, ambient, light, shade, fresnel);
}

float distanceShade(double distance) {
    return std::clamp(1.02f - static_cast<float>(distance) * 0.024f, 0.78f, 1.0f);
}

float wallFaceShade(bool side, int stepX, int stepY, double distance) {
    const int normalX = side ? 0 : -stepX;
    const int normalY = side ? -stepY : 0;
    float shade = 0.82f;
    if (normalX < 0) shade += 0.08f;
    if (normalX > 0) shade -= 0.02f;
    if (normalY < 0) shade += 0.03f;
    if (normalY > 0) shade -= 0.10f;
    if (side) shade -= 0.04f;
    return std::clamp(shade * distanceShade(distance), 0.64f, 1.0f);
}

float openCellOcclusion(const Dungeon& dungeon, int cellX, int cellY) {
    if (!dungeon.inBounds(cellX, cellY)) return 0.84f;
    constexpr std::array<std::pair<int, int>, 4> cardinal = {
        std::pair<int, int>{1, 0},
        std::pair<int, int>{-1, 0},
        std::pair<int, int>{0, 1},
        std::pair<int, int>{0, -1},
    };
    constexpr std::array<std::pair<int, int>, 4> diagonal = {
        std::pair<int, int>{1, 1},
        std::pair<int, int>{1, -1},
        std::pair<int, int>{-1, 1},
        std::pair<int, int>{-1, -1},
    };

    int cardinalWalls = 0;
    int diagonalWalls = 0;
    for (const auto& [dx, dy] : cardinal) {
        if (dungeon.blocksSight(cellX + dx, cellY + dy)) ++cardinalWalls;
    }
    for (const auto& [dx, dy] : diagonal) {
        if (dungeon.blocksSight(cellX + dx, cellY + dy)) ++diagonalWalls;
    }
    return std::clamp(1.0f - static_cast<float>(cardinalWalls) * 0.055f -
                          static_cast<float>(diagonalWalls) * 0.025f,
                      0.80f, 1.0f);
}

Color wallColor(Tile tile, const std::string& materialId, bool side) {
    Color color = tile == Tile::DoorClosed
        ? Color{126, 87, 46, 255}
        : materialColor(materialId, Color{92, 103, 112, 255});
    if (side) {
        color.r = static_cast<unsigned char>(color.r * 0.72f);
        color.g = static_cast<unsigned char>(color.g * 0.72f);
        color.b = static_cast<unsigned char>(color.b * 0.72f);
    }
    return color;
}

double wrapUnit(double value) {
    value -= std::floor(value);
    return value < 0.0 ? value + 1.0 : value;
}

Color sampleTexture(const TextureAsset& asset, double u, double v) {
    const int texX = std::clamp(static_cast<int>(wrapUnit(u) * static_cast<double>(asset.width)), 0, asset.width - 1);
    const int texY = std::clamp(static_cast<int>(wrapUnit(v) * static_cast<double>(asset.height)), 0, asset.height - 1);
    return asset.pixels[static_cast<std::size_t>(texY) * static_cast<std::size_t>(asset.width) + static_cast<std::size_t>(texX)];
}

Color surfaceColorAt(const Dungeon& dungeon, int cellX, int cellY, SurfaceKind surface, double worldX, double worldY,
                     Color fallback, const std::string& root) {
    if (!dungeon.inBounds(cellX, cellY)) return fallback;
    if (surface == SurfaceKind::Ceiling && dungeon.ceilingModeAt(cellX, cellY) == CeilingMode::Sky) {
        return Color{72, 121, 159, 255};
    }
    const std::string materialId = dungeon.materialAt(cellX, cellY, surface);
    const auto* material = findMaterial(materialId);
    if (material == nullptr) return fallback;
    if (const auto* texture = materialTexture(*material, root)) {
        return sampleTexture(*texture, worldX, worldY);
    }
    return {material->swatch.r, material->swatch.g, material->swatch.b, 255};
}

bool isSkyAt(const Dungeon& dungeon, int cellX, int cellY) {
    return dungeon.inBounds(cellX, cellY) && dungeon.ceilingModeAt(cellX, cellY) == CeilingMode::Sky;
}

void drawTexturedFloorAndCeiling(const Dungeon& dungeon, double posX, double posY, double dirX, double dirY,
                                 double planeX, double planeY, const std::string& root,
                                 const LightingFrame& lighting) {
    const double leftRayDirX = dirX - planeX;
    const double leftRayDirY = dirY - planeY;
    const double rightRayDirX = dirX + planeX;
    const double rightRayDirY = dirY + planeY;

    for (int y = ViewH / 2; y < ViewH; y += SurfaceBlock) {
        const int blockH = std::min(SurfaceBlock, ViewH - y);
        const int distanceFromHorizon = y - ViewH / 2;
        if (distanceFromHorizon <= 0) continue;

        const double rowDistance = (0.5 * static_cast<double>(ViewH)) / static_cast<double>(distanceFromHorizon);
        const double stepX = rowDistance * (rightRayDirX - leftRayDirX) / static_cast<double>(ViewW);
        const double stepY = rowDistance * (rightRayDirY - leftRayDirY) / static_cast<double>(ViewW);

        for (int x = 0; x < ViewW; x += SurfaceBlock) {
            const int blockW = std::min(SurfaceBlock, ViewW - x);
            const double sampleX = static_cast<double>(x) + static_cast<double>(blockW) * 0.5;
            const double worldX = posX + rowDistance * leftRayDirX + stepX * sampleX;
            const double worldY = posY + rowDistance * leftRayDirY + stepY * sampleX;
            const int cellX = static_cast<int>(std::floor(worldX));
            const int cellY = static_cast<int>(std::floor(worldY));
            if (!dungeon.inBounds(cellX, cellY)) continue;
            const float shade = openCellOcclusion(dungeon, cellX, cellY) * distanceShade(rowDistance);
            const LightSample surfaceLight = lighting.sampleAt(worldX, worldY, cellX, cellY);
            const float viewNormal = static_cast<float>(0.5 / std::sqrt(rowDistance * rowDistance + 0.25));
            const float grazing = 1.0f - std::clamp(viewNormal, 0.0f, 1.0f);

            const Color floorColor = surfaceColorAt(dungeon, cellX, cellY, SurfaceKind::Floor, worldX, worldY,
                                                     Color{43, 37, 31, 255}, root);
            const auto* floorMaterial = findMaterial(dungeon.materialAt(cellX, cellY, SurfaceKind::Floor));
            DrawRectangle(ViewX + x, ViewY + y, blockW, blockH,
                          shadeMaterial(floorColor, floorMaterial, FloorAmbient, surfaceLight, shade, grazing));

            const int ceilingY = ViewH - y - blockH;
            const bool sky = isSkyAt(dungeon, cellX, cellY);
            const Color ceilingColor = surfaceColorAt(dungeon, cellX, cellY, SurfaceKind::Ceiling, worldX, worldY,
                                                      Color{29, 31, 37, 255}, root);
            const auto* ceilingMaterial = sky ? nullptr
                : findMaterial(dungeon.materialAt(cellX, cellY, SurfaceKind::Ceiling));
            DrawRectangle(ViewX + x, ViewY + ceilingY, blockW, blockH,
                          shadeMaterial(ceilingColor, ceilingMaterial, sky ? SkyAmbient : CeilingAmbient,
                                        sky ? LightSample{} : surfaceLight,
                                        sky ? 1.0f : shade * 0.92f, grazing * 0.4f));
        }
    }
}
}

void Raycaster::draw(const Dungeon& dungeon, const PlayerState& player) const {
    const auto& root = contentRoot_;
    const double posX = player.eyeX();
    const double posY = player.eyeY();
    const LightingFrame lighting = Lighting::buildFrame(dungeon, GetTime(), lightingSettings_);
    const LightSample eyeLight = lighting.sampleAt(posX, posY, player.x(), player.y());
    const bool playerSky = dungeon.ceilingModeAt(player.x(), player.y()) == CeilingMode::Sky;
    const Color ceilingColor = playerSky
        ? Color{72, 121, 159, 255}
        : materialColor(dungeon.materialAt(player.x(), player.y(), SurfaceKind::Ceiling), Color{29, 31, 37, 255});
    const Color floorColor = materialColor(dungeon.materialAt(player.x(), player.y(), SurfaceKind::Floor),
                                           Color{43, 37, 31, 255});
    const auto* playerCeilingMaterial = playerSky ? nullptr
        : findMaterial(dungeon.materialAt(player.x(), player.y(), SurfaceKind::Ceiling));
    const auto* playerFloorMaterial = findMaterial(dungeon.materialAt(player.x(), player.y(), SurfaceKind::Floor));
    DrawRectangle(ViewX, ViewY, ViewW, ViewH / 2,
                  shadeMaterial(ceilingColor, playerCeilingMaterial, playerSky ? SkyAmbient : CeilingAmbient,
                                playerSky ? LightSample{} : eyeLight,
                                openCellOcclusion(dungeon, player.x(), player.y())));
    DrawRectangle(ViewX, ViewY + ViewH / 2, ViewW, ViewH / 2,
                  shadeMaterial(floorColor, playerFloorMaterial, FloorAmbient, eyeLight,
                                openCellOcclusion(dungeon, player.x(), player.y()), 0.15f));

    const double dirX = static_cast<double>(PlayerState::directionX(player.direction()));
    const double dirY = static_cast<double>(PlayerState::directionY(player.direction()));
    const double planeX = -dirY * FovScale;
    const double planeY = dirX * FovScale;

    drawTexturedFloorAndCeiling(dungeon, posX, posY, dirX, dirY, planeX, planeY, root, lighting);

    for (int x = 0; x < ViewW; ++x) {
        const double cameraX = 2.0 * x / static_cast<double>(ViewW) - 1.0;
        const double rayDirX = dirX + planeX * cameraX;
        const double rayDirY = dirY + planeY * cameraX;
        int mapX = static_cast<int>(std::floor(posX));
        int mapY = static_cast<int>(std::floor(posY));
        const double deltaX = rayDirX == 0.0 ? 1e30 : std::abs(1.0 / rayDirX);
        const double deltaY = rayDirY == 0.0 ? 1e30 : std::abs(1.0 / rayDirY);
        double sideDistX{};
        double sideDistY{};
        int stepX{};
        int stepY{};

        if (rayDirX < 0.0) {
            stepX = -1;
            sideDistX = (posX - mapX) * deltaX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0 - posX) * deltaX;
        }
        if (rayDirY < 0.0) {
            stepY = -1;
            sideDistY = (posY - mapY) * deltaY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0 - posY) * deltaY;
        }

        bool side = false;
        Tile hitTile = Tile::Wall;
        for (int guard = 0; guard < 64; ++guard) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaX;
                mapX += stepX;
                side = false;
            } else {
                sideDistY += deltaY;
                mapY += stepY;
                side = true;
            }
            hitTile = dungeon.tile(mapX, mapY);
            if (hitTile == Tile::Wall || hitTile == Tile::DoorClosed || hitTile == Tile::SecretDoorClosed) break;
        }

        const double distance = side ? sideDistY - deltaY : sideDistX - deltaX;
        const int lineHeight = static_cast<int>(ViewH / std::max(0.05, distance));
        const int start = ViewY + std::max(0, (ViewH - lineHeight) / 2);
        const int end = ViewY + std::min(ViewH - 1, (ViewH + lineHeight) / 2);

        // Light the exact point the ray struck, traced from the open cell the
        // wall face is seen from so a torch never bleeds through solid stone.
        const double hitX = side ? posX + distance * rayDirX
                                 : static_cast<double>(mapX) + (stepX < 0 ? 1.0 : 0.0);
        const double hitY = side ? static_cast<double>(mapY) + (stepY < 0 ? 1.0 : 0.0)
                                 : posY + distance * rayDirY;
        const int viewCellX = side ? mapX : mapX - stepX;
        const int viewCellY = side ? mapY - stepY : mapY;
        const LightSample wallLight = lighting.sampleAt(hitX, hitY, viewCellX, viewCellY);
        const float faceShade = wallFaceShade(side, stepX, stepY, distance);

        const std::string materialId = dungeon.materialAt(mapX, mapY, SurfaceKind::Wall);
        const auto* material = findMaterial(materialId);
        TextureAsset* texture = hitTile == Tile::DoorClosed || hitTile == Tile::SecretDoorClosed
            ? doorTexture(hitTile, root)
            : (material == nullptr ? nullptr : materialTexture(*material, root));
        if (texture != nullptr) {
            double wallX = side ? posX + distance * rayDirX : posY + distance * rayDirY;
            wallX -= std::floor(wallX);
            int texX = static_cast<int>(wallX * static_cast<double>(texture->texture.width));
            texX = std::clamp(texX, 0, texture->texture.width - 1);
            if ((!side && rayDirX > 0.0) || (side && rayDirY < 0.0)) {
                texX = texture->texture.width - texX - 1;
            }

            const Rectangle source{static_cast<float>(texX), 0.0f, 1.0f, static_cast<float>(texture->texture.height)};
            const Rectangle dest{static_cast<float>(ViewX + x), static_cast<float>(start), 1.0f,
                                 static_cast<float>(std::max(1, end - start + 1))};
            DrawTexturePro(texture->texture, source, dest, Vector2{0.0f, 0.0f}, 0.0f,
                           textureTint(material, WallAmbient, wallLight, faceShade, 0.12f));
        } else {
            DrawLine(ViewX + x, start, ViewX + x, end,
                      shadeMaterial(wallColor(hitTile, materialId, side), material,
                                    WallAmbient, wallLight, faceShade, 0.12f));
        }
    }

    DrawRectangleLines(ViewX, ViewY, ViewW, ViewH, Color{139, 123, 89, 255});
}

} // namespace sv
