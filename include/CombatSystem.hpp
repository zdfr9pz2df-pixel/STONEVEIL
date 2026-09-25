#pragma once

#include "Character.hpp"
#include "CombatTuning.hpp"
#include "Formation.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace sv {

class Dungeon;
class Party;
class PlayerState;
class Roster;

struct CombatEvent {
    std::string message;
    float messageSeconds{0.0f};
    int xpGained{0};

    bool occurred() const { return !message.empty() || xpGained != 0; }
};

class CombatSystem {
public:
    CombatSystem();
    explicit CombatSystem(std::uint32_t randomSeed);

    void reset();
    void tick(float dt);

    CombatEvent partyAttack(Roster& roster, const Party& party, Dungeon& dungeon, const PlayerState& player);
    CombatEvent updateEnemies(float dt, Roster& roster, Party& party, Dungeon& dungeon, const PlayerState& player);

    bool enemyAt(const Dungeon& dungeon, int x, int y, int ignoreIndex = -1) const;
    int frontEnemyIndex(const Dungeon& dungeon, const PlayerState& player, int maxDistance = 1) const;

    // Party order IS the formation. Exposed so targeting can be tested directly
    // and shown in the debug overlay.
    static std::vector<FormationMember> formationFor(const Roster& roster, const Party& party);

    // Proof that deterministic targeting actually ran at runtime, rather than
    // existing only as a library nothing calls.
    const MeleeTargetResult& lastMeleeTarget() const { return lastMeleeTarget_; }
    int meleeResolutions() const { return meleeResolutions_; }

    // True while any enemy is winding up or recovering from a blow. Combat
    // danger gating for saves will hang off this.
    static bool dangerActive(const Dungeon& dungeon);

private:
    int randomInt(int minimum, int maximum);

    std::mt19937 random_;
    float partyAttackCooldown_{0.0f};
    MeleeTargetResult lastMeleeTarget_{};
    int meleeResolutions_{0};
};

} // namespace sv
