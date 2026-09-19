#include "Game.hpp"
#include "CombatTuning.hpp"
#include "EnemyType.hpp"
#include "LevelIO.hpp"
#include "SaveSystem.hpp"
#include "WorldEvents.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>

#ifndef STONEVEIL_BUILD_LABEL
#define STONEVEIL_BUILD_LABEL "STONEVEIL v0.3"
#endif

#ifndef STONEVEIL_SOURCE_DIR
#define STONEVEIL_SOURCE_DIR ""
#endif

namespace sv {
namespace {
constexpr int ScreenW = 1280;
constexpr int ScreenH = 720;
constexpr int ViewW = 930;
constexpr int ViewH = 560;
constexpr int ViewY = 36;

constexpr int StarterCardW = 330;
constexpr int StarterCardH = 360;
constexpr int StarterCardGap = 28;
constexpr int StarterCardY = 190;
constexpr int StarterCardX = (ScreenW - StarterCardW * 3 - StarterCardGap * 2) / 2;
constexpr int TitleButtonW = 360;
constexpr int TitleButtonH = 48;
constexpr int TitleButtonGap = 12;
constexpr int TitleButtonX = (ScreenW - TitleButtonW) / 2;
constexpr int TitleButtonY = 330;

Rectangle starterCardRectangle(int index) {
    return Rectangle{
        static_cast<float>(StarterCardX + index * (StarterCardW + StarterCardGap)),
        static_cast<float>(StarterCardY),
        static_cast<float>(StarterCardW),
        static_cast<float>(StarterCardH),
    };
}

Rectangle titleButtonRectangle(int index) {
    return {
        static_cast<float>(TitleButtonX),
        static_cast<float>(TitleButtonY + index * (TitleButtonH + TitleButtonGap)),
        static_cast<float>(TitleButtonW),
        static_cast<float>(TitleButtonH),
    };
}

Rectangle campaignPreviousButton() { return {405.0f, 286.0f, 44.0f, 30.0f}; }
Rectangle campaignNextButton() { return {831.0f, 286.0f, 44.0f, 30.0f}; }

std::string resolveContentPath(const std::string& relativePath) {
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(std::filesystem::path{GetApplicationDirectory()} / relativePath);
    std::error_code error;
    const auto workingDirectory = std::filesystem::current_path(error);
    if (!error) candidates.emplace_back(workingDirectory / relativePath);
    if (std::string{STONEVEIL_SOURCE_DIR}.size() > 0) {
        candidates.emplace_back(std::filesystem::path{STONEVEIL_SOURCE_DIR} / relativePath);
    }

    for (const auto& path : candidates) {
        if (std::filesystem::exists(path, error)) return path.string();
        error.clear();
    }

    TraceLog(LOG_WARNING, "STONEVEIL: missing content file '%s'", relativePath.c_str());
    for (const auto& path : candidates) {
        const std::string nativePath = path.string();
        TraceLog(LOG_WARNING, "STONEVEIL: searched '%s'", nativePath.c_str());
    }
    return candidates.empty() ? relativePath : candidates.front().string();
}
}

Game::Game() : Game(std::string{}) {}

Game::Game(std::string levelPathOverride) : Game(std::move(levelPathOverride), {}, false) {}

Game::Game(std::string levelPathOverride, std::string projectFile, bool runtimeOnly)
    : projectFile_(std::move(projectFile)), runtimeOnly_(runtimeOnly) {
    InitWindow(ScreenW, ScreenH, STONEVEIL_BUILD_LABEL);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    audio_.initialize();
    if (!projectFile_.empty()) {
        ProjectDocument project;
        std::string error;
        if (project.open(projectFile_, error)) {
            campaign_ = project.campaign();
            if (!roster_.setDefinitions(project.characters())) quitRequested_ = true;
            contentRoot_ = project.root();
            levelPath_ = project.levelPath(campaign_.startingLevelId);
            for (std::size_t i = 0; i < campaign_.levels.size(); ++i)
                if (campaign_.levels[i].id == campaign_.startingLevelId) campaignLevelIndex_ = static_cast<int>(i);
            raycaster_.setContentRoot(contentRoot_);
            audio_.setContentRoot(contentRoot_);
        } else {
            TraceLog(LOG_ERROR, "STONEVEIL: project failed to open: %s", error.c_str());
            quitRequested_ = true;
        }
    } else if (levelPathOverride.empty()) {
        std::string campaignError;
        if (CampaignIO::load(resolveContentPath("content/campaigns/stoneveil.campaign"), campaign_, campaignError) &&
            !campaign_.levels.empty()) {
            const auto start = std::find_if(campaign_.levels.begin(), campaign_.levels.end(), [this](const auto& entry) {
                return entry.id == campaign_.startingLevelId;
            });
            campaignLevelIndex_ = start == campaign_.levels.end()
                ? 0
                : static_cast<int>(std::distance(campaign_.levels.begin(), start));
            levelPath_ = resolveContentPath(campaign_.levels[static_cast<std::size_t>(campaignLevelIndex_)].path);
        } else {
            levelPath_ = resolveContentPath("content/levels/gatehouse.svl");
        }
    } else {
        levelPath_ = std::move(levelPathOverride);
    }
    editor_ = std::make_unique<LevelEditor>(levelPath_, projectFile_);
    resetWorld();
    roster_.reset();
    party_.clear();
    mode_ = Mode::Title;
}

void Game::resetWorld() {
    LevelDefinition externalLevel;
    std::string levelError;
    if (LevelIO::load(levelPath_, externalLevel, levelError)) resetWorld(externalLevel);
    else if (!projectFile_.empty()) {
        TraceLog(LOG_ERROR, "STONEVEIL: project level failed to load: %s", levelError.c_str());
        quitRequested_ = true;
    } else resetWorld(levelOneDefinition());
}

void Game::resetWorld(const LevelDefinition& level) {
    dungeon_ = Dungeon{level};
    levelMusicPath_ = dungeon_.musicPath();
    player_ = PlayerState{dungeon_.spawnX(), dungeon_.spawnY(), dungeon_.spawnDirection()};
    keys_ = 0;
    potions_ = 1;
    xp_ = 0;
    combat_.reset();
    message_.clear();
    messageTimer_ = 0.0f;
    if (!configureWorldEvents(dungeon_, events_))
        TraceLog(LOG_ERROR, "STONEVEIL: duplicate or invalid compiled event identity");
    storyMessages_.clear();
    storyMessageTimer_ = 0.0f;
    currentRoomId_.clear();
}

void Game::selectCampaignLevel(int delta) {
    if (runtimeOnly_) return;
    if (campaign_.levels.empty()) return;
    if (editor_ && editor_->hasUnsavedChanges()) {
        openEditor();
        return;
    }
    const int count = static_cast<int>(campaign_.levels.size());
    campaignLevelIndex_ = (campaignLevelIndex_ + delta + count) % count;
    const auto& relative = campaign_.levels[static_cast<std::size_t>(campaignLevelIndex_)].path;
    levelPath_ = contentRoot_.empty() ? resolveContentPath(relative) : (std::filesystem::path{contentRoot_} / relative).string();
    editor_ = std::make_unique<LevelEditor>(levelPath_, projectFile_);
    campaignState_.clear();
    resetWorld();
    audio_.play(AudioCue::Turn);
}

void Game::prepareNewGame() {
    const auto starters = roster_.starterIds();
    selectedStarters_.clear();
    if (!starters.empty()) selectedStarters_.push_back(starters.front());
    starterCursor_ = 0;
    enterMode(Mode::NewGame);
}

void Game::beginNewGame() {
    if (selectedStarters_.empty() || selectedStarters_.size() > Party::InitialCapacity) return;
    resetWorld();
    campaignState_.clear();
    editorPlaytest_ = false;
    party_.reset();
    if (!roster_.beginNewGame(selectedStarters_) || !party_.setMembers(selectedStarters_)) {
        enterMode(Mode::Title);
        return;
    }
    setMessage("The chosen enter " + dungeon_.name() + ".", 3.0f);
    audio_.play(AudioCue::UiConfirm);
    enterMode(Mode::Playing);
}

void Game::openEditor() {
    if (runtimeOnly_) return;
    editorPlaytest_ = false;
    enterMode(Mode::Editor);
}

void Game::beginEditorPlaytest() {
    if (editor_ == nullptr) return;
    adoptEditorProject();
    const auto starters = roster_.starterIds();
    if (starters.empty()) return;
    resetWorld(editor_->level());
    campaignState_.clear();
    party_.reset();
    if (!roster_.beginNewGame(starters) || !party_.setMembers(starters)) {
        enterMode(Mode::Editor);
        return;
    }
    editorPlaytest_ = true;
    setMessage("EDITOR PLAYTEST - ESC returns to the editor.", 4.0f);
    audio_.play(AudioCue::UiConfirm);
    enterMode(Mode::Playing);
}

void Game::adoptEditorProject() {
    if (!editor_) return;
    if (editor_->project().isOpen()) {
        projectFile_ = editor_->project().path();
        contentRoot_ = editor_->project().root();
        campaign_ = editor_->project().campaign();
        roster_.setDefinitions(editor_->project().characters());
        campaignLevelIndex_ = 0;
        for (std::size_t i = 0; i < campaign_.levels.size(); ++i)
            if (campaign_.levels[i].id == editor_->level().id) campaignLevelIndex_ = static_cast<int>(i);
        raycaster_.setContentRoot(contentRoot_);
        audio_.setContentRoot(contentRoot_);
    }
    if (!editor_->levelPath().empty()) levelPath_ = editor_->levelPath();
}

void Game::toggleStarter(CharacterId id) {
    const auto it = std::find(selectedStarters_.begin(), selectedStarters_.end(), id);
    if (it != selectedStarters_.end()) {
        selectedStarters_.erase(it);
    } else if (selectedStarters_.size() < Party::InitialCapacity) {
        selectedStarters_.push_back(id);
    }
}

void Game::debugSetPartySize(int size) {
    const auto starters = roster_.starterIds();
    if (starters.empty()) return;
    const int maximum = static_cast<int>(std::min<std::size_t>(starters.size(), Party::InitialCapacity));
    const int clamped = std::clamp(size, 1, maximum);
    const std::vector<CharacterId> chosen(starters.begin(), starters.begin() + clamped);

    roster_.reset();
    party_.reset();
    if (!roster_.beginNewGame(chosen) || !party_.setMembers(chosen)) return;
    combat_.reset();
    gate_.reset();
    setMessage("DEBUG: party size " + std::to_string(clamped) + ".", 2.0f);
}

void Game::run() {
    while (!quitRequested_) {
        if (WindowShouldClose()) requestQuit();
        if (quitRequested_) break;
        update(GetFrameTime());
        audio_.update();
        draw();
    }
    audio_.shutdown();
    CloseWindow();
}

void Game::requestQuit() {
    if (editor_ && editor_->hasUnsavedChanges()) {
        enterMode(Mode::Editor);
        editor_->requestQuit();
    } else quitRequested_ = true;
}

bool Game::captureUiSnapshots(const std::string& outputDirectory) {
    std::error_code error;
    const std::filesystem::path originalDirectory = std::filesystem::current_path(error);
    if (error) {
        audio_.shutdown();
        CloseWindow();
        return false;
    }
    const std::filesystem::path directory = std::filesystem::absolute(outputDirectory, error);
    if (error) {
        audio_.shutdown();
        CloseWindow();
        return false;
    }
    std::filesystem::create_directories(directory, error);
    if (error) {
        audio_.shutdown();
        CloseWindow();
        return false;
    }

    const auto titlePath = directory / "title-menu.png";
    const auto editorPath = directory / "dungeon-editor.png";
    const auto objectEditorPath = directory / "object-editor.png";
    const auto lightDebugPath = directory / "lighting-debug.png";
    const auto storyEditorPath = directory / "story-editor.png";
    const auto gameplayPath = directory / "gameplay-lighting.png";
    const auto partyManagementPath = directory / "party-management.png";
    const auto rawWarmupPath = originalDirectory / "stoneveil-capture-warmup.png";
    const auto rawTitlePath = originalDirectory / "stoneveil-title-menu.png";
    const auto rawEditorPath = originalDirectory / "stoneveil-dungeon-editor.png";
    const auto rawObjectEditorPath = originalDirectory / "stoneveil-object-editor.png";
    const auto rawLightDebugPath = originalDirectory / "stoneveil-lighting-debug.png";
    const auto rawStoryEditorPath = originalDirectory / "stoneveil-story-editor.png";
    const auto rawGameplayPath = originalDirectory / "stoneveil-gameplay-lighting.png";
    const auto rawPartyManagementPath = originalDirectory / "stoneveil-party-management.png";

    std::filesystem::remove(titlePath, error);
    std::filesystem::remove(editorPath, error);
    std::filesystem::remove(objectEditorPath, error);
    std::filesystem::remove(lightDebugPath, error);
    std::filesystem::remove(storyEditorPath, error);
    std::filesystem::remove(gameplayPath, error);
    std::filesystem::remove(partyManagementPath, error);
    std::filesystem::remove(rawWarmupPath, error);
    std::filesystem::remove(rawTitlePath, error);
    std::filesystem::remove(rawEditorPath, error);
    std::filesystem::remove(rawObjectEditorPath, error);
    std::filesystem::remove(rawLightDebugPath, error);
    std::filesystem::remove(rawStoryEditorPath, error);
    std::filesystem::remove(rawGameplayPath, error);
    std::filesystem::remove(rawPartyManagementPath, error);
    error.clear();

    draw();
    draw();
    TakeScreenshot("stoneveil-capture-warmup.png");
    openEditor();
    draw();
    draw();
    TakeScreenshot("stoneveil-dungeon-editor.png");
    if (editor_ != nullptr) editor_->showObjectsLayerForCapture();
    draw();
    draw();
    TakeScreenshot("stoneveil-object-editor.png");
    if (!runtimeOnly_ && editor_ != nullptr) {
        editor_->showProjectPanelForCapture(true);
        draw();
        draw();
        TakeScreenshot("stoneveil-project-panel.png");
        std::filesystem::copy_file(originalDirectory / "stoneveil-project-panel.png", directory / "project-panel.png",
                                  std::filesystem::copy_options::overwrite_existing, error);
        std::filesystem::remove(originalDirectory / "stoneveil-project-panel.png", error);
        editor_->showProjectPanelForCapture(false);
        editor_->showInspectorForCapture();
        draw();
        draw();
        TakeScreenshot("stoneveil-object-inspector.png");
        std::filesystem::copy_file(originalDirectory / "stoneveil-object-inspector.png", directory / "object-inspector.png",
                                  std::filesystem::copy_options::overwrite_existing, error);
        std::filesystem::remove(originalDirectory / "stoneveil-object-inspector.png", error);
        editor_->closeInspectorForCapture();
        editor_->showCharacterCreatorForCapture();
        draw();
        draw();
        TakeScreenshot("stoneveil-character-creator.png");
        std::filesystem::copy_file(originalDirectory / "stoneveil-character-creator.png", directory / "character-creator.png",
                                  std::filesystem::copy_options::overwrite_existing, error);
        std::filesystem::remove(originalDirectory / "stoneveil-character-creator.png", error);
        editor_->closeCharacterCreatorForCapture();
    }
    if (editor_ != nullptr) editor_->showLightsLayerForCapture();
    draw();
    draw();
    TakeScreenshot("stoneveil-lighting-debug.png");
    if (editor_ != nullptr) editor_->showStoryLayerForCapture();
    draw();
    draw();
    TakeScreenshot("stoneveil-story-editor.png");
    if (runtimeOnly_) {
        prepareNewGame();
        beginNewGame();
    } else beginEditorPlaytest();
    draw();
    draw();
    TakeScreenshot("stoneveil-gameplay-lighting.png");
    enterMode(Mode::PartyManagement);
    draw();
    draw();
    TakeScreenshot("stoneveil-party-management.png");
    enterMode(Mode::Title);
    draw();
    draw();
    TakeScreenshot("stoneveil-title-menu.png");

    std::filesystem::copy_file(rawTitlePath, titlePath,
                               std::filesystem::copy_options::overwrite_existing, error);
    if (!error) {
        std::filesystem::copy_file(rawEditorPath, editorPath,
                                   std::filesystem::copy_options::overwrite_existing, error);
    }
    if (!error) {
        std::filesystem::copy_file(rawObjectEditorPath, objectEditorPath,
                                   std::filesystem::copy_options::overwrite_existing, error);
    }
    if (!error) {
        std::filesystem::copy_file(rawLightDebugPath, lightDebugPath,
                                   std::filesystem::copy_options::overwrite_existing, error);
    }
    if (!error) {
        std::filesystem::copy_file(rawStoryEditorPath, storyEditorPath,
                                   std::filesystem::copy_options::overwrite_existing, error);
    }
    if (!error) {
        std::filesystem::copy_file(rawGameplayPath, gameplayPath,
                                   std::filesystem::copy_options::overwrite_existing, error);
    }
    if (!error) {
        std::filesystem::copy_file(rawPartyManagementPath, partyManagementPath,
                                   std::filesystem::copy_options::overwrite_existing, error);
    }
    const bool captured = !error && std::filesystem::exists(titlePath) &&
        std::filesystem::exists(editorPath) && std::filesystem::exists(objectEditorPath) &&
        std::filesystem::exists(lightDebugPath) &&
        std::filesystem::exists(storyEditorPath) &&
        std::filesystem::exists(gameplayPath) && std::filesystem::exists(partyManagementPath);
    std::filesystem::remove(rawWarmupPath, error);
    std::filesystem::remove(rawTitlePath, error);
    std::filesystem::remove(rawEditorPath, error);
    std::filesystem::remove(rawObjectEditorPath, error);
    std::filesystem::remove(rawLightDebugPath, error);
    std::filesystem::remove(rawStoryEditorPath, error);
    std::filesystem::remove(rawGameplayPath, error);
    std::filesystem::remove(rawPartyManagementPath, error);
    audio_.shutdown();
    CloseWindow();
    return captured;
}

void Game::update(float dt) {
    if (mode_ == Mode::Playing && !storyMessages_.empty()) {
        storyMessageTimer_ -= dt;
        if (storyMessageTimer_ <= 0.0f) {
            storyMessages_.pop_front();
            storyMessageTimer_ = 4.0f;
        }
    }
    if (messageTimer_ > 0.0f) messageTimer_ -= dt;
    if (IsKeyPressed(KEY_M)) {
        audio_.toggleMuted();
        setMessage(audio_.muted() ? "Audio muted." : "Audio enabled.", 1.4f);
    }
    if (mode_ == Mode::Title) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            audio_.play(AudioCue::UiBack);
            requestQuit();
            return;
        }
        if (IsKeyPressed(KEY_ENTER)) {
            audio_.play(AudioCue::UiConfirm);
            prepareNewGame();
            return;
        }
        if (IsKeyPressed(KEY_L) && load()) {
            editorPlaytest_ = false;
            enterMode(Mode::Playing);
            return;
        }
        if (IsKeyPressed(KEY_E)) {
            audio_.play(AudioCue::UiConfirm);
            openEditor();
            return;
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET)) selectCampaignLevel(-1);
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) selectCampaignLevel(1);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, campaignPreviousButton())) selectCampaignLevel(-1);
            else if (CheckCollisionPointRec(mouse, campaignNextButton())) selectCampaignLevel(1);
            else if (CheckCollisionPointRec(mouse, titleButtonRectangle(0))) {
                audio_.play(AudioCue::UiConfirm);
                prepareNewGame();
            }
            else if (CheckCollisionPointRec(mouse, titleButtonRectangle(1))) {
                if (load()) {
                    editorPlaytest_ = false;
                    enterMode(Mode::Playing);
                }
            } else if (CheckCollisionPointRec(mouse, titleButtonRectangle(2))) {
                audio_.play(AudioCue::UiConfirm);
                openEditor();
            } else if (CheckCollisionPointRec(mouse, titleButtonRectangle(3))) {
                audio_.play(AudioCue::UiBack);
                requestQuit();
            }
        }
        return;
    }

    if (mode_ == Mode::Editor) {
        if (editor_ == nullptr) {
            enterMode(Mode::Title);
            return;
        }
        editor_->update();
        if (editor_->consumeQuitRequest()) { quitRequested_ = true; return; }
        if (editor_->consumeExitRequest()) {
            // A dirty Exit can only arrive after explicit Discard confirmation.
            if (editor_->hasUnsavedChanges()) editor_ = std::make_unique<LevelEditor>(levelPath_, projectFile_);
            adoptEditorProject();
            audio_.play(AudioCue::UiBack);
            enterMode(Mode::Title);
        }
        else if (editor_->consumePlaytestRequest()) beginEditorPlaytest();
        return;
    }

    if (mode_ == Mode::NewGame) {
        updateNewGame();
        return;
    }
    if (mode_ == Mode::PartyManagement) {
        updatePartyManagement();
        return;
    }

    if (mode_ == Mode::Victory || mode_ == Mode::Defeat) {
        if (editorPlaytest_) {
            if (IsKeyPressed(KEY_ENTER)) beginEditorPlaytest();
            if (IsKeyPressed(KEY_ESCAPE)) {
                audio_.play(AudioCue::UiBack);
                enterMode(Mode::Editor);
            }
        } else {
            if (IsKeyPressed(KEY_ENTER)) prepareNewGame();
            if (IsKeyPressed(KEY_ESCAPE)) {
                audio_.play(AudioCue::UiBack);
                enterMode(Mode::Title);
            }
        }
        return;
    }

    updatePlaying(dt);
}

void Game::updateNewGame() {
    const auto starters = roster_.starterIds();
    if (starters.empty()) {
        enterMode(Mode::Title);
        return;
    }

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        starterCursor_ = (starterCursor_ + static_cast<int>(starters.size()) - 1) % static_cast<int>(starters.size());
        audio_.play(AudioCue::Turn);
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        starterCursor_ = (starterCursor_ + 1) % static_cast<int>(starters.size());
        audio_.play(AudioCue::Turn);
    }
    if (IsKeyPressed(KEY_SPACE)) {
        toggleStarter(starters[static_cast<size_t>(starterCursor_)]);
        audio_.play(AudioCue::UiConfirm);
    }
    if (IsKeyPressed(KEY_ONE)) {
        toggleStarter(starters[0]);
        audio_.play(AudioCue::UiConfirm);
    }
    if (starters.size() > 1 && IsKeyPressed(KEY_TWO)) {
        toggleStarter(starters[1]);
        audio_.play(AudioCue::UiConfirm);
    }
    if (starters.size() > 2 && IsKeyPressed(KEY_THREE)) {
        toggleStarter(starters[2]);
        audio_.play(AudioCue::UiConfirm);
    }

    const Vector2 mouse = GetMousePosition();
    for (size_t i = 0; i < starters.size(); ++i) {
        if (CheckCollisionPointRec(mouse, starterCardRectangle(static_cast<int>(i)))) {
            starterCursor_ = static_cast<int>(i);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                toggleStarter(starters[i]);
                audio_.play(AudioCue::UiConfirm);
            }
        }
    }

    if (IsKeyPressed(KEY_ENTER) && !selectedStarters_.empty()) beginNewGame();
    if (IsKeyPressed(KEY_ESCAPE)) {
        audio_.play(AudioCue::UiBack);
        enterMode(Mode::Title);
    }
}

void Game::updatePartyManagement() {
    std::vector<CharacterId> visible;
    for (const auto& record : roster_.records())
        if (record.status != CharacterStatus::Unrecruited) visible.push_back(record.id);
    if (visible.empty()) { enterMode(Mode::Playing); return; }
    managementCursor_ = std::clamp(managementCursor_, 0, static_cast<int>(visible.size()) - 1);
    if (IsKeyPressed(KEY_ESCAPE)) {
        pendingReserveId_ = InvalidCharacterId;
        enterMode(Mode::Playing);
        return;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) managementCursor_ =
        (managementCursor_ + static_cast<int>(visible.size()) - 1) % static_cast<int>(visible.size());
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) managementCursor_ =
        (managementCursor_ + 1) % static_cast<int>(visible.size());
    const auto choose = [&](CharacterId id) {
        const auto* record = roster_.find(id);
        if (!record || !record->alive()) { setMessage("The dead cannot return to the active party."); return; }
        auto members = party_.members();
        if (record->status == CharacterStatus::Reserve) {
            if (party_.full()) {
                pendingReserveId_ = id;
                setMessage("Choose an active character to move into Reserve.");
                return;
            }
            members.push_back(id);
        } else if (record->status == CharacterStatus::Active) {
            if (pendingReserveId_ != InvalidCharacterId) {
                const auto slot = std::find(members.begin(), members.end(), id);
                if (slot == members.end()) return;
                *slot = pendingReserveId_;
                pendingReserveId_ = InvalidCharacterId;
            } else {
                if (members.size() <= 1) { setMessage("At least one living character must remain active."); return; }
                members.erase(std::remove(members.begin(), members.end(), id), members.end());
            }
        } else return;
        Roster candidateRoster = roster_;
        Party candidateParty = party_;
        if (!candidateRoster.setActiveParty(members) || !candidateParty.setMembers(members)) {
            setMessage("That party change is not valid."); return;
        }
        roster_ = std::move(candidateRoster);
        party_ = std::move(candidateParty);
        setMessage("Active party updated. Health and progression were preserved.");
    };
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) choose(visible[static_cast<std::size_t>(managementCursor_)]);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const auto mouse = GetMousePosition();
        constexpr int visibleRows = 6;
        const int first = std::clamp(managementCursor_ - 2, 0,
                                     std::max(0, static_cast<int>(visible.size()) - visibleRows));
        for (int i = 0; i < std::min(visibleRows, static_cast<int>(visible.size()) - first); ++i) {
            const Rectangle card{260, 175.0f + i * 62.0f, 760, 52};
            if (CheckCollisionPointRec(mouse, card)) {
                managementCursor_ = first + i;
                choose(visible[static_cast<std::size_t>(managementCursor_)]);
                break;
            }
        }
    }
}

void Game::updatePlaying(float dt) {
    worldChanged_ = false;
    if (IsKeyPressed(KEY_ESCAPE)) {
        audio_.play(AudioCue::UiBack);
        enterMode(editorPlaytest_ ? Mode::Editor : Mode::Title);
        return;
    }
    combat_.tick(dt);
    gate_.tick(dt);

    if (!runtimeOnly_ && IsKeyPressed(KEY_F1)) debugSetPartySize(1);
    if (!runtimeOnly_ && IsKeyPressed(KEY_F2)) debugSetPartySize(2);
    if (!runtimeOnly_ && IsKeyPressed(KEY_F3)) debugSetPartySize(3);

    // Held movement, not edge-triggered: the ActionGate provides the rhythm, so
    // holding W steps at the tuned cadence instead of requiring key mashing.
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) move(1, 0);
    else if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) move(-1, 0);
    else if (IsKeyDown(KEY_A)) move(0, -1);
    else if (IsKeyDown(KEY_D)) move(0, 1);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_Q)) turn(-1);
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_E)) turn(1);
    if (IsKeyDown(KEY_SPACE)) attack();
    if (IsKeyPressed(KEY_F)) interact();
    if (IsKeyPressed(KEY_H)) drinkPotion();
    if (IsKeyPressed(KEY_F5)) {
        const bool saved = !editorPlaytest_ && save();
        setMessage(editorPlaytest_ ? "Game saves are disabled during editor playtests."
                                   : (saved ? "Game saved." : "Save failed."));
        audio_.play(saved ? AudioCue::Save : AudioCue::Error);
    }
    if (IsKeyPressed(KEY_F9)) {
        if (editorPlaytest_) {
            setMessage("Save loading is disabled during editor playtests.");
            audio_.play(AudioCue::Error);
        } else {
            load();
        }
    }

    // Interactions may open a modal game screen. Do not let enemies take a
    // hidden turn on the same frame that the party-management screen opens.
    if (mode_ != Mode::Playing) return;
    if (worldChanged_) return;

    const auto enemyEvent = combat_.updateEnemies(dt, roster_, party_, dungeon_, player_);
    if (enemyEvent.occurred()) setMessage(enemyEvent.message, enemyEvent.messageSeconds);

    if (party_.empty()) enterMode(Mode::Defeat);
    if (dungeon_.tile(player_.x(), player_.y()) == Tile::Exit) {
        const auto* passage = dungeon_.objectAt(player_.x(), player_.y());
        if (passage == nullptr || passage->kind != WorldObjectKind::LevelTransition) enterMode(Mode::Victory);
    }
}

void Game::move(int forward, int strafe) {
    if (!gate_.tryMove(combatTuning())) return;
    const auto [nx, ny] = player_.movementTarget(forward, strafe);
    if (dungeon_.blocksMovement(nx, ny) || combat_.enemyAt(dungeon_, nx, ny)) {
        setMessage("Something blocks the way.", 1.0f);
        audio_.play(AudioCue::Bump);
        return;
    }
    fireEvent({EventTriggerType::PlayerLeaveTile, player_.x(), player_.y()});
    player_.moveTo(nx, ny);
    audio_.play(AudioCue::Step);
    fireStoryTrigger(TriggerEvent::EnterCell, nx, ny);
    const auto* room = dungeon_.roomAt(nx, ny);
    const std::string roomId = room == nullptr ? std::string{} : room->id;
    if (room != nullptr && roomId != currentRoomId_) {
        if (!fireStoryTrigger(TriggerEvent::EnterRoom, room->x, room->y, room->id)) {
            // Purpose, mood, and intended feeling are private creator notes.
            setMessage(room->name, 3.0f);
        }
    }
    currentRoomId_ = roomId;
    collectPickup();
    if (dungeon_.tile(player_.x(), player_.y()) == Tile::Exit) {
        const auto* object = dungeon_.objectAt(player_.x(), player_.y());
        if (object && object->kind == WorldObjectKind::LevelTransition)
            fireStoryTrigger(TriggerEvent::InteractObject, object->x, object->y, object->id);
    }
}

void Game::turn(int delta) {
    // Turning has its own recovery and never consumes the attack gate.
    if (!gate_.tryTurn(combatTuning())) return;
    player_.turn(delta);
    audio_.play(AudioCue::Turn);
}

void Game::interact() {
    const auto [tx, ty] = player_.frontCell();
    if (dungeon_.tile(tx, ty) == Tile::SecretDoorClosed) {
        if (dungeon_.revealSecret(tx, ty)) {
            setMessage("Loose stone gives way. A hidden niche opens.", 2.5f);
            audio_.play(AudioCue::Secret);
        }
        return;
    }
    if (dungeon_.tile(tx, ty) == Tile::DoorClosed) {
        const auto* door = dungeon_.doorAt(tx, ty);
        if (door) fireEvent({EventTriggerType::InteractObject, tx, ty, door->id});
        else setMessage("This door is missing its authored identity.");
        return;
    }
    if (const auto* object = dungeon_.objectAt(tx, ty)) {
        if (!fireStoryTrigger(TriggerEvent::InteractObject, tx, ty, object->id)) {
            setMessage(object->text.empty() ? object->name : object->text, 4.0f);
        }
        audio_.play(AudioCue::UiConfirm);
        return;
    }
    setMessage("Nothing here responds.", 1.0f);
    audio_.play(AudioCue::Error);
}

void Game::attack() {
    if (!gate_.tryAttack(combatTuning())) return;
    const int targetIndex = combat_.frontEnemyIndex(dungeon_, player_, 1);
    std::string targetId;
    int targetX{};
    int targetY{};
    if (targetIndex >= 0) {
        const auto& target = dungeon_.enemies()[static_cast<std::size_t>(targetIndex)];
        targetId = target.id;
        targetX = target.x;
        targetY = target.y;
    }
    const auto event = combat_.partyAttack(roster_, party_, dungeon_, player_);
    xp_ += event.xpGained;
    if (event.occurred()) {
        if (event.xpGained <= 0 || !fireStoryTrigger(TriggerEvent::KillEnemy, targetX, targetY, targetId)) {
            setMessage(event.message, event.messageSeconds);
        }
        audio_.play(event.xpGained > 0 ? AudioCue::Kill :
                    (event.message.find("empty air") != std::string::npos ? AudioCue::Attack : AudioCue::Hit));
    } else {
        audio_.play(AudioCue::Attack);
    }
}

void Game::drinkPotion() {
    if (potions_ <= 0) {
        setMessage("No healing draughts remain.");
        audio_.play(AudioCue::Error);
        return;
    }
    CharacterId targetId = InvalidCharacterId;
    float lowestRatio = 1.0f;
    for (const auto id : party_.members()) {
        const auto* record = roster_.find(id);
        const auto* definition = roster_.definition(id);
        if (record == nullptr || definition == nullptr || !record->alive()) continue;
        const float ratio = static_cast<float>(record->hp) / definition->maxHp;
        if (ratio < lowestRatio) {
            lowestRatio = ratio;
            targetId = id;
        }
    }
    if (targetId == InvalidCharacterId) {
        setMessage("No one needs healing.");
        audio_.play(AudioCue::Error);
        return;
    }
    roster_.heal(targetId, 18);
    --potions_;
    setMessage(roster_.definition(targetId)->name + " drinks a healing draught.");
    audio_.play(AudioCue::Heal);
}

void Game::collectPickup() {
    for (auto& pickup : dungeon_.pickups()) {
        if (!pickup.taken && pickup.x == player_.x() && pickup.y == player_.y()) {
            pickup.taken = true;
            if (pickup.type == Pickup::Type::Key) {
                ++keys_;
                setMessage("You found an iron key.");
            } else {
                ++potions_;
                setMessage("You found a healing draught.");
            }
            audio_.play(AudioCue::Pickup);
            fireStoryTrigger(TriggerEvent::PickupItem, pickup.x, pickup.y, pickup.id);
        }
    }
}

bool Game::fireStoryTrigger(TriggerEvent event, int x, int y, const std::string& subjectId) {
    return fireEvent({eventTriggerType(event), x, y, subjectId}).eventsRun > 0;
}

EventFireResult Game::fireEvent(const EventContext& context) {
    std::string pendingLevelTransition;
    std::string pendingArrival;
    WorldEventPresentation presentation;
    presentation.message = [this](const std::string& text) {
        if (storyMessages_.empty()) storyMessageTimer_ = 4.0f;
        // Separate from transient combat/UI messages so a pickup or hit does
        // not erase dialogue from the same frame.
        storyMessages_.push_back(text);
    };
    presentation.cue = [this](WorldEventCue cue) {
        audio_.play(cue == WorldEventCue::DoorOpened ? AudioCue::DoorOpen : AudioCue::DoorLocked);
    };
    presentation.recruit = [this](CharacterId id) {
        const auto* definition = roster_.definition(id);
        return definition && definition->recruitable && roster_.recruit(id);
    };
    presentation.openPartyManagement = [this]() {
        managementCursor_ = 0;
        pendingReserveId_ = InvalidCharacterId;
        enterMode(Mode::PartyManagement);
    };
    // Never replace events_ from inside EventRuntime::fire: doing so would
    // destroy the dispatcher whose stack is still active. Commit the world
    // swap only after dispatch has unwound.
    presentation.transitionLevel = [&](const std::string& levelId, const std::string& arrivalId) {
        pendingLevelTransition = levelId;
        pendingArrival = arrivalId;
        return !levelId.empty() && !arrivalId.empty();
    };
    const auto result = dispatchWorldEvent(events_, context, dungeon_, keys_, presentation);
    if (!pendingLevelTransition.empty() && !transitionToLevel(pendingLevelTransition, pendingArrival))
        setMessage("That passage is not connected to a valid arrival point.");
    return result;
}

bool Game::loadRegisteredLevel(const std::string& levelId, LevelDefinition& level,
                               std::string& path, int& campaignIndex) const {
    const auto found = std::find_if(campaign_.levels.begin(), campaign_.levels.end(), [&](const auto& entry) {
        return entry.id == levelId;
    });
    if (found == campaign_.levels.end()) return false;
    campaignIndex = static_cast<int>(std::distance(campaign_.levels.begin(), found));
    path = contentRoot_.empty() ? resolveContentPath(found->path)
                                : (std::filesystem::path{contentRoot_} / found->path).string();
    std::string error;
    return LevelIO::load(path, level, error) && level.id == levelId;
}

bool Game::transitionToLevel(const std::string& levelId, const std::string& arrivalId) {
    LevelDefinition definition;
    std::string path;
    int campaignIndex{};
    if (!loadRegisteredLevel(levelId, definition, path, campaignIndex)) return false;
    const auto arrival = std::find_if(definition.objects.begin(), definition.objects.end(), [&](const auto& object) {
        return object.kind == WorldObjectKind::ArrivalPoint && object.id == arrivalId;
    });
    if (arrival == definition.objects.end()) return false;

    Dungeon destination{definition};
    EventRuntime destinationEvents;
    if (!configureWorldEvents(destination, destinationEvents) ||
        destination.blocksMovement(arrival->x, arrival->y) ||
        std::any_of(destination.enemies().begin(), destination.enemies().end(), [&](const auto& enemy) {
            return enemy.alive && enemy.x == arrival->x && enemy.y == arrival->y;
        })) return false;

    CampaignState nextState = campaignState_;
    if (!nextState.capture(dungeon_, events_)) return false;
    if (nextState.contains(levelId)) {
        if (!nextState.restore(levelId, destination, destinationEvents)) return false;
        nextState.erase(levelId);
    }

    dungeon_ = std::move(destination);
    events_ = std::move(destinationEvents);
    player_ = PlayerState{arrival->x, arrival->y, arrival->facing};
    campaignState_ = std::move(nextState);
    levelPath_ = std::move(path);
    campaignLevelIndex_ = campaignIndex;
    levelMusicPath_ = dungeon_.musicPath();
    currentRoomId_.clear();
    if (const auto* room = dungeon_.roomAt(player_.x(), player_.y())) currentRoomId_ = room->id;
    combat_.reset();
    gate_.reset();
    worldChanged_ = true;
    audio_.playMusic(levelMusicPath_);
    return true;
}

void Game::enterMode(Mode mode) {
    if (mode_ == mode) return;
    mode_ = mode;
    if (mode_ == Mode::Playing) audio_.playMusic(levelMusicPath_);
    else audio_.stopMusic();
    if (mode_ == Mode::Victory) audio_.play(AudioCue::Victory);
    if (mode_ == Mode::Defeat) audio_.play(AudioCue::Defeat);
}

bool Game::save() const {
    const auto file = contentRoot_.empty() ? std::filesystem::path{"stoneveil.sav"} : std::filesystem::path{contentRoot_} / "saves/game.sav";
    std::error_code error;
    if (!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path(), error);
    if (error) return false;
    return SaveSystem::save(file.string(), player_, roster_, party_, dungeon_, keys_, potions_, xp_,
                            &events_, &campaignState_);
}

bool Game::load() {
    const auto file = contentRoot_.empty() ? std::filesystem::path{"stoneveil.sav"} : std::filesystem::path{contentRoot_} / "saves/game.sav";
    std::string savedLevel;
    if (!SaveSystem::savedLevelId(file.string(), savedLevel)) {
        setMessage("No valid save file found.");
        audio_.play(AudioCue::Error);
        return false;
    }
    Dungeon loadedDungeon = dungeon_;
    std::string loadedPath = levelPath_;
    int loadedCampaignIndex = campaignLevelIndex_;
    if (!savedLevel.empty() && savedLevel != dungeon_.levelId()) {
        LevelDefinition definition;
        if (!loadRegisteredLevel(savedLevel, definition, loadedPath, loadedCampaignIndex)) {
            setMessage("The save references a level that is no longer registered.");
            audio_.play(AudioCue::Error);
            return false;
        }
        loadedDungeon = Dungeon{definition};
    }
    PlayerState loadedPlayer = player_;
    Roster loadedRoster = roster_;
    Party loadedParty = party_;
    EventRuntime loadedEvents;
    CampaignState loadedCampaignState;
    int loadedKeys = keys_;
    int loadedPotions = potions_;
    int loadedXp = xp_;
    if (!SaveSystem::load(file.string(), loadedPlayer, loadedRoster, loadedParty, loadedDungeon,
                          loadedKeys, loadedPotions, loadedXp, &loadedEvents, &loadedCampaignState)) {
        setMessage("No valid save file found.");
        audio_.play(AudioCue::Error);
        return false;
    }
    player_ = loadedPlayer;
    roster_ = std::move(loadedRoster);
    party_ = std::move(loadedParty);
    dungeon_ = std::move(loadedDungeon);
    events_ = std::move(loadedEvents);
    campaignState_ = std::move(loadedCampaignState);
    keys_ = loadedKeys;
    potions_ = loadedPotions;
    xp_ = loadedXp;
    levelPath_ = std::move(loadedPath);
    campaignLevelIndex_ = loadedCampaignIndex;
    combat_.reset();
    gate_.reset();
    storyMessages_.clear();
    storyMessageTimer_ = 0.0f;
    const auto* room = dungeon_.roomAt(player_.x(), player_.y());
    currentRoomId_ = room ? room->id : std::string{};
    levelMusicPath_ = dungeon_.musicPath();
    audio_.playMusic(levelMusicPath_);
    worldChanged_ = true;
    setMessage("Save loaded.");
    audio_.play(AudioCue::Save);
    return true;
}

void Game::setMessage(std::string message, float seconds) {
    message_ = std::move(message);
    messageTimer_ = seconds;
}

void Game::draw() const {
    BeginDrawing();
    ClearBackground(Color{15, 16, 18, 255});
    if (mode_ == Mode::Title) drawTitle();
    else if (mode_ == Mode::NewGame) drawNewGame();
    else if (mode_ == Mode::PartyManagement) drawPartyManagement();
    else if (mode_ == Mode::Victory) drawEndScreen(true);
    else if (mode_ == Mode::Defeat) drawEndScreen(false);
    else if (mode_ == Mode::Editor && editor_ != nullptr) editor_->draw();
    else {
        drawWorld();
        drawHud();
    }
    EndDrawing();
}

void Game::drawWorld() const {
    raycaster_.draw(dungeon_, player_);

    const int enemyIndex = combat_.frontEnemyIndex(dungeon_, player_, 6);
    if (enemyIndex >= 0) {
        const auto& enemy = dungeon_.enemies()[static_cast<size_t>(enemyIndex)];
        const int dist = std::max(1, std::abs(enemy.x - player_.x()) + std::abs(enemy.y - player_.y()));
        const int size = std::clamp(300 / dist, 55, 280);
        const int cx = 24 + ViewW / 2;
        const int cy = ViewY + ViewH / 2 + 45;
        const EnemyDefinition& type = enemyTypeOrDefault(enemy.typeId);

        // Placeholder telegraph: the body flares and a growing bar fills while
        // the enemy is committed to a swing. Readable, and cheap to replace.
        const Color body = enemy.winding ? Color{188, 62, 44, 255}
                                         : (enemy.recoveryRemaining > 0.0f ? Color{74, 62, 74, 255}
                                                                           : Color{108, 28, 30, 255});
        DrawRectangle(cx - size / 2, cy - size, size, size, body);
        DrawRectangleLines(cx - size / 2, cy - size, size, size,
                           enemy.winding ? Color{255, 226, 150, 255} : Color{224, 169, 111, 255});
        DrawText(TextFormat("%s  %d HP", type.name.c_str(), enemy.hp), cx - 60, cy - size - 24, 18, RAYWHITE);

        if (enemy.winding) {
            const float windup = std::max(0.01f, combatTuning().enemyAttackWindup * type.windupScale);
            const float charged = std::clamp(1.0f - enemy.windupRemaining / windup, 0.0f, 1.0f);
            DrawRectangle(cx - size / 2, cy + 8, size, 10, Color{48, 40, 38, 255});
            DrawRectangle(cx - size / 2, cy + 8, static_cast<int>(size * charged), 10, Color{236, 182, 88, 255});
            DrawText("WINDUP", cx - 30, cy + 24, 16, Color{255, 226, 150, 255});
        } else if (enemy.recoveryRemaining > 0.0f) {
            DrawText("RECOVERING", cx - 46, cy + 24, 16, Color{150, 208, 160, 255});
        }
    } else {
        const WorldObject* visibleObject = nullptr;
        int objectDistance = 0;
        int x = player_.x();
        int y = player_.y();
        for (int distance = 1; distance <= 6; ++distance) {
            x += PlayerState::directionX(player_.direction());
            y += PlayerState::directionY(player_.direction());
            if (dungeon_.blocksSight(x, y)) break;
            visibleObject = dungeon_.objectAt(x, y);
            if (visibleObject != nullptr) {
                objectDistance = distance;
                break;
            }
        }
        if (visibleObject != nullptr) {
            const int size = std::clamp(220 / std::max(1, objectDistance), 42, 190);
            const int cx = 24 + ViewW / 2;
            const int cy = ViewY + ViewH / 2 + 80;
            Color body{104, 88, 74, 255};
            const char* glyph = "P";
            if (visibleObject->kind == WorldObjectKind::Shrine) { body = {122, 92, 151, 255}; glyph = "R"; }
            else if (visibleObject->kind == WorldObjectKind::Note) { body = {190, 164, 102, 255}; glyph = "N"; }
            else if (visibleObject->kind == WorldObjectKind::Corpse) { body = {89, 75, 72, 255}; glyph = "C"; }
            else if (visibleObject->kind == WorldObjectKind::Npc) { body = {76, 105, 126, 255}; glyph = "@"; }
            else if (visibleObject->kind == WorldObjectKind::Recruit) { body = {80, 128, 99, 255}; glyph = "+"; }
            else if (visibleObject->kind == WorldObjectKind::PartyManagement) { body = {146, 116, 58, 255}; glyph = "M"; }
            else if (visibleObject->kind == WorldObjectKind::LevelTransition) { body = {84, 109, 151, 255}; glyph = ">"; }
            DrawRectangle(cx - size / 2, cy - size, size, size, body);
            DrawRectangleLines(cx - size / 2, cy - size, size, size, Color{220, 193, 134, 255});
            const int glyphSize = std::max(24, size / 2);
            DrawText(glyph, cx - MeasureText(glyph, glyphSize) / 2, cy - size / 2 - glyphSize / 2,
                     glyphSize, RAYWHITE);
            const int nameWidth = MeasureText(visibleObject->name.c_str(), 18);
            DrawText(visibleObject->name.c_str(), cx - nameWidth / 2, cy - size - 24, 18, RAYWHITE);
        }
    }
}

void Game::drawHud() const {
    const int panelX = 978;
    if (editorPlaytest_) DrawText("EDITOR PLAYTEST", panelX, 16, 15, Color{90, 165, 226, 255});
    DrawText(dungeon_.name().c_str(), panelX, 42, 19, Color{221, 196, 139, 255});
    DrawText(TextFormat("KEYS %d   DRAUGHTS %d", keys_, potions_), panelX, 78, 18, LIGHTGRAY);
    DrawText(TextFormat("XP %d   PARTY %d/%d", xp_, static_cast<int>(party_.size()),
                        static_cast<int>(party_.capacity())), panelX, 104, 18, LIGHTGRAY);

    int y = 154;
    const bool compactCards = party_.size() > Party::InitialCapacity;
    const int cardHeight = compactCards ? 48 : 92;
    const int cardStep = compactCards ? 56 : 104;
    for (const auto id : party_.members()) {
        const auto* record = roster_.find(id);
        const auto* definition = roster_.definition(id);
        if (record == nullptr || definition == nullptr) continue;
        DrawRectangle(panelX, y, 272, cardHeight, Color{27, 29, 32, 255});
        DrawRectangleLines(panelX, y, 272, cardHeight, Color{91, 83, 65, 255});
        DrawText(definition->name.c_str(), panelX + 12, y + (compactCards ? 7 : 10), compactCards ? 17 : 21, RAYWHITE);
        DrawText(TextFormat("HP %d / %d", record->hp, definition->maxHp),
                 panelX + (compactCards ? 142 : 12), y + (compactCards ? 8 : 39),
                 compactCards ? 15 : 18, Color{170, 212, 151, 255});
        const float ratio = definition->maxHp ? static_cast<float>(record->hp) / definition->maxHp : 0.0f;
        const int barY = y + (compactCards ? 33 : 65);
        DrawRectangle(panelX + 12, barY, 240, compactCards ? 7 : 10, Color{48, 45, 42, 255});
        DrawRectangle(panelX + 12, barY, static_cast<int>(240 * std::clamp(ratio, 0.0f, 1.0f)),
                      compactCards ? 7 : 10, Color{135, 52, 48, 255});
        y += cardStep;
    }

    {
        const auto& pick = combat_.lastMeleeTarget();
        const auto* targetDefinition = roster_.definition(pick.target);
        const int debugY = y + 2;
        if (!runtimeOnly_) {
        DrawText(TextFormat("TARGETING  %s  (n=%d)",
                            targetDefinition != nullptr ? targetDefinition->name.c_str() : "-",
                            combat_.meleeResolutions()),
                 panelX, debugY, 15, Color{150, 208, 160, 255});
        DrawText(TextFormat("GATE  mv %.2f  atk %.2f  (%d/%d)",
                            static_cast<double>(gate_.moveRemaining()),
                            static_cast<double>(gate_.attackRemaining()),
                            gate_.movesTaken(), gate_.movesBlocked()),
                 panelX, debugY + 20, 15, Color{150, 208, 160, 255});
        DrawText("F1/F2/F3 debug party size", panelX, debugY + 40, 15, GRAY);
        }
        DrawText("W/S move   A/D strafe", panelX, debugY + 68, 16, GRAY);
        DrawText("Q/E or arrows turn", panelX, debugY + 90, 16, GRAY);
        DrawText("SPACE attack   F interact", panelX, debugY + 112, 16, GRAY);
        DrawText(editorPlaytest_ ? "H heal   ESC return to editor" : "H heal   F5 save   F9 load",
                 panelX, debugY + 134, 16, GRAY);
    }

    if (!storyMessages_.empty() || (messageTimer_ > 0.0f && !message_.empty())) {
        DrawRectangle(24, 610, ViewW, 74, Color{11, 12, 14, 235});
        DrawRectangleLines(24, 610, ViewW, 74, Color{91, 83, 65, 255});
        const auto& shown = storyMessages_.empty() ? message_ : storyMessages_.front();
        DrawText(shown.c_str(), 44, 635, 21, Color{224, 217, 194, 255});
    }
}

void Game::drawTitle() const {
    DrawText("STONEVEIL", 430, 132, 64, Color{220, 193, 134, 255});
    DrawText("A SYSTEMS-FIRST DUNGEON CRAWLER", 417, 218, 22, LIGHTGRAY);
    DrawText(STONEVEIL_BUILD_LABEL, 504, 254, 16, Color{90, 165, 226, 255});
    if (!campaign_.levels.empty() && !runtimeOnly_) {
        const auto& level = campaign_.levels[static_cast<std::size_t>(campaignLevelIndex_)];
        DrawRectangleRec(campaignPreviousButton(), Color{29, 31, 35, 255});
        DrawRectangleRec(campaignNextButton(), Color{29, 31, 35, 255});
        DrawRectangleLinesEx(campaignPreviousButton(), 1.0f, Color{91, 83, 65, 255});
        DrawRectangleLinesEx(campaignNextButton(), 1.0f, Color{91, 83, 65, 255});
        DrawText("<", 420, 290, 20, LIGHTGRAY);
        DrawText(">", 846, 290, 20, LIGHTGRAY);
        const std::string label = TextFormat("LEVEL %d/%d  %s", campaignLevelIndex_ + 1,
                                             static_cast<int>(campaign_.levels.size()), level.name.c_str());
        const int width = MeasureText(label.c_str(), 17);
        DrawText(label.c_str(), 640 - width / 2, 292, 17, Color{172, 165, 145, 255});
    }

    static constexpr std::array<const char*, 4> labels = {
        "NEW GAME", "LOAD GAME", "DUNGEON EDITOR", "QUIT",
    };
    static constexpr std::array<const char*, 4> shortcuts = {
        "ENTER", "L", "E", "ESC",
    };
    const Vector2 mouse = GetMousePosition();
    for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
        if (runtimeOnly_ && index == 2) continue;
        const Rectangle button = titleButtonRectangle(index);
        const bool hovered = CheckCollisionPointRec(mouse, button);
        DrawRectangleRec(button, hovered ? Color{64, 59, 49, 255} : Color{29, 31, 35, 255});
        DrawRectangleLinesEx(button, hovered ? 2.0f : 1.0f,
                             hovered ? Color{220, 193, 134, 255} : Color{91, 83, 65, 255});
        DrawText(labels[static_cast<std::size_t>(index)], static_cast<int>(button.x) + 20,
                 static_cast<int>(button.y) + 13, 21, hovered ? RAYWHITE : LIGHTGRAY);
        const int shortcutWidth = MeasureText(shortcuts[static_cast<std::size_t>(index)], 15);
        DrawText(shortcuts[static_cast<std::size_t>(index)],
                 static_cast<int>(button.x + button.width) - shortcutWidth - 18,
                 static_cast<int>(button.y) + 16, 15, GRAY);
    }
    DrawText("One application: play, build, test, refine.", 464, 604, 18, GRAY);
}

void Game::drawNewGame() const {
    const auto starters = roster_.starterIds();
    DrawText("CHOOSE WHO DESCENDS", 372, 62, 42, Color{220, 193, 134, 255});
    DrawText("Select one, two, or all three. Death will be permanent.", 339, 122, 20, LIGHTGRAY);

    for (size_t i = 0; i < starters.size(); ++i) {
        const auto* definition = roster_.definition(starters[i]);
        if (definition == nullptr) continue;
        const Rectangle card = starterCardRectangle(static_cast<int>(i));
        const bool selected = std::find(selectedStarters_.begin(), selectedStarters_.end(), starters[i]) != selectedStarters_.end();
        const bool focused = starterCursor_ == static_cast<int>(i);
        const Color fill = selected ? Color{48, 55, 52, 255} : Color{26, 28, 32, 255};
        const Color border = selected ? Color{176, 145, 82, 255} : (focused ? Color{108, 119, 126, 255} : Color{72, 68, 61, 255});
        DrawRectangleRec(card, fill);
        DrawRectangleLinesEx(card, focused ? 4.0f : 2.0f, border);

        DrawRectangle(static_cast<int>(card.x) + 24, static_cast<int>(card.y) + 26, 72, 72,
                      selected ? Color{115, 48, 46, 255} : Color{55, 56, 59, 255});
        DrawText(TextFormat("%d", static_cast<int>(i + 1)), static_cast<int>(card.x) + 51, static_cast<int>(card.y) + 48, 28, RAYWHITE);
        DrawText(definition->name.c_str(), static_cast<int>(card.x) + 116, static_cast<int>(card.y) + 26, 27, RAYWHITE);
        DrawText(definition->role.c_str(), static_cast<int>(card.x) + 116, static_cast<int>(card.y) + 64, 18,
                 Color{172, 165, 145, 255});
        DrawText(TextFormat("HP  %d", definition->maxHp), static_cast<int>(card.x) + 26, static_cast<int>(card.y) + 132, 21, LIGHTGRAY);
        DrawText(TextFormat("POWER  %d", definition->power), static_cast<int>(card.x) + 176, static_cast<int>(card.y) + 132, 21, LIGHTGRAY);
        DrawText(definition->summary.c_str(), static_cast<int>(card.x) + 26, static_cast<int>(card.y) + 190, 16,
                 Color{190, 188, 179, 255});
        DrawText(selected ? "SELECTED" : "AVAILABLE", static_cast<int>(card.x) + 26, static_cast<int>(card.y) + 310, 20,
                 selected ? Color{190, 214, 167, 255} : GRAY);
    }

    DrawText("1 / 2 / 3 or click: toggle    LEFT / RIGHT: focus", 325, 592, 19, GRAY);
    if (selectedStarters_.empty()) {
        DrawText("Choose at least one character to begin.", 428, 640, 20, Color{196, 92, 76, 255});
    } else {
        DrawText(TextFormat("ENTER  begin with %d    ESC  back", static_cast<int>(selectedStarters_.size())), 456, 640, 20, RAYWHITE);
    }
}

void Game::drawPartyManagement() const {
    DrawText("PARTY MANAGEMENT", 380, 75, 38, Color{211, 187, 126, 255});
    DrawText("Choose who travels in Party Space. Reserve members add no combat power.", 275, 126, 18, LIGHTGRAY);
    std::vector<const CharacterRecord*> visible;
    for (const auto& record : roster_.records()) {
        if (record.status != CharacterStatus::Unrecruited) visible.push_back(&record);
    }
    constexpr int visibleRows = 6;
    const int first = std::clamp(managementCursor_ - 2, 0,
                                 std::max(0, static_cast<int>(visible.size()) - visibleRows));
    const int count = std::min(visibleRows, static_cast<int>(visible.size()) - first);
    for (int row = 0; row < count; ++row) {
        const auto& record = *visible[static_cast<std::size_t>(first + row)];
        const auto* definition = roster_.definition(record.id);
        if (!definition) continue;
        const Rectangle card{260, 175.0f + row * 62.0f, 760, 52};
        const bool selected = first + row == managementCursor_;
        DrawRectangleRec(card, selected ? Color{76, 62, 37, 255} : Color{30, 32, 36, 255});
        DrawRectangleLinesEx(card, 1, selected ? Color{211, 187, 126, 255} : Color{80, 82, 87, 255});
        DrawText(definition->name.c_str(), 280, static_cast<int>(card.y) + 9, 20, RAYWHITE);
        const char* state = record.status == CharacterStatus::Active ? "ACTIVE" :
            (record.status == CharacterStatus::Reserve ? "RESERVE" : "PERMANENTLY DEAD");
        DrawText(TextFormat("%s   HP %d / %d   XP %d", state, record.hp, definition->maxHp, record.xp),
                 560, static_cast<int>(card.y) + 14, 16,
                 record.status == CharacterStatus::Dead ? Color{170, 66, 64, 255} :
                 (record.status == CharacterStatus::Active ? Color{160, 210, 150, 255} : LIGHTGRAY));
    }
    if (visible.size() > visibleRows)
        DrawText(TextFormat("ROSTER %d-%d OF %d", first + 1, first + count, static_cast<int>(visible.size())),
                 260, 552, 14, GRAY);
    if (pendingReserveId_ != InvalidCharacterId) {
        const auto* pending = roster_.definition(pendingReserveId_);
        DrawText(TextFormat("ADDING %s: choose an ACTIVE member to replace.", pending ? pending->name.c_str() : "RECRUIT"),
                 325, 575, 18, Color{211, 187, 126, 255});
    }
    DrawText("UP/DOWN + ENTER or click: activate / reserve / swap", 350, 615, 18, LIGHTGRAY);
    DrawText("ESC: return to dungeon", 510, 650, 17, GRAY);
}

void Game::drawEndScreen(bool won) const {
    const char* title = won ? "THE VEIL OPENS" : "THE PARTY HAS FALLEN";
    const Color color = won ? Color{211, 187, 126, 255} : Color{170, 66, 64, 255};
    DrawText(title, 390, 240, 46, color);
    DrawText(won ? "You reached the first prototype exit." : "The dungeon keeps what it kills.", 420, 318, 21, LIGHTGRAY);
    DrawText(editorPlaytest_ ? "ENTER replay    ESC return to editor" : "ENTER restart    ESC title",
             editorPlaytest_ ? 430 : 480, 410, 20, RAYWHITE);
}

} // namespace sv
