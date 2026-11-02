#include "World.h"
#include <algorithm>

namespace game {
bool overlaps(const Rect& a, const Rect& b) {
    return a.x < b.right() && a.right() > b.x && a.y < b.bottom() && a.bottom() > b.y;
}

Level Level::tutorial() {
    // Coordinates use a top-left origin; positive Y points down.
    return {
        {{0, 550, 275, 90}, {340, 490, 155, 24}, {540, 430, 150, 24},
         {735, 365, 165, 24}, {945, 425, 170, 24}, {1160, 480, 180, 24},
         {1390, 415, 180, 24}, {1620, 350, 180, 24}, {1850, 420, 160, 24}, {2055, 360, 185, 24}},
        {{180, 526, 24, 24}, {204, 526, 24, 24}, {625, 406, 24, 24}, {649, 406, 24, 24},
         {1245, 456, 24, 24}, {1269, 456, 24, 24}, {1705, 326, 24, 24}, {1729, 326, 24, 24}},
        {555, 386, 24, 44}, {2170, 308, 32, 52}, 60, 522,
        {100, 100, 100, 100, 100, 100, 100, 100}, {0, 150, 200, 250, 200, 250, 300, 300, 300, 350}, 2240
    };
}
World::World() { respawn(); }
float World::cameraX() const {
    return std::clamp(player.body.x + player.body.w / 2 - ScreenWidth * 0.45f,
        0.0f, std::max(0.0f, level.width - ScreenWidth));
}
void World::respawn() {
    spikeActivity.reset();
    dead = false;
    deathReason.clear();
    player = Player{};
    player.body.x = checkpointActive ? level.checkpoint.x : level.spawnX;
    player.body.y = checkpointActive ? level.checkpoint.bottom() - player.body.h : level.spawnY;
    for (const auto& platform : level.platforms) {
        if (player.body.bottom() == platform.y && player.body.right() > platform.x && player.body.x < platform.right())
            player.grounded = true;
    }
}
void World::die(const wchar_t* reason) {
    if (dead || won) return;
    dead = true;
    deathReason = reason;
    ++deaths;
    scoring.settleDeath();
}
void World::restart() {
    checkpointActive = false;
    won = false;
    elapsed = 0;
    deaths = 0;
    scoring.reset();
    respawn();
}
void World::update(const Input& input, float dt) {
    if (won || dead) return;
    const Player before = player;
    elapsed += dt;
    constexpr float Speed = 250, Gravity = 1550, JumpSpeed = 540;
    player.vx = (static_cast<int>(input.right) - static_cast<int>(input.left)) * Speed;
    if (input.jump && player.jumpsLeft > 0) {
        player.vy = -JumpSpeed;
        --player.jumpsLeft;
        player.grounded = false;
    }
    player.vy = std::min(player.vy + Gravity * dt, 800.0f);

    // Resolve axes separately; the fixed step bounds displacement to prevent tunnelling.
    player.body.x += player.vx * dt;
    for (const auto& tile : level.platforms) {
        if (!overlaps(player.body, tile)) continue;
        if (player.vx > 0) player.body.x = tile.x - player.body.w;
        else if (player.vx < 0) player.body.x = tile.right();
        player.vx = 0;
    }
    player.body.x = std::clamp(player.body.x, 0.0f, std::max(0.0f, level.width - player.body.w));
    player.body.y += player.vy * dt;
    player.grounded = false;
    for (const auto& tile : level.platforms) {
        if (!overlaps(player.body, tile)) continue;
        if (player.vy > 0) {
            player.body.y = tile.y - player.body.h;
            player.grounded = true;
            player.jumpsLeft = 2;
        } else if (player.vy < 0) player.body.y = tile.bottom();
        player.vy = 0;
    }
    // Leaving an edge without jumping still permits one air jump.
    if (!player.grounded && player.vy > 0 && player.jumpsLeft == 2) player.jumpsLeft = 1;

    spikeActivity.update(player.body, level.spikes, dt);
    for (std::size_t i = 0; i < level.spikes.size(); ++i) {
        const auto spike = spikeActivity.bounds(i, level.spikes[i]);
        // A smaller hitbox than the drawn triangle keeps the prototype forgiving.
        const Rect hit{spike.x + 5, spike.y + 6, spike.w - 10, spike.h - 6};
        if (overlaps(player.body, hit)) {
            die(spike.h > level.spikes[i].h + 1 ? L"你被突然伸长的尖刺刺中了" : L"你撞上了尖刺");
            return;
        }
    }
    if (player.body.y > ScreenHeight + 80) { die(L"你坠入了深渊"); return; }
    scoring.update(before, player, level);
    if (overlaps(player.body, level.checkpoint)) checkpointActive = true;
    if (overlaps(player.body, level.goal)) won = true;
}
}
