#pragma once

#include "Dungeon.hpp"
#include "LevelDocument.hpp"
#include "Project.hpp"
#include "LevelEditing.hpp"

#include <random>
#include <string>
#include <vector>

namespace sv {

class LevelEditor {
public:
    explicit LevelEditor(std::string levelPath, const std::string& projectFile = {});

    void update();
    void draw() const;
    const LevelDefinition& level() const { return level_; }
    void showLightsLayerForCapture();
    void showStoryLayerForCapture();
    void showObjectsLayerForCapture();
    void showProjectPanelForCapture(bool visible) { projectPanelOpen_ = visible; }
    void showInspectorForCapture() { selection_ = {}; inspectorOpen_ = true; }
    void closeInspectorForCapture() { inspectorOpen_ = movingSelection_ = false; }
    void showCharacterCreatorForCapture() { projectPanelOpen_ = true; openCharacterCreator(); }
    void closeCharacterCreatorForCapture() { characterCreatorOpen_ = false; }
    bool consumePlaytestRequest();
    bool consumeExitRequest();
    bool hasUnsavedChanges() const { return document_.dirty(); }
    void requestQuit();
    bool consumeQuitRequest();
    const ProjectDocument& project() const { return project_; }
    const std::string& levelPath() const { return document_.path(); }

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
        Recruit,
        PartyManagement,
        ArrivalPoint,
        LevelTransition,
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

    enum class PendingAction { None, Exit, Reload, NewLevel, Quit, NewProject, OpenProject, OpenLevel, OpenRegistered };
    enum class CharacterTab { Identity, Role, Recruitment, Advanced };
    enum class CharacterField { None, Name, Role, Summary, Traits, EquipmentTags, StartingEquipment, RecruitmentText };

    void loadLevel(const std::string& path = {});
    void saveLevel();
    void saveLevelAs();
    void newLevel();
    void requestDestructiveAction(PendingAction action);
    void completePendingAction();
    void projectAction(PendingAction action);
    void updateProjectPanel();
    void drawProjectPanel() const;
    void openCharacterCreator();
    void updateCharacterCreator();
    void drawCharacterCreator() const;
    std::string* activeCharacterText();
    void saveCharacterCatalog();
    void registerSavedLevel();
    void inspectCell(int x, int y);
    void updateInspector();
    void drawInspector() const;
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
    bool selectNextTransitionDestination(WorldObject& object);

    bool hasObjectAt(int x, int y) const;
    bool hasLightAt(int x, int y) const;
    const std::string& selectedMaterial() const;
    std::string& selectedMaterial();
    SurfaceKind selectedSurface() const;

    LevelDocument document_;
    ProjectDocument project_;
    LevelSelection selection_;
    bool inspectorOpen_{false};
    bool movingSelection_{false};
    bool projectPanelOpen_{false};
    bool characterCreatorOpen_{false};
    bool characterCatalogDirty_{false};
    std::vector<CharacterDefinition> characterDraft_;
    int selectedCharacter_{0};
    int characterScroll_{0};
    CharacterTab characterTab_{CharacterTab::Identity};
    CharacterField characterField_{CharacterField::None};
    int projectScroll_{0};
    int requestedProjectLevel_{0};
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
