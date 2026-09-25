#pragma once

#include "CombatTuning.hpp"

namespace sv {

// Player action commitment.
//
// Movement is not free: each step, turn and attack starts an independent
// recovery timer, and the corresponding input is ignored until it expires. This
// is the first, deliberately crude candidate answer to square-dancing — the
// point is to have ONE tunable place to test the idea, not to solve it here.
//
// The three timers are independent on purpose: turning must never be treated as
// attacking, so looking at a threat stays cheap even while a swing is on
// cooldown.
class ActionGate {
public:
    void tick(float dt);
    void reset();

    // Each returns true and starts the recovery, or false when still recovering.
    bool tryMove(const CombatTuning& tuning);
    bool tryTurn(const CombatTuning& tuning);
    bool tryAttack(const CombatTuning& tuning);

    float moveRemaining() const { return moveRemaining_; }
    float turnRemaining() const { return turnRemaining_; }
    float attackRemaining() const { return attackRemaining_; }

    bool moveReady() const { return moveRemaining_ <= 0.0f; }
    bool attackReady() const { return attackRemaining_ <= 0.0f; }

    // Counters exist so a runtime debug overlay and the tests can both prove
    // the gate is actually consulted rather than silently bypassed.
    int movesTaken() const { return movesTaken_; }
    int movesBlocked() const { return movesBlocked_; }

private:
    float moveRemaining_{0.0f};
    float turnRemaining_{0.0f};
    float attackRemaining_{0.0f};
    int movesTaken_{0};
    int movesBlocked_{0};
};

} // namespace sv
