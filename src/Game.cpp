#include "Game.h"
#pragma warning(push)
#pragma warning(disable: 4201) // EasyX message structures use anonymous unions.
#include <graphics.h>
#pragma warning(pop)
#include <algorithm>
#include <chrono>
#include <string>

namespace game {
namespace {
void box(const Rect& r, COLORREF color) {
    setfillcolor(color);
    solidrectangle(static_cast<int>(r.x), static_cast<int>(r.y),
        static_cast<int>(r.right()) - 1, static_cast<int>(r.bottom()) - 1);
}
void label(int x, int y, const std::wstring& text, int size, COLORREF color) {
    settextstyle(size, 0, L"Microsoft YaHei");
    settextcolor(color);
    outtextxy(x, y, text.c_str());
}
bool down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }
}

int Game::run(bool smokeTest, bool deathPreview) {
    const HWND window = initgraph(ScreenWidth, ScreenHeight);
    if (!window) return 1;
    SetWindowTextW(window, L"game | EasyX 平台跳跃原型");
    setbkmode(TRANSPARENT);
    BeginBatchDraw();
    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    double accumulator = 0;
    bool previousJump = false, previousPause = false, previousReset = false;
    bool previousEnter = false;
    bool pendingJump = false;
    int frames = 0;
    if (deathPreview) {
        // Exercise real scoring before capturing a death screen with a nonzero score.
        for (int i = 0; i < 20; ++i) world.update({false, true, false}, FixedStep);
        for (int i = 0; i < 180; ++i) {
            world.update({false, world.player.body.x < 365, i == 0 || i == 36}, FixedStep);
            if (i > 36 && world.player.grounded) break;
        }
        world.die(L"你坠入了深渊");
    }

    while (IsWindow(window)) {
        const auto now = Clock::now();
        const double frameTime = std::min(std::chrono::duration<double>(now - previous).count(), 0.1);
        previous = now;
        const bool focused = GetForegroundWindow() == window;
        DeathAction action = DeathAction::None;
        ExMessage message;
        while (peekmessage(&message)) {
            if (focused && world.dead && message.message == WM_LBUTTONDOWN)
                action = deathScreenClick(message.x, message.y);
        }
        if (focused && down(VK_ESCAPE)) break;
        const bool jump = focused && (down(VK_SPACE) || down('Z'));
        const bool pause = focused && down('P');
        const bool reset = focused && down('R');
        const bool enter = focused && down(VK_RETURN);
        if (pause && !previousPause && !world.dead && !world.won) { paused = !paused; pendingJump = false; }
        if (reset && !previousReset) {
            if (world.won || down(VK_SHIFT)) action = DeathAction::Restart;
            else if (world.dead) action = DeathAction::Respawn;
            else world.die(L"你选择了结束本次挑战");
            pendingJump = false;
        }
        if (world.dead && enter && !previousEnter) action = DeathAction::Respawn;
        if (action != DeathAction::None) {
            if (action == DeathAction::Restart) world.restart();
            else world.respawn();
            paused = false;
            pendingJump = false;
            accumulator = 0;
        }
        if (jump && !previousJump && !paused && focused && !world.dead && action == DeathAction::None) pendingJump = true;
        previousJump = jump;
        previousPause = pause;
        previousReset = reset;
        previousEnter = enter;
        if (focused && !paused && !world.dead) {
            accumulator += frameTime;
            while (accumulator >= FixedStep) {
                world.update({down('A') || down(VK_LEFT), down('D') || down(VK_RIGHT), pendingJump}, FixedStep);
                pendingJump = false;
                accumulator -= FixedStep;
            }
        } else { accumulator = 0; pendingJump = false; }
        render();
        if (!focused && !paused && !world.won && !world.dead)
            label(340, 280, L"点击游戏窗口继续", 26, RGB(255, 221, 132));
        FlushBatchDraw();
        if (smokeTest && ++frames >= 3) { saveimage(deathPreview ? L"death-smoke-test.png" : L"smoke-test.png"); break; }
        Sleep(1);
    }
    if (IsWindow(window)) { EndBatchDraw(); closegraph(); }
    return 0;
}

void Game::render() const {
    setbkcolor(RGB(15, 22, 38));
    cleardevice();
    setlinecolor(RGB(25, 36, 54));
    for (int x = 0; x < ScreenWidth; x += 40) line(x, 125, x, ScreenHeight);
    for (int y = 150; y < ScreenHeight; y += 40) line(0, y, ScreenWidth, y);
    box({0, 0, 960, 110}, RGB(22, 32, 51));
    label(28, 16, L"GAME / 初始试炼", 28, RGB(239, 245, 255));
    label(28, 56, L"A D / ← → 移动    空格 / Z 跳跃、二段跳", 18, RGB(153, 178, 202));
    label(28, 81, L"R 重生    Shift+R 重开    P 暂停    Esc 退出", 16, RGB(153, 178, 202));
    label(650, 22, L"死亡 " + std::to_wstring(world.deaths) + L"    时间 " +
        std::to_wstring(static_cast<int>(world.elapsed)) + L"s", 21, RGB(239, 245, 255));
    label(650, 60, world.checkpointActive ? L"存档点已激活" : L"前往绿色旗帜存档", 18, RGB(92, 221, 167));
    label(650, 83, L"积分 " + std::to_wstring(world.scoring.total()) + L"    障碍 " +
        std::to_wstring(world.scoring.cleared()), 18, RGB(255, 230, 90));
    for (const auto& tile : world.level.platforms) {
        box(tile, RGB(52, 71, 94));
        box({tile.x, tile.y, tile.w, 4}, RGB(127, 165, 194));
    }
    for (const auto& spike : world.level.spikes) {
        POINT points[3] = {{static_cast<LONG>(spike.x), static_cast<LONG>(spike.bottom())},
            {static_cast<LONG>(spike.x + spike.w / 2), static_cast<LONG>(spike.y)},
            {static_cast<LONG>(spike.right()), static_cast<LONG>(spike.bottom())}};
        setfillcolor(RGB(255, 105, 128));
        solidpolygon(points, 3);
    }
    const auto& flag = world.level.checkpoint;
    box({flag.x, flag.y, 3, flag.h}, RGB(177, 207, 215));
    box({flag.x + 3, flag.y, 24, 17}, world.checkpointActive ? RGB(107, 255, 175) : RGB(55, 151, 115));
    box(world.level.goal, RGB(234, 183, 80));
    box({world.level.goal.x + 5, world.level.goal.y + 5, 22, 47}, RGB(76, 61, 42));
    label(820, 280, L"终点", 18, RGB(255, 221, 132));
    const auto& p = world.player.body;
    box(p, RGB(95, 213, 255));
    box({p.x + 4, p.y + 5, 16, 8}, RGB(224, 247, 255));
    box({p.x + 15, p.y + 7, 3, 4}, RGB(20, 47, 69));
    label(32, 590, L"避开尖刺，利用二段跳跨越空隙，到达右上方出口。", 18, RGB(153, 178, 202));
    if (world.dead) { drawDeathScreen(world); return; }
    if (paused || world.won) {
        box({235, 190, 490, 140}, RGB(30, 45, 67));
        label(320, 215, world.won ? L"通关！基础试炼完成" : L"游戏已暂停", 28, RGB(255, 221, 132));
        label(320, 270, world.won ? L"得分 " + std::to_wstring(world.scoring.total()) + L" · 按 R 再玩一次" : L"按 P 继续", 20, RGB(239, 245, 255));
    }
}
}
