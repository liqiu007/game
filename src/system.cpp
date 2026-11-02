#include "system.h"
#include "World.h"
#pragma warning(push)
#pragma warning(disable: 4201)
#include <graphics.h>
#pragma warning(pop)
#include <algorithm>
#include <cmath>
#include <string>

namespace game {
void ScoreSystem::award(std::size_t id, int points) {
    if (points <= 0 || std::find(awarded.begin(), awarded.end(), id) != awarded.end()) return;
    awarded.push_back(id);
    score += points;
}

void ScoreSystem::update(const Player& before, const Player& after, const Level& level) {
    for (std::size_t i = 0; i < level.spikes.size(); ++i) {
        const auto& spike = level.spikes[i];
        // The whole player must pass the right edge, from above, while alive.
        if (before.body.x < spike.right() && after.body.x >= spike.right() &&
            after.body.bottom() <= spike.bottom()) {
            const int points = i < level.spikeScores.size() ? level.spikeScores[i] : 100;
            award(i, points);
        }
    }
    for (std::size_t i = 0; i < level.platforms.size(); ++i) {
        const auto& platform = level.platforms[i];
        // A gap is cleared only after a safe landing on its destination platform.
        if (!before.grounded && after.grounded && after.body.right() > platform.x &&
            after.body.x < platform.right() && std::abs(after.body.bottom() - platform.y) < 0.1f) {
            const int points = i < level.platformScores.size() ? level.platformScores[i] : 0;
            award(level.spikes.size() + i, points);
        }
    }
}
void ScoreSystem::settleDeath() { finalScore = score; }
void ScoreSystem::reset() { score = 0; finalScore = 0; awarded.clear(); }

namespace {
constexpr Rect RespawnButton{280, 342, 400, 54};
constexpr Rect RestartButton{280, 412, 400, 54};
bool contains(const Rect& r, int x, int y) {
    return x >= r.x && x < r.right() && y >= r.y && y < r.bottom();
}
void centered(int y, const std::wstring& text, int size, COLORREF color) {
    settextstyle(size, 0, L"Microsoft YaHei");
    const int x = (ScreenWidth - textwidth(text.c_str())) / 2;
    settextcolor(RGB(32, 16, 17));
    outtextxy(x + 3, y + 3, text.c_str());
    settextcolor(color);
    outtextxy(x, y, text.c_str());
}
void button(const Rect& r, const wchar_t* text, POINT mouse) {
    const int x = static_cast<int>(r.x), y = static_cast<int>(r.y);
    const int right = static_cast<int>(r.right()), bottom = static_cast<int>(r.bottom());
    const bool hover = contains(r, mouse.x, mouse.y);
    setfillcolor(RGB(20, 20, 20));
    solidrectangle(x - 3, y - 3, right + 3, bottom + 3);
    setfillcolor(hover ? RGB(133, 143, 164) : RGB(99, 99, 105));
    solidrectangle(x, y, right, bottom);
    setlinecolor(hover ? RGB(221, 230, 250) : RGB(173, 173, 179));
    line(x, y, right - 1, y); line(x, y, x, bottom - 1);
    setlinecolor(RGB(51, 51, 57));
    line(x + 1, bottom, right, bottom); line(right, y + 1, right, bottom);
    centered(y + 13, text, 22, hover ? RGB(255, 255, 180) : RGB(240, 240, 240));
}
}

DeathAction deathScreenClick(int x, int y) {
    if (contains(RespawnButton, x, y)) return DeathAction::Respawn;
    if (contains(RestartButton, x, y)) return DeathAction::Restart;
    return DeathAction::None;
}

void drawDeathScreen(const World& world) {
    // Tint the just-rendered scene red, retaining the death location underneath.
    DWORD* pixels = GetImageBuffer();
    for (int i = 0; i < ScreenWidth * ScreenHeight; ++i) {
        const DWORD p = pixels[i];
        const DWORD red = ((p >> 16) & 255) / 3 + 55;
        const DWORD green = ((p >> 8) & 255) / 4;
        const DWORD blue = (p & 255) / 4;
        pixels[i] = (red << 16) | (green << 8) | blue;
    }
    setbkmode(TRANSPARENT);
    centered(145, L"你死了！", 56, RGB(255, 255, 255));
    centered(218, world.deathReason, 20, RGB(232, 205, 205));
    centered(257, L"分数：" + std::to_wstring(world.scoring.deathScore()), 30, RGB(255, 230, 90));
    centered(302, L"已通过障碍 " + std::to_wstring(world.scoring.cleared()) +
        L"    死亡次数 " + std::to_wstring(world.deaths), 18, RGB(231, 220, 220));
    POINT mouse{};
    GetCursorPos(&mouse);
    ScreenToClient(GetHWnd(), &mouse);
    button(RespawnButton, L"重生  [Enter / R]", mouse);
    button(RestartButton, L"重新开始  [Shift + R]", mouse);
    centered(491, L"重生保留积分与存档点 · 重新开始清空本局", 17, RGB(220, 197, 197));
}
}
