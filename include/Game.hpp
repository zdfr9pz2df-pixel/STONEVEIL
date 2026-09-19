#pragma once

#include "ActionGate.hpp"
#include "AudioSystem.hpp"
#include "Campaign.hpp"
#include "Character.hpp"
#include "CombatSystem.hpp"
#include "Dungeon.hpp"
#include "LevelEditor.hpp"
#include "Party.hpp"
#include "Player.hpp"
#include "Raycaster.hpp"
#include "Roster.hpp"
#include "raylib.h"

#include <string>
#include <deque>
#include <memory>
#include <vector>

namespace sv {

class Game {
public:
    Game();
    explicit Game(std::string levelPathOverride);
    void run();
    bool captureUiSnapshots(const std::string& outputDirectory);

private:
    enum class Mode { Title, NewGame, Playing, Victory, Defeat, Editor };

    void resetWorld();
    void resetWorld(const LevelDefinition& level);
    void prepareNewGame();
    void beginNewGame();
    void openEditor();
    void beginEditorPlaytest();
    void toggleStarter(CharacterId id);
    void debugSetPartySize(int size);
    void selectCampaignLevel(int delta);
    void requestQuit();
    bool fireStoryTrigger(TriggerEvent event, int x, int y, const std::string& subjectId = {});
    EventFireResult fireEvent(const EventContext& context);
    void update(float dt);
    void updateNewGame();
    void updatePlaying(float dt);
    void draw() const;
    void drawWorld() const;
    void drawHud() const;
    void drawTitle() const;
    void drawNewGame() const;
    void drawEndScreen(bool won) const;

    void move(int forward, int strafe);
    void turn(int delta);
    void interact();
    void attack();
    void drinkPotion();
    void collectPickup();
    void enterMode(Mode mode);
    bool save() const;
    bool load();
    void setMessage(std::string message, float seconds = 2.0f);

    Dungeon dungeon_;
    PlayerState player_{};
    Raycaster raycaster_{};
    Mode mode_{Mode::Title};
    Roster roster_{};
    Party party_{};
    CombatSystem combat_{};
    ActionGate gate_{};
    AudioSystem audio_{};
    std::unique_ptr<LevelEditor> editor_;
    CampaignDefinition campaign_{};
    int campaignLevelIndex_{0};
    std::string levelPath_;
    std::string levelMusicPath_;
    bool editorPlaytest_{false};
    bool quitRequested_{false};
    std::vector<CharacterId> selectedStarters_{};
    int starterCursor_{0};
    int keys_{0};
    int potions_{1};
    int xp_{0};
    mutable std::string message_;
    float messageTimer_{0.0f};
    EventRuntime events_{};
    std::deque<std::string> storyMessages_;
    float storyMessageTimer_{0.0f};
    std::string currentRoomId_;
};

} // namespace sv
