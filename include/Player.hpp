#pragma once

#include <utility>

namespace sv {

class PlayerState {
public:
    static constexpr int DirectionCount = 4;

    PlayerState() = default;
    PlayerState(int x, int y, int direction);

    int x() const { return x_; }
    int y() const { return y_; }
    int direction() const { return direction_; }

    // The simulation owns an integer grid cell. Rendering always observes the
    // center of that cell so collision and the camera share one coordinate model.
    double eyeX() const { return static_cast<double>(x_) + 0.5; }
    double eyeY() const { return static_cast<double>(y_) + 0.5; }

    std::pair<int, int> movementTarget(int forward, int strafe) const;
    std::pair<int, int> frontCell(int distance = 1) const;

    void moveTo(int x, int y);
    void turn(int delta);
    bool restore(int x, int y, int direction);

    static int directionX(int direction);
    static int directionY(int direction);

private:
    int x_{2};
    int y_{2};
    int direction_{1};
};

} // namespace sv
