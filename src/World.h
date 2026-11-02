#pragma once
#include "system.h"
#include "SpikeActivity.h"
#include <string>
#include <vector>

namespace game {
inline constexpr int ScreenWidth = 960;
inline constexpr int ScreenHeight = 640;
inline constexpr float FixedStep = 1.0f / 120.0f;

struct Rect {
    float x, y, w, h;
    float right() const { return x + w; }
    float bottom() const { return y + h; }
};
bool overlaps(const Rect& a, const Rect& b);

struct Input { bool left = false, right = false, jump = false; };
struct Player {
    Rect body{60, 522, 24, 28};
    float vx = 0, vy = 0;
    int jumpsLeft = 2;
    bool grounded = false;
};
struct Level {
    std::vector<Rect> platforms;
    std::vector<Rect> spikes;
    Rect checkpoint;
    Rect goal;
    float spawnX, spawnY;
    std::vector<int> spikeScores;
    std::vector<int> platformScores;
    float width = 2240;
    static Level tutorial();
};

// Pure game rules: independent of EasyX, so physics can be tested without a window.
class World {
public:
    Level level = Level::tutorial();
    Player player;
    int deaths = 0;
    bool checkpointActive = false;
    bool won = false;
    bool dead = false;
    std::wstring deathReason;
    ScoreSystem scoring;
    SpikeActivity spikeActivity = SpikeActivity::tutorial(level.spikes);
    float elapsed = 0;

    World();
    void update(const Input& input, float dt);
    void respawn();
    void die(const wchar_t* reason);
    void restart();
    float cameraX() const;
};
int runSelfTests();
}
