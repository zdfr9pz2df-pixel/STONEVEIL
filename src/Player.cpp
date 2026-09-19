#include "Player.hpp"

namespace sv {
namespace {
constexpr int DirectionX[PlayerState::DirectionCount] = {0, 1, 0, -1};
constexpr int DirectionY[PlayerState::DirectionCount] = {-1, 0, 1, 0};
}

PlayerState::PlayerState(int x, int y, int direction) {
    restore(x, y, direction);
}

std::pair<int, int> PlayerState::movementTarget(int forward, int strafe) const {
    const int rightDirection = (direction_ + 1) % DirectionCount;
    return {
        x_ + directionX(direction_) * forward + directionX(rightDirection) * strafe,
        y_ + directionY(direction_) * forward + directionY(rightDirection) * strafe,
    };
}

std::pair<int, int> PlayerState::frontCell(int distance) const {
    return {
        x_ + directionX(direction_) * distance,
        y_ + directionY(direction_) * distance,
    };
}

void PlayerState::moveTo(int x, int y) {
    x_ = x;
    y_ = y;
}

void PlayerState::turn(int delta) {
    direction_ = (direction_ + delta) % DirectionCount;
    if (direction_ < 0) direction_ += DirectionCount;
}

bool PlayerState::restore(int x, int y, int direction) {
    if (direction < 0 || direction >= DirectionCount) return false;
    x_ = x;
    y_ = y;
    direction_ = direction;
    return true;
}

int PlayerState::directionX(int direction) {
    return direction >= 0 && direction < DirectionCount ? DirectionX[direction] : 0;
}

int PlayerState::directionY(int direction) {
    return direction >= 0 && direction < DirectionCount ? DirectionY[direction] : 0;
}

} // namespace sv
