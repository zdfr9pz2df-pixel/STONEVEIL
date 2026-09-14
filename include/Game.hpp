#pragma once

#include "Dungeon.hpp"
#include "EventSystem.hpp"
#include "raylib.h"

#include <array>
#include <string>
#include <unordered_map>

namespace sv {

struct PartyMember {
    std::string name;
    int hp{};
    int maxHp{};
    int power{};
    bool alive() const { return hp > 0; }
};

class Game {
public:
    Game();
    void run();

private:
    enum class Mode { Title, Playing, Victory, Defeat };

    void reset();
    void update(float dt);
    void updatePlaying(float dt);
    void draw() const;
    void drawWorld() const;
    void drawHud() const;
    void drawTitle() const;
    void drawEndScreen(bool won) const;

    void move(int forward, int strafe);
    void turn(int delta);
    void interact();
    void attack();
    void drinkPotion();
    void collectPickup();
    void updateEnemies(float dt);
    bool enemyAt(int x, int y, int ignoreIndex = -1) const;
    int frontEnemyIndex(int maxDistance = 1) const;
    bool save() const;
    bool load();
    void setMessage(std::string message, float seconds = 2.0f);

    void configureEvents();
    EventServices eventServices();
    EventFireResult fireEvent(const EventContext& context);
    EventValue readEventFact(const std::string& name) const;
    void executeEventAction(const EventAction& action, const EventContext& context);

    Dungeon dungeon_;
    EventRuntime events_;
    std::unordered_map<std::string, EventValue> eventFacts_;
    Mode mode_{Mode::Title};
    int px_{2};
    int py_{2};
    int dir_{1}; // 0 north, 1 east, 2 south, 3 west
    std::array<PartyMember, 3> party_{};
    int keys_{0};
    int potions_{1};
    int xp_{0};
    float attackCooldown_{0.0f};
    mutable std::string message_;
    float messageTimer_{0.0f};
};

} // namespace sv
