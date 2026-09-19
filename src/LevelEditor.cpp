#include "LevelEditor.hpp"

#include "LevelBlueprint.hpp"
#include "LevelIO.hpp"
#include "Lighting.hpp"
#include "Material.hpp"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#ifndef STONEVEIL_SOURCE_DIR
#define STONEVEIL_SOURCE_DIR ""
#endif

namespace sv {
namespace {
constexpr float MapAreaX = 32.0f;
constexpr float MapAreaY = 108.0f;
constexpr float MapAreaWidth = 512.0f;
constexpr float MapAreaHeight = 488.0f;
constexpr float PanelX = 576.0f;
constexpr float PanelY = 64.0f;
constexpr float PanelWidth = 672.0f;
constexpr float PanelHeight = 636.0f;
constexpr float OptionX = PanelX + 20.0f;
constexpr float OptionWidth = PanelWidth - 40.0f;

const Color Background{16, 19, 24, 255};
const Color Panel{27, 31, 38, 255};
const Color PanelRaised{39, 44, 53, 255};
const Color Border{75, 84, 98, 255};
const Color Text{224, 229, 235, 255};
const Color Muted{145, 154, 168, 255};
const Color Accent{197, 151, 66, 255};
const Color Valid{91, 181, 120, 255};
const Color Invalid{214, 91, 82, 255};

struct MaterialButton {
    const MaterialDefinition* material{};
    Rectangle bounds{};
    float categoryY{};
    bool startsCategory{false};
};

struct LightButton {
    const LightDefinition* light{};
    Rectangle bounds{};
    float categoryY{};
    bool startsCategory{false};
};

struct MusicTrackButton {
    std::string path;
    std::string label;
    Rectangle bounds{};
    bool none{false};
};

constexpr float EraseLightY = 130.0f;
constexpr float FirstLightY = 190.0f;
constexpr float NoMusicY = 130.0f;
constexpr float FirstMusicY = 190.0f;

Rectangle layerButton(int index) {
    return {MapAreaX + static_cast<float>(index) * 57.0f, 64.0f, 52.0f, 34.0f};
}

Rectangle undoButton() { return {660.0f, 20.0f, 62.0f, 28.0f}; }
Rectangle redoButton() { return {728.0f, 20.0f, 62.0f, 28.0f}; }
Rectangle newButton() { return {796.0f, 20.0f, 62.0f, 28.0f}; }
Rectangle saveAsButton() { return {864.0f, 20.0f, 82.0f, 28.0f}; }

Rectangle objectBrushButton(int index) {
    const int column = index % 2;
    const int row = index / 2;
    return {OptionX + static_cast<float>(column) * (OptionWidth * 0.5f + 3.0f),
            130.0f + static_cast<float>(row) * 35.0f, OptionWidth * 0.5f - 3.0f, 29.0f};
}

Rectangle storyModeButton(bool erase) {
    return {OptionX + (erase ? OptionWidth * 0.5f + 4.0f : 0.0f), 130.0f,
            OptionWidth * 0.5f - 4.0f, 32.0f};
}

Rectangle storyFieldBounds(int index) {
    return {OptionX, 190.0f + static_cast<float>(index) * 48.0f, OptionWidth, 34.0f};
}

Rectangle triggerEventButton(int index) {
    const int column = index % 2;
    const int row = index / 2;
    return {OptionX + static_cast<float>(column) * (OptionWidth * 0.5f + 3.0f),
            130.0f + static_cast<float>(row) * 38.0f, OptionWidth * 0.5f - 3.0f, 32.0f};
}

Rectangle triggerEraseButton() { return {OptionX, 250.0f, OptionWidth, 32.0f}; }
Rectangle triggerMessageBounds() { return {OptionX, 314.0f, OptionWidth, 46.0f}; }

Rectangle eraseLightButton() {
    return {OptionX, EraseLightY, OptionWidth, 34.0f};
}

Rectangle noMusicButton() {
    return {OptionX, NoMusicY, OptionWidth, 34.0f};
}

float mapCellSize(const LevelDefinition& level) {
    const float horizontal = MapAreaWidth / static_cast<float>(std::max(1, level.width));
    const float vertical = MapAreaHeight / static_cast<float>(std::max(1, level.height));
    return std::min(32.0f, std::min(horizontal, vertical));
}

Rectangle mapBounds(const LevelDefinition& level) {
    const float cellSize = mapCellSize(level);
    const float width = cellSize * static_cast<float>(level.width);
    const float height = cellSize * static_cast<float>(level.height);
    return {MapAreaX + (MapAreaWidth - width) * 0.5f,
            MapAreaY + (MapAreaHeight - height) * 0.5f,
            width,
            height};
}

Rectangle dimensionButton(int index) {
    return {120.0f + static_cast<float>(index) * 45.0f, 660.0f, 40.0f, 28.0f};
}

Rectangle randomizeButton() {
    return {304.0f, 660.0f, 82.0f, 28.0f};
}

Rectangle playButton() {
    return {392.0f, 660.0f, 62.0f, 28.0f};
}

Rectangle saveButton() {
    return {462.0f, 660.0f, 62.0f, 28.0f};
}

Rectangle editorMenuButton() {
    return {1152.0f, 78.0f, 76.0f, 28.0f};
}

Rectangle importButton() {
    return {1068.0f, 78.0f, 76.0f, 28.0f};
}

Rectangle importPanelBounds() {
    return {260.0f, 118.0f, 760.0f, 484.0f};
}

Rectangle importPasteButton() {
    return {552.0f, 512.0f, 174.0f, 36.0f};
}

Rectangle importCancelButton() {
    return {744.0f, 512.0f, 112.0f, 36.0f};
}

void drawButton(Rectangle bounds, const char* label, bool selected, int fontSize = 18) {
    DrawRectangleRec(bounds, selected ? Accent : PanelRaised);
    DrawRectangleLinesEx(bounds, 1.0f, selected ? Color{239, 199, 112, 255} : Border);
    const int width = MeasureText(label, fontSize);
    DrawText(label,
             static_cast<int>(bounds.x + (bounds.width - static_cast<float>(width)) * 0.5f),
             static_cast<int>(bounds.y + (bounds.height - static_cast<float>(fontSize)) * 0.5f),
             fontSize,
             selected ? Background : Text);
}

std::string shortened(std::string value, std::size_t maximum);

void drawTextField(Rectangle bounds, const char* label, const std::string& value, bool active) {
    DrawText(label, static_cast<int>(bounds.x), static_cast<int>(bounds.y) - 16, 12, Muted);
    DrawRectangleRec(bounds, Color{18, 21, 26, 255});
    DrawRectangleLinesEx(bounds, active ? 2.0f : 1.0f, active ? Accent : Border);
    DrawText(shortened(value.empty() ? std::string{"(empty)"} : value, 74).c_str(),
             static_cast<int>(bounds.x) + 9, static_cast<int>(bounds.y) + 11, 14,
             value.empty() ? Muted : Text);
}

std::string shortened(std::string value, std::size_t maximum) {
    if (value.size() <= maximum) return value;
    if (maximum <= 3) return value.substr(0, maximum);
    return value.substr(0, maximum - 3) + "...";
}

std::vector<MaterialButton> materialButtons(SurfaceKind surface, float firstY) {
    std::vector<MaterialButton> buttons;
    std::string previousCategory;
    float y = firstY;
    for (const auto* material : materialsFor(surface)) {
        const bool startsCategory = material->category != previousCategory;
        float categoryY = 0.0f;
        if (startsCategory) {
            if (!previousCategory.empty()) y += 7.0f;
            categoryY = y;
            y += 24.0f;
            previousCategory = material->category;
        }
        buttons.push_back({material, {OptionX, y, OptionWidth, 32.0f}, categoryY, startsCategory});
        y += 38.0f;
    }
    return buttons;
}

std::vector<LightButton> lightButtons(float firstY) {
    std::vector<LightButton> buttons;
    std::string previousCategory;
    float y = firstY;
    for (const auto& light : lightCatalog()) {
        const bool startsCategory = light.category != previousCategory;
        float categoryY = 0.0f;
        if (startsCategory) {
            if (!previousCategory.empty()) y += 7.0f;
            categoryY = y;
            y += 24.0f;
            previousCategory = light.category;
        }
        buttons.push_back({&light, {OptionX, y, OptionWidth, 32.0f}, categoryY, startsCategory});
        y += 38.0f;
    }
    return buttons;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool hasMusicExtension(const std::filesystem::path& path) {
    const auto extension = lowercase(path.extension().string());
    return extension == ".wav" || extension == ".ogg" || extension == ".mp3" || extension == ".flac";
}

std::vector<std::string> discoverMusicTracks() {
    std::vector<std::filesystem::path> bases;
    bases.emplace_back(GetApplicationDirectory());
    std::error_code error;
    const auto workingDirectory = std::filesystem::current_path(error);
    if (!error) bases.emplace_back(workingDirectory);
    if (std::string{STONEVEIL_SOURCE_DIR}.size() > 0) bases.emplace_back(STONEVEIL_SOURCE_DIR);

    std::set<std::string> discovered;
    for (const auto& base : bases) {
        const auto directory = base / "content" / "audio" / "music";
        if (!std::filesystem::exists(directory, error)) {
            error.clear();
            continue;
        }
        for (std::filesystem::recursive_directory_iterator it(directory, error), end; it != end && !error; it.increment(error)) {
            if (!it->is_regular_file(error) || !hasMusicExtension(it->path())) {
                error.clear();
                continue;
            }
            const auto relative = std::filesystem::relative(it->path(), base, error);
            if (!error) discovered.insert(relative.generic_string());
            error.clear();
        }
        error.clear();
    }

    return {discovered.begin(), discovered.end()};
}

std::vector<MusicTrackButton> musicTrackButtons(float firstY) {
    std::vector<MusicTrackButton> buttons;
    float y = firstY;
    for (const auto& path : discoverMusicTracks()) {
        const std::filesystem::path filePath{path};
        buttons.push_back({path, filePath.filename().string(), {OptionX, y, OptionWidth, 32.0f}, false});
        y += 48.0f;
    }
    return buttons;
}

Color lightColor(const std::string& id) {
    const auto* light = findLight(id);
    if (light == nullptr) return {150, 146, 138, 255};
    return {light->tint.r, light->tint.g, light->tint.b, 255};
}

Color lightColorAlpha(const std::string& id, unsigned char alpha) {
    Color color = lightColor(id);
    color.a = alpha;
    return color;
}

Color withAlpha(Color color, unsigned char alpha) {
    color.a = alpha;
    return color;
}

float lightIntensity(const LightSample& sample) {
    return std::max({sample.red, sample.green, sample.blue});
}

double lightDistance(int fromX, int fromY, int toX, int toY) {
    const double dx = static_cast<double>(fromX - toX);
    const double dy = static_cast<double>(fromY - toY);
    return std::sqrt(dx * dx + dy * dy);
}

bool cellWithinLightRadius(int lightX, int lightY, int cellX, int cellY, const LightDefinition& definition) {
    return lightDistance(lightX, lightY, cellX, cellY) < static_cast<double>(definition.radius);
}

Color materialColor(const std::string& id) {
    const auto* material = findMaterial(id);
    if (material == nullptr) return {92, 94, 101, 255};
    return {material->swatch.r, material->swatch.g, material->swatch.b, 255};
}

bool isSolid(char marker) {
    return marker == '#';
}

bool isDoorMarker(char marker) {
    return marker == 'D' || marker == 'S';
}

std::string stableId(const LevelDefinition& level, const std::string& prefix, int x, int y) {
    const std::string base = prefix + "." + std::to_string(x) + "-" + std::to_string(y);
    const auto used = [&level](const std::string& id) {
        for (const auto& pickup : level.pickups) if (pickup.id == id) return true;
        for (const auto& enemy : level.enemies) if (enemy.id == id) return true;
        for (const auto& door : level.doors) if (door.id == id) return true;
        for (const auto& object : level.objects) if (object.id == id) return true;
        for (const auto& room : level.rooms) if (room.id == id) return true;
        for (const auto& trigger : level.triggers) if (trigger.id == id) return true;
        return false;
    };
    if (!used(base)) return base;
    for (int suffix = 2; suffix < 10000; ++suffix) {
        const std::string candidate = base + "-" + std::to_string(suffix);
        if (!used(candidate)) return candidate;
    }
    return base + ".overflow";
}

const char* objectKindLabel(WorldObjectKind kind) {
    switch (kind) {
        case WorldObjectKind::Prop: return "Prop";
        case WorldObjectKind::Shrine: return "Shrine";
        case WorldObjectKind::Note: return "Note";
        case WorldObjectKind::Corpse: return "Corpse";
        case WorldObjectKind::Npc: return "NPC";
    }
    return "Object";
}

const char* objectMapLabel(WorldObjectKind kind) {
    switch (kind) {
        case WorldObjectKind::Prop: return "P";
        case WorldObjectKind::Shrine: return "R";
        case WorldObjectKind::Note: return "N";
        case WorldObjectKind::Corpse: return "C";
        case WorldObjectKind::Npc: return "@";
    }
    return "?";
}

char markerForBrush(int brush) {
    constexpr std::array<char, 5> markers = {'.', '#', 'D', 'S', 'E'};
    return markers[static_cast<std::size_t>(brush)];
}

std::vector<const MaterialDefinition*> filteredMaterials(SurfaceKind surface,
                                                         const std::vector<std::string>& categories) {
    std::vector<const MaterialDefinition*> matches;
    for (const auto* material : materialsFor(surface)) {
        if (categories.empty() ||
            std::find(categories.begin(), categories.end(), material->category) != categories.end()) {
            matches.push_back(material);
        }
    }
    return matches;
}

const MaterialDefinition* chooseMaterial(const std::vector<const MaterialDefinition*>& materials,
                                         std::mt19937& randomizer) {
    if (materials.empty()) return nullptr;
    std::uniform_int_distribution<std::size_t> distribution(0, materials.size() - 1);
    return materials[distribution(randomizer)];
}

void drawLightDebugCell(Rectangle cell, Color color, float intensity) {
    const unsigned char alpha = static_cast<unsigned char>(
        std::clamp(26.0f + intensity * 105.0f, 0.0f, 138.0f));
    DrawRectangleRec({cell.x + 1.0f, cell.y + 1.0f, cell.width - 2.0f, cell.height - 2.0f},
                     withAlpha(color, alpha));
}

void drawBlockedDebugCell(Rectangle cell) {
    DrawRectangleRec({cell.x + 2.0f, cell.y + 2.0f, cell.width - 4.0f, cell.height - 4.0f},
                     Color{154, 51, 43, 36});
    if (cell.width >= 14.0f && cell.height >= 14.0f) {
        DrawLine(static_cast<int>(cell.x + 3.0f), static_cast<int>(cell.y + 3.0f),
                 static_cast<int>(cell.x + cell.width - 3.0f),
                 static_cast<int>(cell.y + cell.height - 3.0f), Color{214, 91, 82, 104});
        DrawLine(static_cast<int>(cell.x + cell.width - 3.0f), static_cast<int>(cell.y + 3.0f),
                 static_cast<int>(cell.x + 3.0f),
                 static_cast<int>(cell.y + cell.height - 3.0f), Color{214, 91, 82, 104});
    }
}

void drawAuthoredLightVisibility(const LevelDefinition& level,
                                 const Dungeon& dungeon,
                                 Rectangle map,
                                 float cellSize) {
    for (int y = 0; y < level.height; ++y) {
        for (int x = 0; x < level.width; ++x) {
            const Rectangle cell{map.x + static_cast<float>(x) * cellSize,
                                 map.y + static_cast<float>(y) * cellSize,
                                 cellSize,
                                 cellSize};
            const LightSample sample = Lighting::sampleAt(dungeon,
                                                          static_cast<double>(x) + 0.5,
                                                          static_cast<double>(y) + 0.5,
                                                          x,
                                                          y);
            const float intensity = lightIntensity(sample);
            if (intensity > 0.01f) {
                const Color color{
                    static_cast<unsigned char>(std::clamp(sample.red * 255.0f, 0.0f, 255.0f)),
                    static_cast<unsigned char>(std::clamp(sample.green * 255.0f, 0.0f, 255.0f)),
                    static_cast<unsigned char>(std::clamp(sample.blue * 255.0f, 0.0f, 255.0f)),
                    255,
                };
                drawLightDebugCell(cell, color, intensity);
                continue;
            }

            bool blockedByAnyLight = false;
            for (const auto& light : level.lights) {
                const auto* definition = findLight(light.lightId);
                if (definition == nullptr) continue;
                if (!cellWithinLightRadius(light.x, light.y, x, y, *definition)) continue;
                if (!Lighting::reaches(dungeon, light.x, light.y, x, y)) {
                    blockedByAnyLight = true;
                    break;
                }
            }
            if (blockedByAnyLight) drawBlockedDebugCell(cell);
        }
    }
}

void drawPreviewLightVisibility(const Dungeon& dungeon,
                                int lightX,
                                int lightY,
                                const LightDefinition& light,
                                Rectangle map,
                                float cellSize,
                                int width,
                                int height) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!cellWithinLightRadius(lightX, lightY, x, y, light)) continue;
            const Rectangle cell{map.x + static_cast<float>(x) * cellSize,
                                 map.y + static_cast<float>(y) * cellSize,
                                 cellSize,
                                 cellSize};
            if (!Lighting::reaches(dungeon, lightX, lightY, x, y)) {
                drawBlockedDebugCell(cell);
                continue;
            }
            const float intensity = Lighting::falloff(lightDistance(lightX, lightY, x, y), light.radius) *
                light.intensity;
            if (intensity <= 0.0f) continue;
            drawLightDebugCell(cell, lightColor(light.id), std::min(intensity, 1.0f));
        }
    }
}
}

LevelEditor::LevelEditor(std::string levelPath)
    : levelPath_(std::move(levelPath)),
      randomizer_(std::random_device{}()) {
    loadLevel();
}

bool LevelEditor::consumePlaytestRequest() {
    const bool requested = playtestRequested_;
    playtestRequested_ = false;
    return requested;
}

bool LevelEditor::consumeExitRequest() {
    const bool requested = exitRequested_;
    exitRequested_ = false;
    return requested;
}

void LevelEditor::showLightsLayerForCapture() {
    setLayer(Layer::Lights);
}

void LevelEditor::showStoryLayerForCapture() {
    setLayer(Layer::Story);
    if (!level_.rooms.empty()) selectedRoomIndex_ = 0;
}

void LevelEditor::loadLevel() {
    std::string error;
    LevelDefinition loaded;
    if (LevelIO::load(levelPath_, loaded, error)) {
        level_ = std::move(loaded);
        dirty_ = false;
        status_ = "Loaded " + level_.name;
    } else {
        level_ = levelOneDefinition();
        dirty_ = true;
        status_ = "Load failed; using the built-in template. " + error;
    }
    syncSelectionsFromLevel();
    validationErrors_ = LevelIO::validate(level_);
    undoStack_.clear();
    redoStack_.clear();
    selectedObjectIndex_ = -1;
    selectedRoomIndex_ = -1;
    selectedTriggerIndex_ = -1;
}

void LevelEditor::saveLevel() {
    validationErrors_ = LevelIO::validate(level_);
    if (!validationErrors_.empty()) {
        status_ = "Not saved: " + validationErrors_.front();
        return;
    }
    std::string error;
    if (!LevelIO::save(levelPath_, level_, error)) {
        status_ = "Save failed: " + error;
        return;
    }
    dirty_ = false;
    status_ = "Saved. The game will load this level on its next launch.";
}

void LevelEditor::saveLevelAs() {
    std::filesystem::path directory = std::filesystem::path{levelPath_}.parent_path();
    std::string stem = level_.id.empty() ? "new-level" : level_.id;
    std::replace_if(stem.begin(), stem.end(), [](unsigned char ch) {
        return !std::isalnum(ch) && ch != '-' && ch != '_';
    }, '-');
    if (stem.empty()) stem = "new-level";
    std::filesystem::path candidate = directory / (stem + ".svl");
    int suffix = 2;
    std::error_code errorCode;
    while (std::filesystem::exists(candidate, errorCode)) {
        candidate = directory / (stem + "-" + std::to_string(suffix++) + ".svl");
        errorCode.clear();
    }
    const std::string previous = levelPath_;
    levelPath_ = candidate.string();
    saveLevel();
    if (dirty_) {
        levelPath_ = previous;
    } else {
        status_ = "Saved copy as " + candidate.filename().string() + ".";
    }
}

void LevelEditor::newLevel() {
    recordUndo();
    LevelDefinition fresh;
    fresh.id = "level.new";
    fresh.name = "NEW STORY LEVEL";
    fresh.width = 16;
    fresh.height = 16;
    fresh.map.assign(16, std::string(16, '.'));
    std::fill(fresh.map.front().begin(), fresh.map.front().end(), '#');
    std::fill(fresh.map.back().begin(), fresh.map.back().end(), '#');
    for (auto& row : fresh.map) {
        row.front() = '#';
        row.back() = '#';
    }
    fresh.spawnX = 2;
    fresh.spawnY = 2;
    fresh.spawnDirection = 1;
    fresh.map[13][13] = 'E';
    level_ = std::move(fresh);
    syncSelectionsFromLevel();
    selectedObjectIndex_ = -1;
    selectedRoomIndex_ = -1;
    selectedTriggerIndex_ = -1;
    dirty_ = true;
    refreshValidation("New level created. Edit its ID/name, then use Save As.");
}

void LevelEditor::requestDestructiveAction(PendingAction action) {
    if (!dirty_) {
        pendingAction_ = action;
        completePendingAction();
        return;
    }
    pendingAction_ = action;
    status_ = "Unsaved work: save, discard, or cancel.";
}

void LevelEditor::completePendingAction() {
    const PendingAction action = pendingAction_;
    pendingAction_ = PendingAction::None;
    if (action == PendingAction::Exit) exitRequested_ = true;
    else if (action == PendingAction::Reload) loadLevel();
    else if (action == PendingAction::NewLevel) newLevel();
}

void LevelEditor::recordUndo() {
    undoStack_.push_back(level_);
    if (undoStack_.size() > 64) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

void LevelEditor::undo() {
    if (undoStack_.empty()) {
        status_ = "Nothing to undo.";
        return;
    }
    redoStack_.push_back(level_);
    level_ = std::move(undoStack_.back());
    undoStack_.pop_back();
    dirty_ = true;
    syncSelectionsFromLevel();
    refreshValidation("Undo.");
}

void LevelEditor::redo() {
    if (redoStack_.empty()) {
        status_ = "Nothing to redo.";
        return;
    }
    undoStack_.push_back(level_);
    level_ = std::move(redoStack_.back());
    redoStack_.pop_back();
    dirty_ = true;
    syncSelectionsFromLevel();
    refreshValidation("Redo.");
}

std::string* LevelEditor::activeText() {
    if (textField_ == TextField::LevelId) return &level_.id;
    if (textField_ == TextField::LevelName) return &level_.name;
    if (selectedObjectIndex_ >= 0 && selectedObjectIndex_ < static_cast<int>(level_.objects.size())) {
        auto& object = level_.objects[static_cast<std::size_t>(selectedObjectIndex_)];
        if (textField_ == TextField::ObjectName) return &object.name;
        if (textField_ == TextField::ObjectText) return &object.text;
    }
    if (selectedRoomIndex_ >= 0 && selectedRoomIndex_ < static_cast<int>(level_.rooms.size())) {
        auto& room = level_.rooms[static_cast<std::size_t>(selectedRoomIndex_)];
        if (textField_ == TextField::RoomName) return &room.name;
        if (textField_ == TextField::RoomPurpose) return &room.purpose;
        if (textField_ == TextField::RoomMood) return &room.mood;
        if (textField_ == TextField::RoomLore) return &room.lore;
        if (textField_ == TextField::RoomFeeling) return &room.intendedFeeling;
    }
    if (selectedTriggerIndex_ >= 0 && selectedTriggerIndex_ < static_cast<int>(level_.triggers.size()) &&
        textField_ == TextField::TriggerMessage) {
        return &level_.triggers[static_cast<std::size_t>(selectedTriggerIndex_)].message;
    }
    return nullptr;
}

void LevelEditor::beginTextEdit(TextField field) {
    if (textField_ != field) recordUndo();
    textField_ = field;
    status_ = "Typing. Enter accepts; Escape stops editing.";
}

void LevelEditor::finishTextEdit() {
    textField_ = TextField::None;
    dirty_ = true;
    refreshValidation("Text updated.");
}

void LevelEditor::updateTextEdit() {
    std::string* value = activeText();
    if (value == nullptr) {
        textField_ = TextField::None;
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
        finishTextEdit();
        return;
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !value->empty()) value->pop_back();
    for (int codepoint = GetCharPressed(); codepoint > 0; codepoint = GetCharPressed()) {
        if (codepoint >= 32 && codepoint <= 126 && value->size() < 240) {
            value->push_back(static_cast<char>(codepoint));
        }
    }
    dirty_ = true;
}

void LevelEditor::refreshValidation(const std::string& successMessage) {
    validationErrors_ = LevelIO::validate(level_);
    status_ = validationErrors_.empty() ? successMessage : "Invalid: " + validationErrors_.front();
}

void LevelEditor::syncSelectionsFromLevel() {
    selectedWallMaterial_ = level_.surfaces.wallMaterial;
    selectedFloorMaterial_ = level_.surfaces.floorMaterial;
    selectedCeilingMaterial_ = level_.surfaces.ceilingMaterial;
    selectedCeilingSky_ = level_.surfaces.ceilingMode == CeilingMode::Sky;
}

void LevelEditor::setLayer(Layer layer) {
    layer_ = layer;
    switch (layer_) {
        case Layer::Structure: status_ = "Structure layer: paint the navigable map."; break;
        case Layer::Wall: status_ = "Wall layer: paint finishes onto solid wall cells."; break;
        case Layer::Floor: status_ = "Floor layer: paint finishes onto walkable cells."; break;
        case Layer::Ceiling: status_ = "Ceiling layer: paint a material or open sky."; break;
        case Layer::Objects: status_ = "Object layer: place enemies, pickups, doors, props, lore, and NPCs."; break;
        case Layer::Story: status_ = "Story layer: draw rooms and author their purpose, mood, and lore."; break;
        case Layer::Triggers: status_ = "Trigger layer: connect dungeon events to discovery text."; break;
        case Layer::Lights: status_ = "Light layer: place or erase torches cell by cell."; break;
        case Layer::Audio: status_ = "Audio layer: choose the looping music for this level."; break;
    }
}

bool LevelEditor::hasLightAt(int x, int y) const {
    for (const auto& light : level_.lights) {
        if (light.x == x && light.y == y) return true;
    }
    return false;
}

bool LevelEditor::hasObjectAt(int x, int y) const {
    if (level_.spawnX == x && level_.spawnY == y) return true;
    for (const auto& pickup : level_.pickups) {
        if (pickup.x == x && pickup.y == y) return true;
    }
    for (const auto& enemy : level_.enemies) {
        if (enemy.x == x && enemy.y == y) return true;
    }
    for (const auto& object : level_.objects) {
        if (object.x == x && object.y == y) return true;
    }
    return false;
}

SurfaceKind LevelEditor::selectedSurface() const {
    if (layer_ == Layer::Wall) return SurfaceKind::Wall;
    if (layer_ == Layer::Floor) return SurfaceKind::Floor;
    return SurfaceKind::Ceiling;
}

const std::string& LevelEditor::selectedMaterial() const {
    if (layer_ == Layer::Wall) return selectedWallMaterial_;
    if (layer_ == Layer::Floor) return selectedFloorMaterial_;
    return selectedCeilingMaterial_;
}

std::string& LevelEditor::selectedMaterial() {
    if (layer_ == Layer::Wall) return selectedWallMaterial_;
    if (layer_ == Layer::Floor) return selectedFloorMaterial_;
    return selectedCeilingMaterial_;
}

void LevelEditor::paintStructure(int x, int y) {
    const bool boundary = x == 0 || y == 0 || x == level_.width - 1 || y == level_.height - 1;
    if (boundary) {
        status_ = "Boundary cells are locked to walls.";
        return;
    }

    if (structureBrush_ == StructureBrush::Spawn) {
        if (isSolid(level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)])) {
            status_ = "The player spawn must be placed on a walkable cell.";
            return;
        }
        recordUndo();
        level_.spawnX = x;
        level_.spawnY = y;
        dirty_ = true;
        refreshValidation("Player spawn moved.");
        return;
    }

    const char marker = markerForBrush(static_cast<int>(structureBrush_));
    if (marker == '#' && hasObjectAt(x, y)) {
        status_ = "Move the spawn or existing object before placing a wall here.";
        return;
    }

    if (marker == 'E') {
        for (auto& row : level_.map) {
            std::replace(row.begin(), row.end(), 'E', '.');
        }
    }

    auto& cell = level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
    if (cell == marker) return;
    recordUndo();
    const char previousMarker = cell;
    cell = marker;

    if (previousMarker == 'D' && marker != 'D') {
        level_.doors.erase(std::remove_if(level_.doors.begin(), level_.doors.end(), [x, y](const DoorPlacement& door) {
            return door.x == x && door.y == y;
        }), level_.doors.end());
    }
    if (marker == 'D') {
        const auto found = std::find_if(level_.doors.begin(), level_.doors.end(), [x, y](const DoorPlacement& door) {
            return door.x == x && door.y == y;
        });
        if (found == level_.doors.end()) {
            level_.doors.push_back({stableId(level_, "door", x, y), x, y, DoorKind::Door, true});
        }
    }

    level_.surfaceOverrides.erase(
        std::remove_if(level_.surfaceOverrides.begin(), level_.surfaceOverrides.end(),
                       [x, y, marker](const CellSurfaceOverride& surfaceOverride) {
                           if (surfaceOverride.x != x || surfaceOverride.y != y) return false;
                           return marker == '#' ? surfaceOverride.surface != SurfaceKind::Wall
                                                : surfaceOverride.surface == SurfaceKind::Wall;
                       }),
        level_.surfaceOverrides.end());
    dirty_ = true;
    refreshValidation("Structure painted.");
}

void LevelEditor::paintSurface(int x, int y, SurfaceKind surface) {
    const char marker = level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
    if (surface == SurfaceKind::Wall && !isSolid(marker)) {
        status_ = "Wall finishes can only be painted onto solid wall cells.";
        return;
    }
    if (surface != SurfaceKind::Wall && isSolid(marker)) {
        status_ = "Floor and ceiling finishes can only be painted onto walkable cells.";
        return;
    }

    const CeilingMode mode = surface == SurfaceKind::Ceiling && selectedCeilingSky_
        ? CeilingMode::Sky
        : CeilingMode::Material;
    const std::string materialId = mode == CeilingMode::Sky ? std::string{} : selectedMaterial();

    bool matchesDefault = false;
    if (surface == SurfaceKind::Wall) matchesDefault = materialId == level_.surfaces.wallMaterial;
    if (surface == SurfaceKind::Floor) matchesDefault = materialId == level_.surfaces.floorMaterial;
    if (surface == SurfaceKind::Ceiling) {
        matchesDefault = mode == level_.surfaces.ceilingMode &&
            (mode == CeilingMode::Sky || materialId == level_.surfaces.ceilingMaterial);
    }

    const auto existing = std::find_if(level_.surfaceOverrides.begin(), level_.surfaceOverrides.end(),
                                       [x, y, surface](const CellSurfaceOverride& surfaceOverride) {
                                           return surfaceOverride.x == x && surfaceOverride.y == y &&
                                               surfaceOverride.surface == surface;
                                       });
    recordUndo();
    if (matchesDefault) {
        if (existing != level_.surfaceOverrides.end()) level_.surfaceOverrides.erase(existing);
    } else if (existing != level_.surfaceOverrides.end()) {
        existing->materialId = materialId;
        existing->ceilingMode = mode;
    } else {
        level_.surfaceOverrides.push_back({x, y, materialId, surface, mode});
    }
    dirty_ = true;
    refreshValidation("Surface painted.");
}

void LevelEditor::paintLight(int x, int y) {
    const auto existing = std::find_if(level_.lights.begin(), level_.lights.end(),
                                       [x, y](const LightPlacement& light) {
                                           return light.x == x && light.y == y;
                                       });
    if (eraseLight_) {
        if (existing == level_.lights.end()) {
            status_ = "No light in that cell.";
            return;
        }
        recordUndo();
        level_.lights.erase(existing);
        dirty_ = true;
        refreshValidation("Light removed.");
        return;
    }

    const auto* definition = findLight(selectedLightId_);
    if (definition == nullptr) {
        status_ = "Select a light type before painting.";
        return;
    }
    if (existing != level_.lights.end()) {
        if (existing->lightId == selectedLightId_) return;
        recordUndo();
        existing->lightId = selectedLightId_;
    } else {
        if (level_.lights.size() >= Lighting::MaxLightsPerLevel) {
            status_ = "This level already holds the maximum number of lights.";
            return;
        }
        recordUndo();
        level_.lights.push_back({x, y, selectedLightId_});
    }
    dirty_ = true;
    refreshValidation("Placed " + definition->name + ".");
}

void LevelEditor::paintObject(int x, int y) {
    if (x <= 0 || y <= 0 || x >= level_.width - 1 || y >= level_.height - 1) {
        status_ = "Objects must stay inside the boundary.";
        return;
    }

    const auto objectAt = std::find_if(level_.objects.begin(), level_.objects.end(), [x, y](const WorldObject& object) {
        return object.x == x && object.y == y;
    });
    if (objectBrush_ == ObjectBrush::Erase) {
        const bool found = objectAt != level_.objects.end() ||
            std::any_of(level_.pickups.begin(), level_.pickups.end(), [x, y](const Pickup& value) { return value.x == x && value.y == y; }) ||
            std::any_of(level_.enemies.begin(), level_.enemies.end(), [x, y](const Enemy& value) { return value.x == x && value.y == y; }) ||
            std::any_of(level_.doors.begin(), level_.doors.end(), [x, y](const DoorPlacement& value) { return value.x == x && value.y == y; });
        if (!found) {
            status_ = "No editable object in that cell.";
            return;
        }
        recordUndo();
        level_.objects.erase(std::remove_if(level_.objects.begin(), level_.objects.end(), [x, y](const WorldObject& value) {
            return value.x == x && value.y == y;
        }), level_.objects.end());
        level_.pickups.erase(std::remove_if(level_.pickups.begin(), level_.pickups.end(), [x, y](const Pickup& value) {
            return value.x == x && value.y == y;
        }), level_.pickups.end());
        level_.enemies.erase(std::remove_if(level_.enemies.begin(), level_.enemies.end(), [x, y](const Enemy& value) {
            return value.x == x && value.y == y;
        }), level_.enemies.end());
        level_.doors.erase(std::remove_if(level_.doors.begin(), level_.doors.end(), [x, y](const DoorPlacement& value) {
            return value.x == x && value.y == y;
        }), level_.doors.end());
        if (level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == 'D') {
            level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = '.';
        }
        selectedObjectIndex_ = -1;
        dirty_ = true;
        refreshValidation("Object removed.");
        return;
    }

    if (objectAt != level_.objects.end()) {
        selectedObjectIndex_ = static_cast<int>(std::distance(level_.objects.begin(), objectAt));
        status_ = "Object selected. Edit its name and interaction text in the panel.";
        return;
    }
    const bool doorBrush = objectBrush_ == ObjectBrush::LockedDoor ||
        objectBrush_ == ObjectBrush::UnlockedDoor || objectBrush_ == ObjectBrush::Gate;
    const auto doorAt = std::find_if(level_.doors.begin(), level_.doors.end(), [x, y](const DoorPlacement& door) {
        return door.x == x && door.y == y;
    });
    if (doorBrush && doorAt != level_.doors.end()) {
        recordUndo();
        doorAt->kind = objectBrush_ == ObjectBrush::Gate ? DoorKind::Gate : DoorKind::Door;
        doorAt->locked = objectBrush_ != ObjectBrush::UnlockedDoor;
        dirty_ = true;
        refreshValidation("Door or gate settings updated.");
        return;
    }
    if (hasObjectAt(x, y)) {
        status_ = "That cell already holds an enemy, pickup, object, or the player spawn.";
        return;
    }

    if (!doorBrush && isSolid(level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)])) {
        status_ = "Place this object on a walkable cell.";
        return;
    }

    recordUndo();
    if (objectBrush_ == ObjectBrush::EnemyProwler || objectBrush_ == ObjectBrush::EnemyBrute) {
        Enemy enemy;
        enemy.x = x;
        enemy.y = y;
        enemy.typeId = objectBrush_ == ObjectBrush::EnemyBrute ? "enemy.brute" : "enemy.prowler";
        enemy.hp = objectBrush_ == ObjectBrush::EnemyBrute ? 30 : 18;
        enemy.id = stableId(level_, "enemy", x, y);
        level_.enemies.push_back(std::move(enemy));
    } else if (objectBrush_ == ObjectBrush::Key || objectBrush_ == ObjectBrush::Potion) {
        Pickup pickup;
        pickup.type = objectBrush_ == ObjectBrush::Key ? Pickup::Type::Key : Pickup::Type::Potion;
        pickup.x = x;
        pickup.y = y;
        pickup.id = stableId(level_, objectBrush_ == ObjectBrush::Key ? "pickup.key" : "pickup.potion", x, y);
        level_.pickups.push_back(std::move(pickup));
    } else if (doorBrush) {
        level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = 'D';
        const DoorKind kind = objectBrush_ == ObjectBrush::Gate ? DoorKind::Gate : DoorKind::Door;
        const bool locked = objectBrush_ != ObjectBrush::UnlockedDoor;
        level_.doors.push_back({stableId(level_, kind == DoorKind::Gate ? "gate" : "door", x, y), x, y, kind, locked});
    } else {
        WorldObjectKind kind = WorldObjectKind::Prop;
        if (objectBrush_ == ObjectBrush::Shrine) kind = WorldObjectKind::Shrine;
        else if (objectBrush_ == ObjectBrush::Note) kind = WorldObjectKind::Note;
        else if (objectBrush_ == ObjectBrush::Corpse) kind = WorldObjectKind::Corpse;
        else if (objectBrush_ == ObjectBrush::Npc) kind = WorldObjectKind::Npc;
        WorldObject object;
        object.id = stableId(level_, lowercase(objectKindLabel(kind)), x, y);
        object.kind = kind;
        object.x = x;
        object.y = y;
        object.name = objectKindLabel(kind);
        object.text = kind == WorldObjectKind::Note ? "The note has not been written yet."
            : (kind == WorldObjectKind::Shrine ? "The shrine waits in silence." : "Nothing more is known yet.");
        level_.objects.push_back(std::move(object));
        selectedObjectIndex_ = static_cast<int>(level_.objects.size()) - 1;
    }
    dirty_ = true;
    refreshValidation("Object placed.");
}

void LevelEditor::paintStory(int x, int y) {
    const auto containing = std::find_if(level_.rooms.begin(), level_.rooms.end(), [x, y](const StoryRoom& room) {
        return room.contains(x, y);
    });
    if (storyErase_) {
        if (containing == level_.rooms.end()) {
            status_ = "No story room covers that cell.";
            return;
        }
        recordUndo();
        level_.rooms.erase(containing);
        selectedRoomIndex_ = -1;
        dirty_ = true;
        refreshValidation("Story room removed.");
        return;
    }
    if (containing != level_.rooms.end()) {
        selectedRoomIndex_ = static_cast<int>(std::distance(level_.rooms.begin(), containing));
        roomAnchorX_ = -1;
        roomAnchorY_ = -1;
        status_ = "Story room selected. Click a field to edit its meaning.";
        return;
    }
    if (roomAnchorX_ < 0) {
        roomAnchorX_ = x;
        roomAnchorY_ = y;
        status_ = "Room corner set. Click the opposite corner.";
        return;
    }
    const int left = std::min(roomAnchorX_, x);
    const int top = std::min(roomAnchorY_, y);
    const int right = std::max(roomAnchorX_, x);
    const int bottom = std::max(roomAnchorY_, y);
    recordUndo();
    StoryRoom room;
    room.id = stableId(level_, "room", left, top);
    room.x = left;
    room.y = top;
    room.width = right - left + 1;
    room.height = bottom - top + 1;
    room.name = "Unnamed Room";
    room.purpose = "Story purpose";
    room.mood = "Unsettled";
    room.lore = "What happened here?";
    room.intendedFeeling = "Curiosity";
    level_.rooms.push_back(std::move(room));
    selectedRoomIndex_ = static_cast<int>(level_.rooms.size()) - 1;
    roomAnchorX_ = -1;
    roomAnchorY_ = -1;
    dirty_ = true;
    refreshValidation("Story room created. Give it a purpose before decorating it.");
}

void LevelEditor::paintTrigger(int x, int y) {
    const auto existing = std::find_if(level_.triggers.begin(), level_.triggers.end(), [this, x, y](const StoryTrigger& trigger) {
        return trigger.x == x && trigger.y == y && trigger.event == triggerEvent_;
    });
    if (triggerErase_) {
        if (existing == level_.triggers.end()) {
            status_ = "No selected event trigger in that cell.";
            return;
        }
        recordUndo();
        level_.triggers.erase(existing);
        selectedTriggerIndex_ = -1;
        dirty_ = true;
        refreshValidation("Trigger removed.");
        return;
    }
    if (existing != level_.triggers.end()) {
        selectedTriggerIndex_ = static_cast<int>(std::distance(level_.triggers.begin(), existing));
        status_ = "Trigger selected. Edit its discovery message in the panel.";
        return;
    }
    StoryTrigger trigger;
    trigger.id = stableId(level_, "trigger", x, y);
    trigger.event = triggerEvent_;
    trigger.x = x;
    trigger.y = y;
    trigger.message = "A story beat happens here.";
    if (trigger.event == TriggerEvent::EnterRoom) {
        for (const auto& room : level_.rooms) if (room.contains(x, y)) { trigger.subjectId = room.id; break; }
    } else if (trigger.event == TriggerEvent::OpenDoor) {
        for (const auto& door : level_.doors) if (door.x == x && door.y == y) { trigger.subjectId = door.id; break; }
    } else if (trigger.event == TriggerEvent::KillEnemy) {
        for (const auto& enemy : level_.enemies) if (enemy.x == x && enemy.y == y) { trigger.subjectId = enemy.id; break; }
    } else if (trigger.event == TriggerEvent::PickupItem) {
        for (const auto& pickup : level_.pickups) if (pickup.x == x && pickup.y == y) { trigger.subjectId = pickup.id; break; }
    } else if (trigger.event == TriggerEvent::InteractObject) {
        for (const auto& object : level_.objects) if (object.x == x && object.y == y) { trigger.subjectId = object.id; break; }
    }
    recordUndo();
    level_.triggers.push_back(std::move(trigger));
    selectedTriggerIndex_ = static_cast<int>(level_.triggers.size()) - 1;
    dirty_ = true;
    refreshValidation("Trigger placed. Write what the player discovers.");
}

void LevelEditor::paintCell(int x, int y) {
    if (x < 0 || y < 0 || x >= level_.width || y >= level_.height) return;
    if (layer_ == Layer::Structure) paintStructure(x, y);
    else if (layer_ == Layer::Objects) paintObject(x, y);
    else if (layer_ == Layer::Story) paintStory(x, y);
    else if (layer_ == Layer::Triggers) paintTrigger(x, y);
    else if (layer_ == Layer::Lights) paintLight(x, y);
    else if (layer_ == Layer::Audio) status_ = "Use the Audio panel to select this level's loop.";
    else paintSurface(x, y, selectedSurface());
}

void LevelEditor::makeSelectionDefault() {
    recordUndo();
    if (layer_ == Layer::Wall) level_.surfaces.wallMaterial = selectedWallMaterial_;
    if (layer_ == Layer::Floor) level_.surfaces.floorMaterial = selectedFloorMaterial_;
    if (layer_ == Layer::Ceiling) {
        level_.surfaces.ceilingMode = selectedCeilingSky_ ? CeilingMode::Sky : CeilingMode::Material;
        if (!selectedCeilingSky_) level_.surfaces.ceilingMaterial = selectedCeilingMaterial_;
    }
    dirty_ = true;
    refreshValidation("Level default updated. Existing overrides were preserved.");
}

void LevelEditor::randomizeSurfaces() {
    const auto caveWalls = filteredMaterials(SurfaceKind::Wall, {"Cave"});
    const auto portalWalls = filteredMaterials(SurfaceKind::Wall, {"Portal"});
    const auto floors = materialsFor(SurfaceKind::Floor);
    const auto ceilings = materialsFor(SurfaceKind::Ceiling);

    if (caveWalls.empty() || floors.empty() || ceilings.empty()) {
        status_ = "Randomize needs at least one wall, floor, and ceiling material.";
        return;
    }

    recordUndo();
    level_.surfaceOverrides.clear();

    level_.surfaces.wallMaterial = chooseMaterial(caveWalls, randomizer_)->id;
    level_.surfaces.floorMaterial = chooseMaterial(floors, randomizer_)->id;

    std::bernoulli_distribution skyDefault(0.18);
    if (skyDefault(randomizer_)) {
        level_.surfaces.ceilingMode = CeilingMode::Sky;
    } else {
        level_.surfaces.ceilingMode = CeilingMode::Material;
        level_.surfaces.ceilingMaterial = chooseMaterial(ceilings, randomizer_)->id;
    }

    selectedWallMaterial_ = level_.surfaces.wallMaterial;
    selectedFloorMaterial_ = level_.surfaces.floorMaterial;
    selectedCeilingMaterial_ = level_.surfaces.ceilingMaterial;
    selectedCeilingSky_ = level_.surfaces.ceilingMode == CeilingMode::Sky;

    std::bernoulli_distribution wallVariation(0.34);
    std::bernoulli_distribution portalVariation(0.45);
    std::bernoulli_distribution floorVariation(0.24);
    std::bernoulli_distribution ceilingVariation(0.24);
    std::bernoulli_distribution skyVariation(0.16);

    constexpr std::size_t OverrideBudget = 1400;
    const auto hasAdjacentDoor = [this](int x, int y) {
        constexpr std::array<std::pair<int, int>, 4> offsets = {
            std::pair<int, int>{1, 0},
            std::pair<int, int>{-1, 0},
            std::pair<int, int>{0, 1},
            std::pair<int, int>{0, -1},
        };
        for (const auto& [dx, dy] : offsets) {
            const int nx = x + dx;
            const int ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= level_.width || ny >= level_.height) continue;
            if (isDoorMarker(level_.map[static_cast<std::size_t>(ny)][static_cast<std::size_t>(nx)])) {
                return true;
            }
        }
        return false;
    };
    const auto canAddOverride = [this]() {
        return level_.surfaceOverrides.size() < OverrideBudget;
    };
    const auto addOverride = [this, &canAddOverride](int x,
                                                     int y,
                                                     SurfaceKind surface,
                                                     CeilingMode mode,
                                                     const std::string& materialId) {
        if (!canAddOverride()) return;
        level_.surfaceOverrides.push_back({x, y, materialId, surface, mode});
    };

    for (int y = 0; y < level_.height; ++y) {
        for (int x = 0; x < level_.width; ++x) {
            const char marker = level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            if (isSolid(marker)) {
                const MaterialDefinition* material = nullptr;
                if (!portalWalls.empty() && hasAdjacentDoor(x, y) && portalVariation(randomizer_)) {
                    material = chooseMaterial(portalWalls, randomizer_);
                } else if (wallVariation(randomizer_)) {
                    material = chooseMaterial(caveWalls, randomizer_);
                }
                if (material != nullptr && material->id != level_.surfaces.wallMaterial) {
                    addOverride(x, y, SurfaceKind::Wall, CeilingMode::Material, material->id);
                }
            } else {
                if (floorVariation(randomizer_)) {
                    const auto* material = chooseMaterial(floors, randomizer_);
                    if (material != nullptr && material->id != level_.surfaces.floorMaterial) {
                        addOverride(x, y, SurfaceKind::Floor, CeilingMode::Material, material->id);
                    }
                }
                if (ceilingVariation(randomizer_)) {
                    if (skyVariation(randomizer_)) {
                        if (level_.surfaces.ceilingMode != CeilingMode::Sky) {
                            addOverride(x, y, SurfaceKind::Ceiling, CeilingMode::Sky, {});
                        }
                    } else {
                        const auto* material = chooseMaterial(ceilings, randomizer_);
                        const bool matchesDefault = level_.surfaces.ceilingMode == CeilingMode::Material &&
                            material != nullptr && material->id == level_.surfaces.ceilingMaterial;
                        if (material != nullptr && !matchesDefault) {
                            addOverride(x, y, SurfaceKind::Ceiling, CeilingMode::Material, material->id);
                        }
                    }
                }
            }
        }
    }

    dirty_ = true;
    refreshValidation("Randomized surfaces. Click Randomize again for a different mix.");
}

void LevelEditor::openImportPanel() {
    importPanelOpen_ = true;
    importStatus_ = "Copy a STONEVEIL_BLUEPRINT block, then click Paste + Import.";
}

void LevelEditor::importBlueprintFromClipboard() {
    const char* clipboard = GetClipboardText();
    if (clipboard == nullptr || std::string(clipboard).empty()) {
        importStatus_ = "Clipboard is empty.";
        status_ = importStatus_;
        return;
    }

    LevelDefinition imported;
    std::string error;
    if (!LevelBlueprint::parse(clipboard, imported, error)) {
        importStatus_ = error;
        status_ = "Import failed: " + error;
        return;
    }

    recordUndo();
    level_ = std::move(imported);
    syncSelectionsFromLevel();
    validationErrors_ = LevelIO::validate(level_);
    dirty_ = true;
    importPanelOpen_ = false;
    layer_ = Layer::Structure;
    status_ = "Imported blueprint: " + level_.name + ". Playtest or save when ready.";
}

void LevelEditor::importDroppedAudio() {
    FilePathList dropped = LoadDroppedFiles();
    if (dropped.count == 0) {
        UnloadDroppedFiles(dropped);
        return;
    }
    const std::filesystem::path contentDirectory = std::filesystem::path{levelPath_}.parent_path().parent_path();
    const std::filesystem::path musicDirectory = contentDirectory / "audio" / "music";
    std::error_code error;
    std::filesystem::create_directories(musicDirectory, error);
    int imported = 0;
    for (unsigned int index = 0; index < dropped.count; ++index) {
        const std::filesystem::path source{dropped.paths[index]};
        if (!hasMusicExtension(source) || !std::filesystem::is_regular_file(source, error)) {
            error.clear();
            continue;
        }
        std::filesystem::path destination = musicDirectory / source.filename();
        int suffix = 2;
        while (std::filesystem::exists(destination, error)) {
            destination = musicDirectory / (source.stem().string() + "-" + std::to_string(suffix++) + source.extension().string());
            error.clear();
        }
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::none, error);
        if (!error) ++imported;
        error.clear();
    }
    UnloadDroppedFiles(dropped);
    status_ = imported > 0
        ? "Imported " + std::to_string(imported) + " audio file(s). Select one on the AUDIO tab."
        : "No supported audio imported. Drop .wav, .ogg, .mp3, or .flac files.";
}

void LevelEditor::resizeLevel(int widthDelta, int heightDelta) {
    const int newWidth = std::clamp(level_.width + widthDelta,
                                    LevelDefinition::MinimumDimension,
                                    LevelDefinition::MaximumDimension);
    const int newHeight = std::clamp(level_.height + heightDelta,
                                     LevelDefinition::MinimumDimension,
                                     LevelDefinition::MaximumDimension);
    if (newWidth == level_.width && newHeight == level_.height) {
        status_ = "Level size is already at the editor limit.";
        return;
    }

    const auto remainsInside = [newWidth, newHeight](int x, int y) {
        return x > 0 && y > 0 && x < newWidth - 1 && y < newHeight - 1;
    };
    if (!remainsInside(level_.spawnX, level_.spawnY)) {
        status_ = "Move the player spawn inward before shrinking this edge.";
        return;
    }
    for (const auto& pickup : level_.pickups) {
        if (!remainsInside(pickup.x, pickup.y)) {
            status_ = "Move pickups inward before shrinking this edge.";
            return;
        }
    }
    for (const auto& enemy : level_.enemies) {
        if (!remainsInside(enemy.x, enemy.y)) {
            status_ = "Move enemies inward before shrinking this edge.";
            return;
        }
    }
    for (const auto& object : level_.objects) {
        if (!remainsInside(object.x, object.y)) {
            status_ = "Move world objects inward before shrinking this edge.";
            return;
        }
    }
    for (const auto& door : level_.doors) {
        if (!remainsInside(door.x, door.y)) {
            status_ = "Move doors and gates inward before shrinking this edge.";
            return;
        }
    }
    for (const auto& room : level_.rooms) {
        if (room.x < 0 || room.y < 0 || room.x + room.width > newWidth || room.y + room.height > newHeight) {
            status_ = "Resize or move story rooms before shrinking this edge.";
            return;
        }
    }
    for (const auto& trigger : level_.triggers) {
        if (trigger.x < 0 || trigger.y < 0 || trigger.x >= newWidth || trigger.y >= newHeight) {
            status_ = "Move story triggers inward before shrinking this edge.";
            return;
        }
    }
    for (const auto& water : level_.water) {
        if (water.x >= newWidth || water.y >= newHeight) {
            status_ = "Move water inward before shrinking this edge.";
            return;
        }
    }
    for (const auto& light : level_.lights) {
        // Lights may legitimately sit in boundary walls, so they only need to
        // stay on the map rather than strictly inside it.
        if (light.x >= newWidth || light.y >= newHeight) {
            status_ = "Move lights inward before shrinking this edge.";
            return;
        }
    }
    for (int y = 0; y < level_.height; ++y) {
        for (int x = 0; x < level_.width; ++x) {
            if (level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == 'E' &&
                !remainsInside(x, y)) {
                status_ = "Move the exit inward before shrinking this edge.";
                return;
            }
        }
    }

    recordUndo();
    std::vector<std::string> resized(static_cast<std::size_t>(newHeight),
                                     std::string(static_cast<std::size_t>(newWidth), '.'));
    for (int y = 0; y < newHeight; ++y) {
        resized[static_cast<std::size_t>(y)][0] = '#';
        resized[static_cast<std::size_t>(y)][static_cast<std::size_t>(newWidth - 1)] = '#';
    }
    std::fill(resized.front().begin(), resized.front().end(), '#');
    std::fill(resized.back().begin(), resized.back().end(), '#');

    const int copiedWidth = std::min(level_.width - 1, newWidth - 1);
    const int copiedHeight = std::min(level_.height - 1, newHeight - 1);
    for (int y = 1; y < copiedHeight; ++y) {
        for (int x = 1; x < copiedWidth; ++x) {
            resized[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
        }
    }

    level_.width = newWidth;
    level_.height = newHeight;
    level_.map = std::move(resized);
    level_.surfaceOverrides.erase(
        std::remove_if(level_.surfaceOverrides.begin(), level_.surfaceOverrides.end(),
                       [newWidth, newHeight](const CellSurfaceOverride& surfaceOverride) {
                           return surfaceOverride.x <= 0 || surfaceOverride.y <= 0 ||
                               surfaceOverride.x >= newWidth - 1 || surfaceOverride.y >= newHeight - 1;
                       }),
        level_.surfaceOverrides.end());
    dirty_ = true;
    refreshValidation("Level resized. New interior cells use the current defaults.");
}

void LevelEditor::requestPlaytest() {
    validationErrors_ = LevelIO::validate(level_);
    if (!validationErrors_.empty()) {
        status_ = "Cannot play: " + validationErrors_.front();
        return;
    }
    playtestRequested_ = true;
    status_ = "Starting an unsaved in-editor playtest.";
}

void LevelEditor::update() {
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsFileDropped()) importDroppedAudio();
    if (pendingAction_ != PendingAction::None) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            pendingAction_ = PendingAction::None;
            status_ = "Kept editing; nothing was discarded.";
        } else if (IsKeyPressed(KEY_D)) {
            completePendingAction();
        } else if (IsKeyPressed(KEY_S)) {
            saveLevel();
            if (!dirty_) completePendingAction();
        }
        return;
    }
    if (textField_ != TextField::None) {
        updateTextEdit();
        return;
    }
    if (importPanelOpen_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            importPanelOpen_ = false;
            status_ = "Blueprint import cancelled.";
            return;
        }
        const Vector2 mouse = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mouse, importPasteButton())) {
                importBlueprintFromClipboard();
                return;
            }
            if (CheckCollisionPointRec(mouse, importCancelButton())) {
                importPanelOpen_ = false;
                status_ = "Blueprint import cancelled.";
                return;
            }
        }
        if (control && IsKeyPressed(KEY_V)) {
            importBlueprintFromClipboard();
            return;
        }
        return;
    }

    if (control && IsKeyPressed(KEY_S)) {
        if (shift) saveLevelAs(); else saveLevel();
    }
    if (control && IsKeyPressed(KEY_R)) requestDestructiveAction(PendingAction::Reload);
    if (control && IsKeyPressed(KEY_N)) requestDestructiveAction(PendingAction::NewLevel);
    if (control && IsKeyPressed(KEY_Z)) undo();
    if (control && IsKeyPressed(KEY_Y)) redo();
    if (IsKeyPressed(KEY_V)) refreshValidation("Level is valid and ready to save.");
    if (IsKeyPressed(KEY_P)) requestPlaytest();
    if (IsKeyPressed(KEY_ESCAPE)) {
        requestDestructiveAction(PendingAction::Exit);
        return;
    }

    const Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        for (int index = 0; index < 4; ++index) {
            if (CheckCollisionPointRec(mouse, dimensionButton(index))) {
                constexpr std::array<int, 4> widthChanges = {-1, 1, 0, 0};
                constexpr std::array<int, 4> heightChanges = {0, 0, -1, 1};
                const int step = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 8 : 1;
                resizeLevel(widthChanges[static_cast<std::size_t>(index)] * step,
                            heightChanges[static_cast<std::size_t>(index)] * step);
                return;
            }
        }
        if (CheckCollisionPointRec(mouse, playButton())) {
            requestPlaytest();
            return;
        }
        if (CheckCollisionPointRec(mouse, randomizeButton())) {
            randomizeSurfaces();
            return;
        }
        if (CheckCollisionPointRec(mouse, saveButton())) {
            saveLevel();
            return;
        }
        if (CheckCollisionPointRec(mouse, undoButton())) { undo(); return; }
        if (CheckCollisionPointRec(mouse, redoButton())) { redo(); return; }
        if (CheckCollisionPointRec(mouse, newButton())) {
            requestDestructiveAction(PendingAction::NewLevel);
            return;
        }
        if (CheckCollisionPointRec(mouse, saveAsButton())) { saveLevelAs(); return; }
        if (CheckCollisionPointRec(mouse, editorMenuButton())) {
            requestDestructiveAction(PendingAction::Exit);
            return;
        }
        if (CheckCollisionPointRec(mouse, importButton())) {
            openImportPanel();
            return;
        }
        for (int index = 0; index < LayerCount; ++index) {
            if (CheckCollisionPointRec(mouse, layerButton(index))) {
                setLayer(static_cast<Layer>(index));
                return;
            }
        }

        if (layer_ == Layer::Structure) {
            static constexpr std::array<const char*, 6> labels = {
                "Floor / Open", "Solid Wall", "Door", "Secret Door", "Exit", "Player Spawn",
            };
            for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
                const Rectangle bounds{OptionX, 130.0f + static_cast<float>(index) * 42.0f, OptionWidth, 34.0f};
                if (CheckCollisionPointRec(mouse, bounds)) {
                    structureBrush_ = static_cast<StructureBrush>(index);
                    status_ = std::string("Selected ") + labels[static_cast<std::size_t>(index)] + ".";
                    return;
                }
            }
        } else if (layer_ == Layer::Objects) {
            static constexpr std::array<const char*, 13> labels = {
                "Erase", "Enemy: Prowler", "Enemy: Brute", "Pickup: Key", "Pickup: Potion",
                "Locked Door", "Unlocked Door", "Gate", "Prop", "Shrine", "Note / Inscription",
                "Corpse", "NPC",
            };
            for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
                if (CheckCollisionPointRec(mouse, objectBrushButton(index))) {
                    objectBrush_ = static_cast<ObjectBrush>(index);
                    status_ = std::string("Selected ") + labels[static_cast<std::size_t>(index)] + ".";
                    return;
                }
            }
            if (selectedObjectIndex_ >= 0 && selectedObjectIndex_ < static_cast<int>(level_.objects.size())) {
                if (CheckCollisionPointRec(mouse, storyFieldBounds(4))) { beginTextEdit(TextField::ObjectName); return; }
                if (CheckCollisionPointRec(mouse, storyFieldBounds(5))) { beginTextEdit(TextField::ObjectText); return; }
            }
        } else if (layer_ == Layer::Story) {
            if (CheckCollisionPointRec(mouse, storyModeButton(false))) {
                storyErase_ = false;
                status_ = "Draw Room selected. Click two corners, or click a room to select it.";
                return;
            }
            if (CheckCollisionPointRec(mouse, storyModeButton(true))) {
                storyErase_ = true;
                roomAnchorX_ = -1;
                roomAnchorY_ = -1;
                status_ = "Erase Room selected.";
                return;
            }
            if (CheckCollisionPointRec(mouse, storyFieldBounds(0))) { beginTextEdit(TextField::LevelId); return; }
            if (CheckCollisionPointRec(mouse, storyFieldBounds(1))) { beginTextEdit(TextField::LevelName); return; }
            if (selectedRoomIndex_ >= 0 && selectedRoomIndex_ < static_cast<int>(level_.rooms.size())) {
                if (CheckCollisionPointRec(mouse, storyFieldBounds(2))) { beginTextEdit(TextField::RoomName); return; }
                if (CheckCollisionPointRec(mouse, storyFieldBounds(3))) { beginTextEdit(TextField::RoomPurpose); return; }
                if (CheckCollisionPointRec(mouse, storyFieldBounds(4))) { beginTextEdit(TextField::RoomMood); return; }
                if (CheckCollisionPointRec(mouse, storyFieldBounds(5))) { beginTextEdit(TextField::RoomLore); return; }
                if (CheckCollisionPointRec(mouse, storyFieldBounds(6))) { beginTextEdit(TextField::RoomFeeling); return; }
            }
        } else if (layer_ == Layer::Triggers) {
            for (int index = 0; index < 6; ++index) {
                if (CheckCollisionPointRec(mouse, triggerEventButton(index))) {
                    triggerEvent_ = static_cast<TriggerEvent>(index);
                    triggerErase_ = false;
                    status_ = std::string("Selected ") + triggerEventName(triggerEvent_) + " trigger.";
                    return;
                }
            }
            if (CheckCollisionPointRec(mouse, triggerEraseButton())) {
                triggerErase_ = true;
                status_ = "Erase selected event triggers.";
                return;
            }
            if (selectedTriggerIndex_ >= 0 && selectedTriggerIndex_ < static_cast<int>(level_.triggers.size()) &&
                CheckCollisionPointRec(mouse, triggerMessageBounds())) {
                beginTextEdit(TextField::TriggerMessage);
                return;
            }
        } else if (layer_ == Layer::Lights) {
            if (CheckCollisionPointRec(mouse, eraseLightButton())) {
                eraseLight_ = true;
                status_ = "Erase brush selected. Click a light to remove it.";
                return;
            }
            for (const auto& button : lightButtons(FirstLightY)) {
                if (CheckCollisionPointRec(mouse, button.bounds)) {
                    selectedLightId_ = button.light->id;
                    eraseLight_ = false;
                    status_ = "Selected " + button.light->category + " / " + button.light->name + ".";
                    return;
                }
            }
        } else if (layer_ == Layer::Audio) {
            if (CheckCollisionPointRec(mouse, noMusicButton())) {
                recordUndo();
                level_.musicPath.clear();
                dirty_ = true;
                refreshValidation("Level music cleared.");
                return;
            }
            for (const auto& button : musicTrackButtons(FirstMusicY)) {
                if (CheckCollisionPointRec(mouse, button.bounds)) {
                    recordUndo();
                    level_.musicPath = button.path;
                    dirty_ = true;
                    refreshValidation("Selected music loop: " + button.label + ".");
                    return;
                }
            }
        } else {
            float firstMaterialY = 130.0f;
            if (layer_ == Layer::Ceiling) {
                const Rectangle skyBounds{OptionX, 154.0f, OptionWidth, 34.0f};
                if (CheckCollisionPointRec(mouse, skyBounds)) {
                    selectedCeilingSky_ = true;
                    status_ = "Selected Sky ceiling mode.";
                    return;
                }
                firstMaterialY = 211.0f;
            }
            const auto buttons = materialButtons(selectedSurface(), firstMaterialY);
            for (const auto& button : buttons) {
                if (CheckCollisionPointRec(mouse, button.bounds)) {
                    selectedMaterial() = button.material->id;
                    if (layer_ == Layer::Ceiling) selectedCeilingSky_ = false;
                    status_ = "Selected " + button.material->category + " / " + button.material->name + ".";
                    return;
                }
            }
            const float defaultY = buttons.empty() ? firstMaterialY : buttons.back().bounds.y + 50.0f;
            const Rectangle defaultBounds{OptionX, defaultY, OptionWidth, 36.0f};
            if (CheckCollisionPointRec(mouse, defaultBounds)) {
                makeSelectionDefault();
                return;
            }
        }
    }

    const Rectangle map = mapBounds(level_);
    const float cellSize = mapCellSize(level_);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, map)) {
        const int x = static_cast<int>((mouse.x - map.x) / cellSize);
        const int y = static_cast<int>((mouse.y - map.y) / cellSize);
        if (x != lastPaintX_ || y != lastPaintY_) {
            paintCell(x, y);
            lastPaintX_ = x;
            lastPaintY_ = y;
        }
    } else {
        lastPaintX_ = -1;
        lastPaintY_ = -1;
    }
}

void LevelEditor::draw() const {
    ClearBackground(Background);
    const Rectangle map = mapBounds(level_);
    const float cellSize = mapCellSize(level_);
    DrawText("STONEVEIL DUNGEON EDITOR", 32, 22, 26, Text);
    DrawText(dirty_ ? "UNSAVED" : "SAVED", 472, 28, 14, dirty_ ? Accent : Valid);
    drawButton(undoButton(), "UNDO", false, 11);
    drawButton(redoButton(), "REDO", false, 11);
    drawButton(newButton(), "NEW", false, 11);
    drawButton(saveAsButton(), "SAVE AS", false, 11);

    static constexpr std::array<const char*, LayerCount> layerLabels = {
        "MAP", "WALL", "FLOOR", "CEIL", "OBJ", "STORY", "EVENT", "LIGHT", "AUDIO",
    };
    for (int index = 0; index < LayerCount; ++index) {
        drawButton(layerButton(index), layerLabels[static_cast<std::size_t>(index)],
                   static_cast<int>(layer_) == index, 10);
    }

    for (int y = 0; y < level_.height; ++y) {
        for (int x = 0; x < level_.width; ++x) {
            const char marker = level_.map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            const Rectangle cell{map.x + static_cast<float>(x) * cellSize,
                                 map.y + static_cast<float>(y) * cellSize,
                                 cellSize,
                                 cellSize};
            Color cellColor = isSolid(marker) ? Color{57, 62, 69, 255} : Color{31, 36, 43, 255};

            if (layer_ == Layer::Wall || layer_ == Layer::Floor || layer_ == Layer::Ceiling) {
                const SurfaceKind surface = selectedSurface();
                const bool compatible = surface == SurfaceKind::Wall ? isSolid(marker) : !isSolid(marker);
                if (compatible) {
                    CeilingMode mode = surface == SurfaceKind::Ceiling ? level_.surfaces.ceilingMode : CeilingMode::Material;
                    std::string materialId;
                    if (surface == SurfaceKind::Wall) materialId = level_.surfaces.wallMaterial;
                    if (surface == SurfaceKind::Floor) materialId = level_.surfaces.floorMaterial;
                    if (surface == SurfaceKind::Ceiling) materialId = level_.surfaces.ceilingMaterial;
                    for (const auto& surfaceOverride : level_.surfaceOverrides) {
                        if (surfaceOverride.x == x && surfaceOverride.y == y && surfaceOverride.surface == surface) {
                            mode = surfaceOverride.ceilingMode;
                            materialId = surfaceOverride.materialId;
                        }
                    }
                    cellColor = mode == CeilingMode::Sky ? Color{72, 121, 159, 255} : materialColor(materialId);
                } else {
                    cellColor = {25, 28, 34, 255};
                }
            }

            DrawRectangleRec(cell, cellColor);
            DrawRectangleLinesEx(cell, 1.0f, Color{19, 22, 27, 255});

            if ((layer_ == Layer::Structure || layer_ == Layer::Objects || layer_ == Layer::Story ||
                 layer_ == Layer::Triggers || layer_ == Layer::Lights || layer_ == Layer::Audio) &&
                marker != '.' && marker != '#' &&
                cellSize >= 14.0f) {
                const char markerText[2] = {marker, '\0'};
                const int markerSize = std::max(10, static_cast<int>(cellSize * 0.55f));
                DrawText(markerText, static_cast<int>(cell.x + cellSize * 0.34f),
                         static_cast<int>(cell.y + cellSize * 0.22f), markerSize,
                         marker == 'E' ? Valid : (marker == 'D' ? Accent : Color{159, 114, 190, 255}));
            }
        }
    }

    if (layer_ == Layer::Structure || layer_ == Layer::Objects || layer_ == Layer::Story ||
        layer_ == Layer::Triggers || layer_ == Layer::Lights || layer_ == Layer::Audio) {
        if (layer_ == Layer::Lights) {
            const Dungeon lightingDungeon{level_};
            drawAuthoredLightVisibility(level_, lightingDungeon, map, cellSize);

            for (const auto& light : level_.lights) {
                const auto* definition = findLight(light.lightId);
                if (definition == nullptr) continue;
                const int centerX = static_cast<int>(map.x + (static_cast<float>(light.x) + 0.5f) * cellSize);
                const int centerY = static_cast<int>(map.y + (static_cast<float>(light.y) + 0.5f) * cellSize);
                const float radius = definition->radius * cellSize;
                DrawCircle(centerX, centerY, radius, lightColorAlpha(light.lightId, 22));
                DrawCircleLines(centerX, centerY, radius, lightColorAlpha(light.lightId, 130));
                DrawCircleLines(centerX, centerY, radius * 0.5f, lightColorAlpha(light.lightId, 82));
            }
            if (!eraseLight_) {
                const auto* selectedLight = findLight(selectedLightId_);
                const Vector2 mouse = GetMousePosition();
                if (selectedLight != nullptr && CheckCollisionPointRec(mouse, map)) {
                    const int x = static_cast<int>((mouse.x - map.x) / cellSize);
                    const int y = static_cast<int>((mouse.y - map.y) / cellSize);
                    if (x >= 0 && y >= 0 && x < level_.width && y < level_.height) {
                        drawPreviewLightVisibility(lightingDungeon, x, y, *selectedLight, map, cellSize,
                                                   level_.width, level_.height);
                        const int centerX = static_cast<int>(map.x + (static_cast<float>(x) + 0.5f) * cellSize);
                        const int centerY = static_cast<int>(map.y + (static_cast<float>(y) + 0.5f) * cellSize);
                        DrawCircleLines(centerX, centerY, selectedLight->radius * cellSize,
                                        lightColorAlpha(selectedLight->id, 210));
                    }
                }
            }
        }
        if (cellSize >= 12.0f) {
            for (const auto& pickup : level_.pickups) {
                const char* label = pickup.type == Pickup::Type::Key ? "K" : "+";
                DrawText(label, static_cast<int>(map.x + static_cast<float>(pickup.x) * cellSize + 2.0f),
                         static_cast<int>(map.y + static_cast<float>(pickup.y) * cellSize + 1.0f), 11,
                         pickup.type == Pickup::Type::Key ? Color{245, 211, 88, 255} : Color{94, 196, 149, 255});
            }
            for (const auto& enemy : level_.enemies) {
                DrawText("X", static_cast<int>(map.x + static_cast<float>(enemy.x) * cellSize + cellSize * 0.58f),
                         static_cast<int>(map.y + static_cast<float>(enemy.y) * cellSize + cellSize * 0.46f), 11,
                         Color{214, 91, 82, 255});
            }
            for (const auto& object : level_.objects) {
                DrawText(objectMapLabel(object.kind),
                         static_cast<int>(map.x + static_cast<float>(object.x) * cellSize + cellSize * 0.32f),
                         static_cast<int>(map.y + static_cast<float>(object.y) * cellSize + cellSize * 0.18f), 11,
                         Color{190, 148, 214, 255});
            }
            for (const auto& door : level_.doors) {
                if (door.kind != DoorKind::Gate) continue;
                DrawText("G", static_cast<int>(map.x + static_cast<float>(door.x) * cellSize + 2.0f),
                         static_cast<int>(map.y + static_cast<float>(door.y) * cellSize + cellSize * 0.48f), 11,
                         Color{210, 180, 110, 255});
            }
        }
        const Rectangle spawnCell{map.x + static_cast<float>(level_.spawnX) * cellSize,
                                  map.y + static_cast<float>(level_.spawnY) * cellSize,
                                  cellSize,
                                  cellSize};
        const float spawnRadius = std::clamp(cellSize * 0.22f, 2.0f, 7.0f);
        DrawCircle(static_cast<int>(spawnCell.x + cellSize * 0.5f),
                   static_cast<int>(spawnCell.y + cellSize * 0.5f), spawnRadius, Color{90, 165, 226, 255});
        DrawCircleLines(static_cast<int>(spawnCell.x + cellSize * 0.5f),
                        static_cast<int>(spawnCell.y + cellSize * 0.5f), spawnRadius + 1.0f, Text);

        const float lightRadius = std::clamp(cellSize * 0.26f, 2.0f, 8.0f);
        for (const auto& light : level_.lights) {
            const int centerX = static_cast<int>(map.x + (static_cast<float>(light.x) + 0.5f) * cellSize);
            const int centerY = static_cast<int>(map.y + (static_cast<float>(light.y) + 0.5f) * cellSize);
            DrawCircle(centerX, centerY, lightRadius, lightColor(light.lightId));
            DrawCircleLines(centerX, centerY, lightRadius + 1.0f, Color{24, 18, 12, 255});
        }

        if (layer_ == Layer::Story) {
            for (std::size_t index = 0; index < level_.rooms.size(); ++index) {
                const auto& room = level_.rooms[index];
                const Rectangle bounds{map.x + static_cast<float>(room.x) * cellSize,
                                       map.y + static_cast<float>(room.y) * cellSize,
                                       static_cast<float>(room.width) * cellSize,
                                       static_cast<float>(room.height) * cellSize};
                const bool selected = selectedRoomIndex_ >= 0 && index == static_cast<std::size_t>(selectedRoomIndex_);
                DrawRectangleRec(bounds, Color{88, 72, 130, static_cast<unsigned char>(selected ? 74 : 38)});
                DrawRectangleLinesEx(bounds, selected ? 3.0f : 1.0f,
                                     Color{184, 142, 220, 210});
            }
            if (roomAnchorX_ >= 0) {
                const Rectangle anchor{map.x + static_cast<float>(roomAnchorX_) * cellSize,
                                       map.y + static_cast<float>(roomAnchorY_) * cellSize, cellSize, cellSize};
                DrawRectangleLinesEx(anchor, 3.0f, Accent);
            }
        }
        if (layer_ == Layer::Triggers) {
            for (const auto& trigger : level_.triggers) {
                const int centerX = static_cast<int>(map.x + (static_cast<float>(trigger.x) + 0.5f) * cellSize);
                const int centerY = static_cast<int>(map.y + (static_cast<float>(trigger.y) + 0.5f) * cellSize);
                DrawCircle(centerX, centerY, std::clamp(cellSize * 0.24f, 3.0f, 8.0f), Color{220, 108, 180, 220});
                DrawText("!", centerX - 3, centerY - 6, 11, Text);
            }
        }
    }

    DrawRectangleLinesEx(map, 2.0f, Border);

    DrawRectangleRec({32.0f, 604.0f, 512.0f, 96.0f}, Panel);
    DrawText(shortened(status_, 67).c_str(), 44, 614, 15, validationErrors_.empty() ? Text : Invalid);
    DrawText("Resize by 1 (hold Shift for 8). Randomize reshuffles surfaces.", 44, 638, 13, Muted);
    DrawText(TextFormat("%d x %d", level_.width, level_.height), 44, 666, 16, Text);
    static constexpr std::array<const char*, 4> dimensionLabels = {"W-", "W+", "H-", "H+"};
    for (int index = 0; index < static_cast<int>(dimensionLabels.size()); ++index) {
        drawButton(dimensionButton(index), dimensionLabels[static_cast<std::size_t>(index)], false, 13);
    }
    drawButton(randomizeButton(), "RANDOM", false, 12);
    drawButton(playButton(), "PLAY", validationErrors_.empty(), 13);
    drawButton(saveButton(), "SAVE", false, 13);

    DrawRectangleRec({PanelX, PanelY, PanelWidth, PanelHeight}, Panel);
    DrawRectangleLinesEx({PanelX, PanelY, PanelWidth, PanelHeight}, 1.0f, Border);
    DrawText(level_.name.c_str(), static_cast<int>(OptionX), 82, 23, Text);
    drawButton(importButton(), "IMPORT", importPanelOpen_, 13);
    drawButton(editorMenuButton(), "MENU", false, 13);

    if (layer_ == Layer::Structure) {
        DrawText("STRUCTURE BRUSH", static_cast<int>(OptionX), 112, 14, Muted);
        static constexpr std::array<const char*, 6> labels = {
            "Floor / Open", "Solid Wall", "Door", "Secret Door", "Exit", "Player Spawn",
        };
        for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
            drawButton({OptionX, 130.0f + static_cast<float>(index) * 42.0f, OptionWidth, 34.0f},
                       labels[static_cast<std::size_t>(index)],
                       static_cast<int>(structureBrush_) == index);
        }
        DrawText("K = key   + = potion   X = enemy   blue dot = spawn   warm dot = light",
                 static_cast<int>(OptionX), 400, 15, Muted);
        DrawText("Use OBJ for enemies, pickups, doors, props, notes, shrines, corpses, gates, and NPCs.",
                 static_cast<int>(OptionX), 426, 15, Muted);
    } else if (layer_ == Layer::Objects) {
        DrawText("OBJECT BRUSH", static_cast<int>(OptionX), 112, 14, Muted);
        static constexpr std::array<const char*, 13> labels = {
            "Erase", "Enemy: Prowler", "Enemy: Brute", "Pickup: Key", "Pickup: Potion",
            "Locked Door", "Unlocked Door", "Gate", "Prop", "Shrine", "Note / Inscription",
            "Corpse", "NPC",
        };
        for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
            drawButton(objectBrushButton(index), labels[static_cast<std::size_t>(index)],
                       static_cast<int>(objectBrush_) == index, 13);
        }
        if (selectedObjectIndex_ >= 0 && selectedObjectIndex_ < static_cast<int>(level_.objects.size())) {
            const auto& object = level_.objects[static_cast<std::size_t>(selectedObjectIndex_)];
            drawTextField(storyFieldBounds(4), "SELECTED OBJECT NAME", object.name, textField_ == TextField::ObjectName);
            drawTextField(storyFieldBounds(5), "INTERACTION / NOTE / DIALOGUE TEXT", object.text,
                          textField_ == TextField::ObjectText);
        }
    } else if (layer_ == Layer::Story) {
        DrawText("STORY ROOMS", static_cast<int>(OptionX), 112, 14, Muted);
        drawButton(storyModeButton(false), "DRAW ROOM", !storyErase_, 13);
        drawButton(storyModeButton(true), "ERASE ROOM", storyErase_, 13);
        drawTextField(storyFieldBounds(0), "LEVEL ID", level_.id, textField_ == TextField::LevelId);
        drawTextField(storyFieldBounds(1), "LEVEL NAME", level_.name, textField_ == TextField::LevelName);
        if (selectedRoomIndex_ >= 0 && selectedRoomIndex_ < static_cast<int>(level_.rooms.size())) {
            const auto& room = level_.rooms[static_cast<std::size_t>(selectedRoomIndex_)];
            drawTextField(storyFieldBounds(2), "ROOM NAME", room.name, textField_ == TextField::RoomName);
            drawTextField(storyFieldBounds(3), "PURPOSE", room.purpose, textField_ == TextField::RoomPurpose);
            drawTextField(storyFieldBounds(4), "MOOD", room.mood, textField_ == TextField::RoomMood);
            drawTextField(storyFieldBounds(5), "LORE NOTE", room.lore, textField_ == TextField::RoomLore);
            drawTextField(storyFieldBounds(6), "INTENDED FEELING", room.intendedFeeling,
                          textField_ == TextField::RoomFeeling);
        } else {
            DrawText("Click two map corners to create a room, then name why it exists.",
                     static_cast<int>(OptionX), 324, 14, Muted);
        }
    } else if (layer_ == Layer::Triggers) {
        DrawText("STORY EVENT", static_cast<int>(OptionX), 112, 14, Muted);
        static constexpr std::array<const char*, 6> labels = {
            "ENTER CELL", "ENTER ROOM", "OPEN DOOR", "KILL ENEMY", "PICKUP", "INTERACT",
        };
        for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
            drawButton(triggerEventButton(index), labels[static_cast<std::size_t>(index)],
                       !triggerErase_ && static_cast<int>(triggerEvent_) == index, 12);
        }
        drawButton(triggerEraseButton(), "ERASE SELECTED EVENT", triggerErase_, 13);
        if (selectedTriggerIndex_ >= 0 && selectedTriggerIndex_ < static_cast<int>(level_.triggers.size())) {
            const auto& trigger = level_.triggers[static_cast<std::size_t>(selectedTriggerIndex_)];
            drawTextField(triggerMessageBounds(), "DISCOVERY / DIALOGUE MESSAGE", trigger.message,
                          textField_ == TextField::TriggerMessage);
            DrawText(shortened("Target: " + (trigger.subjectId.empty() ? std::string{"cell"} : trigger.subjectId), 72).c_str(),
                     static_cast<int>(OptionX), 380, 14, Muted);
        } else {
            DrawText("Pick an event and click its cell or authored subject.", static_cast<int>(OptionX), 316, 14, Muted);
        }
        DrawText("Triggers currently display text once during the playtest.", static_cast<int>(OptionX), 414, 14, Muted);
    } else if (layer_ == Layer::Lights) {
        DrawText("LIGHT BRUSH", static_cast<int>(OptionX), 112, 14, Muted);
        drawButton(eraseLightButton(), "Erase Light", eraseLight_);
        const auto buttons = lightButtons(FirstLightY);
        for (const auto& button : buttons) {
            if (button.startsCategory) {
                DrawText(button.light->category.c_str(), static_cast<int>(OptionX),
                         static_cast<int>(button.categoryY), 14, Muted);
            }
            drawButton(button.bounds, button.light->name.c_str(),
                       !eraseLight_ && selectedLightId_ == button.light->id);
            DrawText(TextFormat("R %.1f", button.light->radius),
                     static_cast<int>(button.bounds.x + button.bounds.width - 58.0f),
                     static_cast<int>(button.bounds.y + 10.0f), 11,
                     !eraseLight_ && selectedLightId_ == button.light->id ? Background : Muted);
        }
        const int summaryY = static_cast<int>(buttons.empty() ? FirstLightY : buttons.back().bounds.y + 50.0f);
        DrawText(TextFormat("%d light(s) placed", static_cast<int>(level_.lights.size())),
                 static_cast<int>(OptionX), summaryY, 15, Text);
        DrawText("Click a cell to place, click it again to swap the type.",
                 static_cast<int>(OptionX), summaryY + 24, 15, Muted);
        DrawText("Sconces belong in wall cells; braziers read best on open floor.",
                 static_cast<int>(OptionX), summaryY + 48, 15, Muted);
        DrawText("Warm overlay = reached light; red X cells are blocked by occlusion.",
                 static_cast<int>(OptionX), summaryY + 72, 15, Muted);
        DrawText("Radius rings are only range; visibility uses the runtime grid trace.",
                 static_cast<int>(OptionX), summaryY + 96, 15, Muted);
    } else if (layer_ == Layer::Audio) {
        DrawText("LEVEL AUDIO", static_cast<int>(OptionX), 112, 14, Muted);
        drawButton(noMusicButton(), "No Music", level_.musicPath.empty());
        const auto buttons = musicTrackButtons(FirstMusicY);
        for (const auto& button : buttons) {
            drawButton(button.bounds, button.label.c_str(), level_.musicPath == button.path);
            DrawText(shortened(button.path, 48).c_str(),
                     static_cast<int>(button.bounds.x + 14.0f),
                     static_cast<int>(button.bounds.y + 36.0f), 11, Muted);
        }
        const int summaryY = static_cast<int>(buttons.empty() ? FirstMusicY : buttons.back().bounds.y + 64.0f);
        const std::string currentMusic = level_.musicPath.empty()
            ? std::string{"Current: silent"}
            : shortened("Current: " + level_.musicPath, 72);
        DrawText(currentMusic.c_str(), static_cast<int>(OptionX), summaryY, 15, Text);
        DrawText("Drag audio files onto this window to import them into content/audio/music.",
                 static_cast<int>(OptionX), summaryY + 28, 14, Muted);
        DrawText("Supported: .wav, .ogg, .mp3, .flac. The selected track loops in Play.",
                 static_cast<int>(OptionX), summaryY + 50, 14, Muted);
        DrawText("Later events can swap this path for combat, boss, or sanctuary music.",
                 static_cast<int>(OptionX), summaryY + 72, 14, Muted);
    } else {
        const char* heading = layer_ == Layer::Wall ? "WALL MATERIAL" :
            (layer_ == Layer::Floor ? "FLOOR MATERIAL" : "CEILING MATERIAL / MODE");
        DrawText(heading, static_cast<int>(OptionX), 112, 14, Muted);

        float firstMaterialY = 130.0f;
        if (layer_ == Layer::Ceiling) {
            DrawText("ATMOSPHERE", static_cast<int>(OptionX), 132, 14, Muted);
            drawButton({OptionX, 154.0f, OptionWidth, 34.0f}, "Sky", selectedCeilingSky_);
            firstMaterialY = 211.0f;
        }
        const auto buttons = materialButtons(selectedSurface(), firstMaterialY);
        for (const auto& button : buttons) {
            if (button.startsCategory) {
                DrawText(button.material->category.c_str(), static_cast<int>(OptionX),
                         static_cast<int>(button.categoryY), 14, Muted);
            }
            const bool selected = !selectedCeilingSky_ && selectedMaterial() == button.material->id;
            drawButton(button.bounds, button.material->name.c_str(), selected);
            const bool textured = !button.material->texturePath.empty();
            DrawText(textured ? "TEX" : "NO TEX",
                     static_cast<int>(button.bounds.x + button.bounds.width - (textured ? 38.0f : 58.0f)),
                     static_cast<int>(button.bounds.y + 10.0f), 11, textured ? Valid : Invalid);
        }
        const float defaultY = buttons.empty() ? firstMaterialY : buttons.back().bounds.y + 50.0f;
        drawButton({OptionX, defaultY, OptionWidth, 36.0f}, "MAKE SELECTION LEVEL DEFAULT", false, 15);
    }

    const int validationY = 548;
    DrawText(validationErrors_.empty() ? "VALIDATION: READY" : "VALIDATION ISSUES", static_cast<int>(OptionX),
             validationY, 15, validationErrors_.empty() ? Valid : Invalid);
    const std::size_t visibleErrors = std::min<std::size_t>(validationErrors_.size(), 4);
    for (std::size_t index = 0; index < visibleErrors; ++index) {
        DrawText(shortened(validationErrors_[index], 78).c_str(), static_cast<int>(OptionX),
                 validationY + 27 + static_cast<int>(index) * 22, 14, Invalid);
    }
    DrawText(shortened(levelPath_, 82).c_str(), static_cast<int>(OptionX), 678, 12, Muted);

    if (importPanelOpen_) {
        const Rectangle panel = importPanelBounds();
        DrawRectangleRec({0.0f, 0.0f, 1280.0f, 720.0f}, Color{4, 5, 7, 172});
        DrawRectangleRec(panel, Color{23, 27, 34, 255});
        DrawRectangleLinesEx(panel, 2.0f, Accent);
        DrawText("IMPORT STONEVEIL BLUEPRINT", static_cast<int>(panel.x) + 28,
                 static_cast<int>(panel.y) + 28, 24, Text);
        DrawText("1. Ask Codex for a STONEVEIL_BLUEPRINT block.", static_cast<int>(panel.x) + 32,
                 static_cast<int>(panel.y) + 82, 17, Muted);
        DrawText("2. Copy the entire block, including STONEVEIL_BLUEPRINT 1 and END.",
                 static_cast<int>(panel.x) + 32, static_cast<int>(panel.y) + 110, 17, Muted);
        DrawText("3. Click Paste + Import. The result becomes the editable draft.",
                 static_cast<int>(panel.x) + 32, static_cast<int>(panel.y) + 138, 17, Muted);

        DrawRectangleRec({panel.x + 32.0f, panel.y + 182.0f, panel.width - 64.0f, 212.0f},
                         Color{15, 17, 22, 255});
        DrawRectangleLinesEx({panel.x + 32.0f, panel.y + 182.0f, panel.width - 64.0f, 212.0f},
                             1.0f, Border);
        DrawText("Example:", static_cast<int>(panel.x) + 50, static_cast<int>(panel.y) + 200, 15, Accent);
        DrawText("STONEVEIL_BLUEPRINT 1", static_cast<int>(panel.x) + 50, static_cast<int>(panel.y) + 228, 14, Text);
        DrawText("NAME \"THE LOW MEAD HALL\"", static_cast<int>(panel.x) + 50, static_cast<int>(panel.y) + 250, 14, Text);
        DrawText("SIZE 26 18", static_cast<int>(panel.x) + 50, static_cast<int>(panel.y) + 272, 14, Text);
        DrawText("ROOM 2 2 22 12 floor", static_cast<int>(panel.x) + 50, static_cast<int>(panel.y) + 294, 14, Text);
        DrawText("SPAWN 13 15 NORTH    EXIT 13 2", static_cast<int>(panel.x) + 50,
                 static_cast<int>(panel.y) + 316, 14, Text);
        DrawText("LIGHT brazier 13 7    WATER 5 8 3 1 2 EAST 4", static_cast<int>(panel.x) + 50,
                 static_cast<int>(panel.y) + 338, 14, Text);
        DrawText("ENEMY 8 6 18    END", static_cast<int>(panel.x) + 50,
                 static_cast<int>(panel.y) + 360, 14, Text);

        DrawText(shortened(importStatus_, 88).c_str(), static_cast<int>(panel.x) + 32,
                 static_cast<int>(panel.y) + 424, 15,
                 importStatus_.find("failed") != std::string::npos ||
                         importStatus_.find("Blueprint line") != std::string::npos ||
                         importStatus_.find("invalid") != std::string::npos
                     ? Invalid
                     : Text);
        drawButton(importPasteButton(), "PASTE + IMPORT", false, 15);
        drawButton(importCancelButton(), "CANCEL", false, 15);
        DrawText("Shortcut: Ctrl+V imports directly from this panel. Esc cancels.",
                 static_cast<int>(panel.x) + 32, static_cast<int>(panel.y) + 410, 13, Muted);
    }

    if (pendingAction_ != PendingAction::None) {
        DrawRectangleRec({0.0f, 0.0f, 1280.0f, 720.0f}, Color{4, 5, 7, 190});
        const Rectangle panel{330.0f, 250.0f, 620.0f, 190.0f};
        DrawRectangleRec(panel, Color{23, 27, 34, 255});
        DrawRectangleLinesEx(panel, 2.0f, Invalid);
        DrawText("UNSAVED CHANGES", 510, 280, 25, Text);
        DrawText("S  save and continue     D  discard     ESC  cancel", 408, 344, 18, Muted);
        DrawText("Your draft stays intact until you choose.", 462, 386, 16, Accent);
    }
}

} // namespace sv
