// 积分与结算实现：发放一次性奖励、冻结死亡分数，并绘制可点击的结算界面。
#include "system.h"
#include "World.h"
#include "DisplayLayout.h"
// 仅在第三方 EasyX 头文件范围内关闭匿名结构体警告，不影响项目自身检查。
#pragma warning(push)
#pragma warning(disable: 4201)
#include <graphics.h>
#pragma warning(pop)
#include <algorithm>
#include <cmath>
#include <string>

namespace game {
void ScoreSystem::award(std::size_t id, int points) {
    // 先拒绝无分值或已计分的障碍，再登记编号并累加奖励。
    if (points <= 0 || std::find(awarded.begin(), awarded.end(), id) != awarded.end()) return;
    awarded.push_back(id);
    score += points;
}

void ScoreSystem::update(const Player& before, const Player& after, const Level& level) {
    // 第一步：逐枚检测尖刺的向右跨越，分值按原始尖刺索引读取。
    for (std::size_t i = 0; i < level.spikes.size(); ++i) {
        const auto& spike = level.spikes[i];
        // 玩家左边缘跨过尖刺右边缘才算整个人通过；从底座下方经过不计分。
        if (before.body.x < spike.right() && after.body.x >= spike.right() &&
            after.body.bottom() <= spike.bottom()) {
            const int points = i < level.spikeScores.size() ? level.spikeScores[i] : 100;
            award(i, points);
        }
    }
    // 第二步：检测由空中进入落地的瞬间，避免站在平台上每帧重复得分。
    for (std::size_t i = 0; i < level.platforms.size(); ++i) {
        const auto& platform = level.platforms[i];
        // 确认脚底确实落到这个平台，再使用与尖刺不重叠的编号发分。
        if (!before.grounded && after.grounded && after.body.right() > platform.x &&
            after.body.x < platform.right() && std::abs(after.body.bottom() - platform.y) < 0.1f) {
            const int points = i < level.platformScores.size() ? level.platformScores[i] : 0;
            // 使用固定编号区间，无尽模式新增尖刺时不会改变旧平台的奖励编号。
            award((std::size_t{1} << 30) + i, points);
        }
    }
    // 第三步：安全从倒挂尖刺下方穿过同样获得奖励，夹击上排与地面合并计分。
    for (std::size_t i = 0; i < level.overheadTraps.size(); ++i) {
        const auto& trap = level.overheadTraps[i];
        if (before.body.x < trap.body.right() && after.body.x >= trap.body.right() && after.body.y >= trap.body.bottom())
            award((std::size_t{1} << 31) + i, trap.points);
    }
}
// 死亡冻结显示分数；只有重新开始才同时清除累计分数和已计分记录。
void ScoreSystem::settleDeath() { finalScore = score; }
void ScoreSystem::reset() { score = 0; finalScore = 0; coinCount = 0; awarded.clear(); }
void ScoreSystem::collectCoin(std::size_t index) {
    // 金币使用独立编号，重复触碰或错误的重复调用都不能发第二次奖励。
    const int before = score;
    award(std::size_t{0xE0000000} + index, CoinPoints);
    if (score > before) ++coinCount;
}
void ScoreSystem::spendRespawn() { score = std::max(0, score - RespawnCost); }
void ScoreSystem::defeatEnemy(std::size_t index) { award(std::size_t{0xC0000000} + index, 200); }
bool ScoreSystem::hasDefeatedEnemy() const {
    return std::any_of(awarded.begin(), awarded.end(), [](std::size_t id) { return id >= 0xC0000000 && id < 0xD0000000; });
}
bool ScoreSystem::hasDefeatedBoss() const {
    return std::find(awarded.begin(), awarded.end(), std::size_t{0xFFFFFFFE}) != awarded.end();
}
void ScoreSystem::defeatBoss() { award(std::size_t{0xFFFFFFFE}, 1000); }
void ScoreSystem::collectFeather(std::size_t index) { award(std::size_t{0xD0000000} + index, 500); }

namespace {
// 绘制和鼠标命中共用两块按钮区域；坐标属于屏幕，不受镜头影响。
constexpr Rect RespawnButton{280, 342, 400, 54};
constexpr Rect RestartButton{280, 412, 400, 54};
bool contains(const Rect& r, int x, int y) {
    // 检查屏幕鼠标坐标是否位于按钮内部。
    return x >= r.x && x < r.right() && y >= r.y && y < r.bottom();
}
void centered(int y, const std::wstring& text, int size, COLORREF color) {
    // 先测量文字宽度确定居中位置，再绘制偏移阴影和正文。
    settextstyle(size, 0, L"Microsoft YaHei");
    const int x = (ScreenWidth - textwidth(text.c_str())) / 2;
    settextcolor(RGB(32, 16, 17));
    outtextxy(x + 3, y + 3, text.c_str());
    settextcolor(color);
    outtextxy(x, y, text.c_str());
}
void button(const Rect& r, const wchar_t* text, POINT mouse, bool enabled = true) {
    // 第一步：转换绘图坐标并检查悬停，用不同填充色提供鼠标反馈。
    const int x = static_cast<int>(r.x), y = static_cast<int>(r.y);
    const int right = static_cast<int>(r.right()), bottom = static_cast<int>(r.bottom());
    const bool hover = enabled && contains(r, mouse.x, mouse.y);
    // 第二步：绘制外框、底色和明暗边缘，形成方块风格的立体按钮。
    setfillcolor(RGB(20, 20, 20));
    solidrectangle(x - 3, y - 3, right + 3, bottom + 3);
    setfillcolor(!enabled ? RGB(55, 52, 57) : (hover ? RGB(133, 143, 164) : RGB(99, 99, 105)));
    solidrectangle(x, y, right, bottom);
    setlinecolor(hover ? RGB(221, 230, 250) : RGB(173, 173, 179));
    line(x, y, right - 1, y); line(x, y, x, bottom - 1);
    setlinecolor(RGB(51, 51, 57));
    line(x + 1, bottom, right, bottom); line(right, y + 1, right, bottom);
    // 第三步：在按钮中放置标题及快捷键，悬停时高亮文字。
    centered(y + 13, text, 22, !enabled ? RGB(136, 126, 131) : (hover ? RGB(255, 255, 180) : RGB(240, 240, 240)));
}
}

DeathAction deathScreenClick(int x, int y, bool canRespawn) {
    // 按命中区域返回动作，点击按钮外的地方不改变游戏状态。
    if (canRespawn && contains(RespawnButton, x, y)) return DeathAction::Respawn;
    if (contains(RestartButton, x, y)) return DeathAction::Restart;
    return DeathAction::None;
}

void drawDeathScreen(const World& world) {
    // 第一步：给当前帧叠加暗红色调，保留死亡地点作为结算背景。
    DWORD* pixels = GetImageBuffer(GetWorkingImage());
    for (int i = 0; i < ScreenWidth * ScreenHeight; ++i) {
        const DWORD p = pixels[i];
        const DWORD red = ((p >> 16) & 255) / 3 + 55;
        const DWORD green = ((p >> 8) & 255) / 4;
        const DWORD blue = (p & 255) / 4;
        pixels[i] = (red << 16) | (green << 8) | blue;
    }
    // 第二步：透明绘制死亡原因、冻结分数、障碍数和死亡次数。
    setbkmode(TRANSPARENT);
    centered(145, world.canRespawn() ? L"你死了！" : L"体力耗尽", 56, RGB(255, 255, 255));
    centered(218, world.deathReason, 20, RGB(232, 205, 205));
    centered(257, L"分数：" + std::to_wstring(world.scoring.deathScore()), 30, RGB(255, 230, 90));
    centered(302, L"金币 " + std::to_wstring(world.scoring.coinsCollected()) +
        L"    体力 " + std::to_wstring(world.stamina) + L" / 10    死亡 " + std::to_wstring(world.deaths), 18, RGB(231, 220, 220));
    // 第三步：将鼠标从桌面坐标转成窗口坐标，绘制按钮与重生规则提示。
    POINT mouse{};
    GetCursorPos(&mouse);
    ScreenToClient(GetHWnd(), &mouse);
    // 窗口可缩放，按钮布局仍使用 960×640 逻辑坐标。
    RECT client{};
    GetClientRect(GetHWnd(), &client);
    int logicalX = -1, logicalY = -1;
    if (client.right > 0 && client.bottom > 0)
        fitViewport(client.right, client.bottom).mapPoint(mouse.x, mouse.y, logicalX, logicalY);
    mouse.x = logicalX; mouse.y = logicalY;
    button(RespawnButton, world.canRespawn() ? L"重生  -1 体力 / -100 分  [Enter / R]" : L"体力不足，无法重生", mouse, world.canRespawn());
    button(RestartButton, L"重新开始  [Shift + R]", mouse);
    centered(491, world.canRespawn() ? L"重生扣分最低为 0 · 已吃金币不刷新 · 保留存档点" : L"本次挑战结束，请重新开始本关", 17, RGB(220, 197, 197));
    centered(520, L"重新开始：分数清零、金币恢复、体力重置为 10", 17, RGB(220, 197, 197));
}
}
