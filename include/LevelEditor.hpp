#pragma once

#include "Dungeon.hpp"
#include "LevelDocument.hpp"

#include <random>
#include <string>
#include <vector>

namespace sv {

class LevelEditor {
public:
    explicit LevelEditor(std::string levelPath);

    void update();
    void draw() const;
    const LevelDefinition& level() const { return level_; }
    void showLightsLayerForCapture();
    void showStoryLayerForCapture();
    bool consumePlaytestRequest();
    bool consumeExitRequest();
    bool hasUnsavedChanges() const { return document_.dirty(); }
    void requestQuit();
    bool consumeQuitRequest();

private:
    enum class Layer {
        Structure,
        Wall,
        Floor,
        Ceiling,
        Objects,
        Story,
        Triggers,
        Lights,
        Audio,
    };
    static constexpr int LayerCount = 9;

    enum class StructureBrush {
        Floor,
        Wall,
        Door,
        SecretDoor,
        Exit,
        Spawn,
    };

    enum class ObjectBrush {
        Erase,
        EnemyProwler,
        EnemyBrute,
        Key,
        Potion,
        LockedDoor,
        UnlockedDoor,
        Gate,
        Prop,
        Shrine,
        Note,
        Corpse,
        Npc,
    };

    enum class TextField {
        None,
        LevelId,
        LevelName,
        ObjectName,
        ObjectText,
        RoomName,
        RoomPurpose,
        RoomMood,
        RoomLore,
        RoomFeeling,
        TriggerMessage,
    };

    enum class PendingAction { None, Exit, Reload, NewLevel, Quit };

    void loadLevel(const std::string& path = {});
    void saveLevel();
    void saveLevelAs();
    void newLevel();
    void requestDestructiveAction(PendingAction action);
    void completePendingAction();
    void refreshValidation(const std::string& successMessage);
    void syncSelectionsFromLevel();
    void setLayer(Layer layer);
    void paintCell(int x, int y);
    void paintStructure(int x, int y);
    void paintSurface(int x, int y, SurfaceKind surface);
    void paintLight(int x, int y);
    void paintObject(int x, int y);
    void paintStory(int x, int y);
    void paintTrigger(int x, int y);
    void makeSelectionDefault();
    void randomizeSurfaces();
    void openImportPanel();
    void importBlueprintFromClipboard();
    void importDroppedAudio();
    void resizeLevel(int widthDelta, int heightDelta);
    void requestPlaytest();
    void recordUndo();
    void undo();
    void redo();
    void beginTextEdit(TextField field);
    void updateTextEdit();
    std::string* activeText();
    void finishTextEdit();

    bool hasObjectAt(int x, int y) const;
    bool hasLightAt(int x, int y) const;
    const std::string& selectedMaterial() const;
    std::string& selectedMaterial();
    SurfaceKind selectedSurface() const;

    LevelDocument document_;
    std::string levelDirectory_;
    // Presentation aliases keep painting incremental; all identity/history
    // operations belong to the testable document owner.
    const std::string& levelPath_{document_.path()};
    LevelDefinition& level_{document_.draft()};
    Layer layer_{Layer::Structure};
    StructureBrush structureBrush_{StructureBrush::Floor};
    ObjectBrush objectBrush_{ObjectBrush::Erase};
    TriggerEvent triggerEvent_{TriggerEvent::EnterCell};
    std::string selectedWallMaterial_;
    std::string selectedFloorMaterial_;
    std::string selectedCeilingMaterial_;
    bool selectedCeilingSky_{false};
    std::string selectedLightId_{defaultLightId()};
    bool eraseLight_{false};
    bool importPanelOpen_{false};
    bool storyErase_{false};
    bool triggerErase_{false};
    int roomAnchorX_{-1};
    int roomAnchorY_{-1};
    int selectedObjectIndex_{-1};
    int selectedRoomIndex_{-1};
    int selectedTriggerIndex_{-1};
    TextField textField_{TextField::None};
    PendingAction pendingAction_{PendingAction::None};
    std::string importStatus_;
    int lastPaintX_{-1};
    int lastPaintY_{-1};
    std::mt19937 randomizer_;
    bool playtestRequested_{false};
    bool exitRequested_{false};
    bool quitRequested_{false};
    std::string status_;
    std::vector<std::string> validationErrors_;
};

} // namespace sv
