#include "Game.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace sv {
namespace {
constexpr int ScreenW = 1280;
constexpr int ScreenH = 720;
constexpr int ViewW = 930;
constexpr int ViewH = 560;
constexpr int ViewY = 36;
constexpr float FovScale = 0.66f;

constexpr int DX[4] = {0, 1, 0, -1};
constexpr int DY[4] = {-1, 0, 1, 0};

Color wallColor(Tile t, bool side) {
    Color c = (t == Tile::DoorClosed) ? Color{126, 87, 46, 255} : Color{92, 103, 112, 255};
    if (side) {
        c.r = static_cast<unsigned char>(c.r * 0.72f);
        c.g = static_cast<unsigned char>(c.g * 0.72f);
        c.b = static_cast<unsigned char>(c.b * 0.72f);
    }
    return c;
}
}

Game::Game() {
    InitWindow(ScreenW, ScreenH, "STONEVEIL v0.1");
    SetTargetFPS(60);
    reset();
    mode_ = Mode::Title;
}

void Game::reset() {
    dungeon_ = Dungeon{};
    px_ = 2;
    py_ = 2;
    dir_ = 1;
    party_ = {{{"Vanguard", 42, 42, 9}, {"Ranger", 32, 32, 7}, {"Mystic", 27, 27, 6}}};
    keys_ = 0;
    potions_ = 1;
    xp_ = 0;
    attackCooldown_ = 0.0f;
    message_.clear();
    messageTimer_ = 0.0f;
}

void Game::run() {
    while (!WindowShouldClose()) {
        update(GetFrameTime());
        draw();
    }
    CloseWindow();
}

void Game::update(float dt) {
    if (messageTimer_ > 0.0f) messageTimer_ -= dt;
    if (attackCooldown_ > 0.0f) attackCooldown_ -= dt;

    if (mode_ == Mode::Title) {
        if (IsKeyPressed(KEY_ENTER)) {
            reset();
            mode_ = Mode::Playing;
        }
        if (IsKeyPressed(KEY_L) && load()) mode_ = Mode::Playing;
        return;
    }

    if (mode_ == Mode::Victory || mode_ == Mode::Defeat) {
        if (IsKeyPressed(KEY_ENTER)) {
            reset();
            mode_ = Mode::Playing;
        }
        if (IsKeyPressed(KEY_ESCAPE)) mode_ = Mode::Title;
        return;
    }

    updatePlaying(dt);
}

void Game::updatePlaying(float dt) {
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) move(1, 0);
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) move(-1, 0);
    if (IsKeyPressed(KEY_A)) move(0, -1);
    if (IsKeyPressed(KEY_D)) move(0, 1);
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_Q)) turn(-1);
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_E)) turn(1);
    if (IsKeyPressed(KEY_SPACE)) attack();
    if (IsKeyPressed(KEY_F)) interact();
    if (IsKeyPressed(KEY_H)) drinkPotion();
    if (IsKeyPressed(KEY_F5)) save();
    if (IsKeyPressed(KEY_F9)) load();
    if (IsKeyPressed(KEY_ESCAPE)) mode_ = Mode::Title;

    updateEnemies(dt);

    bool anyoneAlive = false;
    for (const auto& member : party_) anyoneAlive = anyoneAlive || member.alive();
    if (!anyoneAlive) mode_ = Mode::Defeat;
    if (dungeon_.tile(px_, py_) == Tile::Exit) mode_ = Mode::Victory;
}

void Game::move(int forward, int strafe) {
    const int rightDir = (dir_ + 1) % 4;
    const int nx = px_ + DX[dir_] * forward + DX[rightDir] * strafe;
    const int ny = py_ + DY[dir_] * forward + DY[rightDir] * strafe;
    if (dungeon_.blocksMovement(nx, ny) || enemyAt(nx, ny)) {
        setMessage("Something blocks the way.", 1.0f);
        return;
    }
    px_ = nx;
    py_ = ny;
    collectPickup();
}

void Game::turn(int delta) {
    dir_ = (dir_ + delta + 4) % 4;
}

void Game::interact() {
    const int tx = px_ + DX[dir_];
    const int ty = py_ + DY[dir_];
    if (dungeon_.tile(tx, ty) == Tile::DoorClosed) {
        if (keys_ <= 0) {
            setMessage("The iron lock needs a key.");
            return;
        }
        if (dungeon_.openDoor(tx, ty, true)) {
            --keys_;
            setMessage("The lock gives. The door opens.");
        }
        return;
    }
    setMessage("Nothing here responds.", 1.0f);
}

void Game::attack() {
    if (attackCooldown_ > 0.0f) return;
    attackCooldown_ = 0.55f;
    const int index = frontEnemyIndex(1);
    if (index < 0) {
        setMessage("Your weapons cut empty air.", 1.0f);
        return;
    }

    int damage = 0;
    for (const auto& member : party_) if (member.alive()) damage += member.power;
    damage = std::max(1, damage / 2 + GetRandomValue(0, 5));
    auto& enemy = dungeon_.enemies()[static_cast<size_t>(index)];
    enemy.hp -= damage;
    if (enemy.hp <= 0) {
        enemy.alive = false;
        xp_ += 25;
        setMessage("Enemy felled. +25 XP");
    } else {
        setMessage("Party strikes for " + std::to_string(damage) + ".", 1.2f);
    }
}

void Game::drinkPotion() {
    if (potions_ <= 0) {
        setMessage("No healing draughts remain.");
        return;
    }
    auto it = std::min_element(party_.begin(), party_.end(), [](const PartyMember& a, const PartyMember& b) {
        const float ar = a.maxHp > 0 ? static_cast<float>(a.hp) / a.maxHp : 1.0f;
        const float br = b.maxHp > 0 ? static_cast<float>(b.hp) / b.maxHp : 1.0f;
        return ar < br;
    });
    if (it == party_.end() || it->hp >= it->maxHp) {
        setMessage("No one needs healing.");
        return;
    }
    it->hp = std::min(it->maxHp, it->hp + 18);
    --potions_;
    setMessage(it->name + " drinks a healing draught.");
}

void Game::collectPickup() {
    for (auto& pickup : dungeon_.pickups()) {
        if (!pickup.taken && pickup.x == px_ && pickup.y == py_) {
            pickup.taken = true;
            if (pickup.type == Pickup::Type::Key) {
                ++keys_;
                setMessage("You found an iron key.");
            } else {
                ++potions_;
                setMessage("You found a healing draught.");
            }
        }
    }
}

bool Game::enemyAt(int x, int y, int ignoreIndex) const {
    for (size_t i = 0; i < dungeon_.enemies().size(); ++i) {
        const auto& enemy = dungeon_.enemies()[i];
        if (static_cast<int>(i) != ignoreIndex && enemy.alive && enemy.x == x && enemy.y == y) return true;
    }
    return false;
}

int Game::frontEnemyIndex(int maxDistance) const {
    int x = px_;
    int y = py_;
    for (int d = 1; d <= maxDistance; ++d) {
        x += DX[dir_];
        y += DY[dir_];
        if (dungeon_.blocksSight(x, y)) return -1;
        for (size_t i = 0; i < dungeon_.enemies().size(); ++i) {
            const auto& enemy = dungeon_.enemies()[i];
            if (enemy.alive && enemy.x == x && enemy.y == y) return static_cast<int>(i);
        }
    }
    return -1;
}

void Game::updateEnemies(float dt) {
    auto& enemies = dungeon_.enemies();
    for (size_t i = 0; i < enemies.size(); ++i) {
        auto& enemy = enemies[i];
        if (!enemy.alive) continue;
        enemy.attackCooldown = std::max(0.0f, enemy.attackCooldown - dt);
        const int distance = std::abs(enemy.x - px_) + std::abs(enemy.y - py_);

        if (distance == 1 && enemy.attackCooldown <= 0.0f) {
            enemy.attackCooldown = 1.25f;
            std::vector<int> living;
            for (int p = 0; p < static_cast<int>(party_.size()); ++p) if (party_[p].alive()) living.push_back(p);
            if (!living.empty()) {
                const int target = living[static_cast<size_t>(GetRandomValue(0, static_cast<int>(living.size()) - 1))];
                const int damage = GetRandomValue(3, 8);
                party_[target].hp = std::max(0, party_[target].hp - damage);
                setMessage(party_[target].name + " takes " + std::to_string(damage) + " damage.", 1.0f);
            }
            continue;
        }

        if (distance > 7 || enemy.attackCooldown > 0.5f) continue;
        int nx = enemy.x;
        int ny = enemy.y;
        const int stepX = (px_ > enemy.x) - (px_ < enemy.x);
        const int stepY = (py_ > enemy.y) - (py_ < enemy.y);
        if (std::abs(px_ - enemy.x) >= std::abs(py_ - enemy.y)) nx += stepX;
        else ny += stepY;

        if (!dungeon_.blocksMovement(nx, ny) && !(nx == px_ && ny == py_) && !enemyAt(nx, ny, static_cast<int>(i))) {
            enemy.x = nx;
            enemy.y = ny;
            enemy.attackCooldown = 0.65f;
        }
    }
}

bool Game::save() const {
    std::ofstream out("stoneveil.sav", std::ios::trunc);
    if (!out) return false;
    out << px_ << ' ' << py_ << ' ' << dir_ << ' ' << keys_ << ' ' << potions_ << ' ' << xp_ << '\n';
    for (const auto& p : party_) out << p.hp << ' ';
    out << '\n';
    for (const auto& e : dungeon_.enemies()) out << e.x << ' ' << e.y << ' ' << e.hp << ' ' << e.alive << '\n';
    return true;
}

bool Game::load() {
    std::ifstream in("stoneveil.sav");
    if (!in) {
        setMessage("No save file found.");
        return false;
    }
    reset();
    in >> px_ >> py_ >> dir_ >> keys_ >> potions_ >> xp_;
    for (auto& p : party_) in >> p.hp;
    for (auto& e : dungeon_.enemies()) in >> e.x >> e.y >> e.hp >> e.alive;
    setMessage("Save loaded.");
    return static_cast<bool>(in);
}

void Game::setMessage(std::string message, float seconds) {
    message_ = std::move(message);
    messageTimer_ = seconds;
}

void Game::draw() const {
    BeginDrawing();
    ClearBackground(Color{15, 16, 18, 255});
    if (mode_ == Mode::Title) drawTitle();
    else if (mode_ == Mode::Victory) drawEndScreen(true);
    else if (mode_ == Mode::Defeat) drawEndScreen(false);
    else {
        drawWorld();
        drawHud();
    }
    EndDrawing();
}

void Game::drawWorld() const {
    DrawRectangle(24, ViewY, ViewW, ViewH / 2, Color{29, 31, 37, 255});
    DrawRectangle(24, ViewY + ViewH / 2, ViewW, ViewH / 2, Color{43, 37, 31, 255});

    const double dirX = static_cast<double>(DX[dir_]);
    const double dirY = static_cast<double>(DY[dir_]);
    const double planeX = -dirY * FovScale;
    const double planeY = dirX * FovScale;

    for (int x = 0; x < ViewW; ++x) {
        const double cameraX = 2.0 * x / static_cast<double>(ViewW) - 1.0;
        const double rayDirX = dirX + planeX * cameraX;
        const double rayDirY = dirY + planeY * cameraX;
        int mapX = px_;
        int mapY = py_;
        const double deltaX = rayDirX == 0.0 ? 1e30 : std::abs(1.0 / rayDirX);
        const double deltaY = rayDirY == 0.0 ? 1e30 : std::abs(1.0 / rayDirY);
        double sideDistX{};
        double sideDistY{};
        int stepX{};
        int stepY{};
        if (rayDirX < 0) { stepX = -1; sideDistX = (px_ - mapX) * deltaX; }
        else { stepX = 1; sideDistX = (mapX + 1.0 - px_) * deltaX; }
        if (rayDirY < 0) { stepY = -1; sideDistY = (py_ - mapY) * deltaY; }
        else { stepY = 1; sideDistY = (mapY + 1.0 - py_) * deltaY; }

        bool side = false;
        Tile hitTile = Tile::Wall;
        for (int guard = 0; guard < 64; ++guard) {
            if (sideDistX < sideDistY) { sideDistX += deltaX; mapX += stepX; side = false; }
            else { sideDistY += deltaY; mapY += stepY; side = true; }
            hitTile = dungeon_.tile(mapX, mapY);
            if (hitTile == Tile::Wall || hitTile == Tile::DoorClosed) break;
        }

        const double dist = side ? sideDistY - deltaY : sideDistX - deltaX;
        const int lineH = static_cast<int>(ViewH / std::max(0.05, dist));
        const int start = ViewY + std::max(0, (ViewH - lineH) / 2);
        const int end = ViewY + std::min(ViewH - 1, (ViewH + lineH) / 2);
        DrawLine(24 + x, start, 24 + x, end, wallColor(hitTile, side));
    }

    const int enemyIndex = frontEnemyIndex(6);
    if (enemyIndex >= 0) {
        const auto& enemy = dungeon_.enemies()[static_cast<size_t>(enemyIndex)];
        const int dist = std::max(1, std::abs(enemy.x - px_) + std::abs(enemy.y - py_));
        const int size = std::clamp(300 / dist, 55, 280);
        const int cx = 24 + ViewW / 2;
        const int cy = ViewY + ViewH / 2 + 45;
        DrawRectangle(cx - size / 2, cy - size, size, size, Color{108, 28, 30, 255});
        DrawRectangleLines(cx - size / 2, cy - size, size, size, Color{224, 169, 111, 255});
        DrawText(TextFormat("FOE %d HP", enemy.hp), cx - 48, cy - size - 24, 18, RAYWHITE);
    }

    DrawRectangleLines(24, ViewY, ViewW, ViewH, Color{139, 123, 89, 255});
}

void Game::drawHud() const {
    const int panelX = 978;
    DrawText("STONEVEIL", panelX, 38, 30, Color{221, 196, 139, 255});
    DrawText(TextFormat("KEYS %d   DRAUGHTS %d", keys_, potions_), panelX, 84, 18, LIGHTGRAY);
    DrawText(TextFormat("XP %d", xp_), panelX, 108, 18, LIGHTGRAY);

    int y = 154;
    for (const auto& p : party_) {
        DrawRectangle(panelX, y, 272, 92, Color{27, 29, 32, 255});
        DrawRectangleLines(panelX, y, 272, 92, Color{91, 83, 65, 255});
        DrawText(p.name.c_str(), panelX + 12, y + 10, 21, RAYWHITE);
        DrawText(TextFormat("HP %d / %d", p.hp, p.maxHp), panelX + 12, y + 39, 18, p.alive() ? Color{170, 212, 151, 255} : Color{190, 70, 70, 255});
        const float ratio = p.maxHp ? static_cast<float>(p.hp) / p.maxHp : 0.0f;
        DrawRectangle(panelX + 12, y + 65, 240, 10, Color{48, 45, 42, 255});
        DrawRectangle(panelX + 12, y + 65, static_cast<int>(240 * std::clamp(ratio, 0.0f, 1.0f)), 10, Color{135, 52, 48, 255});
        y += 104;
    }

    DrawText("W/S move   A/D strafe", panelX, 492, 16, GRAY);
    DrawText("Q/E or arrows turn", panelX, 514, 16, GRAY);
    DrawText("SPACE attack   F interact", panelX, 536, 16, GRAY);
    DrawText("H heal   F5 save   F9 load", panelX, 558, 16, GRAY);

    if (messageTimer_ > 0.0f && !message_.empty()) {
        DrawRectangle(24, 610, ViewW, 74, Color{11, 12, 14, 235});
        DrawRectangleLines(24, 610, ViewW, 74, Color{91, 83, 65, 255});
        DrawText(message_.c_str(), 44, 635, 21, Color{224, 217, 194, 255});
    }
}

void Game::drawTitle() const {
    DrawText("STONEVEIL", 430, 190, 64, Color{220, 193, 134, 255});
    DrawText("A SYSTEMS-FIRST DUNGEON CRAWLER", 417, 272, 22, LIGHTGRAY);
    DrawText("ENTER  descend", 545, 380, 22, RAYWHITE);
    DrawText("L      load save", 545, 416, 22, RAYWHITE);
    DrawText("Early prototype - placeholder presentation", 433, 520, 18, GRAY);
}

void Game::drawEndScreen(bool won) const {
    const char* title = won ? "THE VEIL OPENS" : "THE PARTY HAS FALLEN";
    const Color color = won ? Color{211, 187, 126, 255} : Color{170, 66, 64, 255};
    DrawText(title, 390, 240, 46, color);
    DrawText(won ? "You reached the first prototype exit." : "The dungeon keeps what it kills.", 420, 318, 21, LIGHTGRAY);
    DrawText("ENTER restart    ESC title", 480, 410, 20, RAYWHITE);
}

} // namespace sv
