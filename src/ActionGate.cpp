#include "ActionGate.hpp"

#include <algorithm>

namespace sv {
namespace {
float decay(float remaining, float dt) {
    return std::max(0.0f, remaining - dt);
}
} // namespace

void ActionGate::tick(float dt) {
    if (dt <= 0.0f) return;
    moveRemaining_ = decay(moveRemaining_, dt);
    turnRemaining_ = decay(turnRemaining_, dt);
    attackRemaining_ = decay(attackRemaining_, dt);
}

void ActionGate::reset() {
    moveRemaining_ = 0.0f;
    turnRemaining_ = 0.0f;
    attackRemaining_ = 0.0f;
    movesTaken_ = 0;
    movesBlocked_ = 0;
}

bool ActionGate::tryMove(const CombatTuning& tuning) {
    if (moveRemaining_ > 0.0f) {
        ++movesBlocked_;
        return false;
    }
    moveRemaining_ = std::max(0.0f, tuning.playerMoveRecovery);
    ++movesTaken_;
    return true;
}

bool ActionGate::tryTurn(const CombatTuning& tuning) {
    if (turnRemaining_ > 0.0f) return false;
    turnRemaining_ = std::max(0.0f, tuning.playerTurnRecovery);
    return true;
}

bool ActionGate::tryAttack(const CombatTuning& tuning) {
    if (attackRemaining_ > 0.0f) return false;
    attackRemaining_ = std::max(0.0f, tuning.playerAttackRecovery);
    return true;
}

} // namespace sv
