// 游戏主循环与画面：收集输入、按固定步长更新世界，再绘制当前状态。
#include "Game.h"
#include "Account.h"
#include "StarSystem.h"
#include "Sound.h"
#include "SpecialTraps.h"
#include "SceneRenderer.h"
#include "Controls.h"
#include "DisplayLayout.h"
#include "CharacterRenderer.h"
#pragma warning(push)
#pragma warning(disable: 4201) // EasyX 消息结构使用匿名联合，仅在此头文件内忽略警告。
#include <graphics.h>
#pragma warning(pop)
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <iostream>

namespace game {
namespace {
void box(const Rect& r, COLORREF color) {
    // 将浮点外框转换为像素矩形，右下边缘减一以匹配矩形宽高。
    setfillcolor(color);
    solidrectangle(static_cast<int>(r.x), static_cast<int>(r.y),
        static_cast<int>(r.right()) - 1, static_cast<int>(r.bottom()) - 1);
}
void label(int x, int y, const std::wstring& text, int size, COLORREF color) {
    // 统一设置字体、字号和颜色后输出界面文字。
    settextstyle(size, 0, L"Microsoft YaHei");
    settextcolor(color);
    outtextxy(x, y, text.c_str());
}
// 只读取按键当前是否按住；单次触发由主循环比较上一帧状态实现。
bool down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }
}

int Game::run(Preview preview) {
    // 第一步：创建窗口。正常启动停留主页面，截图模式直接进入指定页面或场景。
    const bool smokeTest = preview != Preview::None;
    // 昵称登录在图形窗口创建后执行。
    int pendingCoins = 0;
    // 正常启动使用时间种子；自动回归和预览保持固定种子，便于复现随机陷阱。
    if (!smokeTest) world = World(static_cast<std::uint32_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    // 使用固定逻辑画布，窗口大小只影响最终缩放，不改变物理坐标。
    int widths[]{960, 1200, 1440, GetSystemMetrics(SM_CXSCREEN)};
    int heights[]{640, 800, 960, GetSystemMetrics(SM_CYSCREEN)};
    int appliedSize = menu.settings.windowSize;
    HWND window = initgraph(widths[appliedSize], heights[appliedSize]);
    IMAGE canvas(ScreenWidth, ScreenHeight);
    if (!window) return 1;
    // 大尺寸若超过当前显示器工作区，保持 3:2 比例缩小，防止系统单独截掉高度。
    MONITORINFO monitor{sizeof(MONITORINFO)};
    monitor.rcMonitor = {0, 0, widths[3], heights[3]};
    RECT outer{}, client{};
    GetWindowRect(window, &outer); GetClientRect(window, &client);
    if (GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
        // 全屏使用整块显示器区域；窗口档仍按不遮挡任务栏的工作区适配。
        widths[3] = monitor.rcMonitor.right - monitor.rcMonitor.left;
        heights[3] = monitor.rcMonitor.bottom - monitor.rcMonitor.top;
        const int maxW = static_cast<int>(monitor.rcWork.right - monitor.rcWork.left - (outer.right - outer.left - client.right));
        const int maxH = static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top - (outer.bottom - outer.top - client.bottom));
        for (int i = 0; i < 3; ++i) {
            const int unit = std::max(1, std::min({widths[i] / 3, heights[i] / 2, maxW / 3, maxH / 2}));
            widths[i] = unit * 3; heights[i] = unit * 2;
        }
        // 起始窗口也可能受小屏幕限制，首次绘制前用适配后的尺寸重新建立。
        if (client.right != widths[appliedSize] || client.bottom != heights[appliedSize]) {
            closegraph(); window = initgraph(widths[appliedSize], heights[appliedSize]);
            if (!window) return 1;
        }
    }
    // 无边框全屏不修改系统分辨率，覆盖当前显示器，仍可正常 Alt+Tab 切换应用。
    const auto applyDisplayStyle = [&]() {
        if (appliedSize != 3) return;
        SetWindowLongPtrW(window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongPtrW(window, GWL_EXSTYLE, GetWindowLongPtrW(window, GWL_EXSTYLE) & ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE));
        SetWindowPos(window, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
            widths[3], heights[3], SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    };
    applyDisplayStyle();
    Viewport viewport = fitViewport(widths[appliedSize], heights[appliedSize]);
    SetWindowTextW(window, L"game | 极限试炼");
    setbkmode(TRANSPARENT);
    BeginBatchDraw();
    if (!smokeTest) {
        // 登录不可跳过，取消退出；空白昵称和不可读存档提示后重新输入。
        wchar_t name[21]{};
        while (true) {
            if (!InputBox(name, 21, L"输入昵称（1–20 字），无需密码。同昵称读取同一存档。", L"玩家登录", L"", 520, 200, false)) {
                EndBatchDraw(); closegraph(); return 0;
            }
            if (account.login(name)) break;
            MessageBoxW(window, L"昵称不能为空，或存档无法读取/写入，请重试。", L"登录未完成", MB_OK);
        }
    }
    bool progressSaved = false;
    if (smokeTest) {
        world.load(0);
        menu.page = preview == Preview::Home ? Page::Home :
            (preview == Preview::Levels ? Page::Levels : (preview == Preview::Modes ? Page::Modes :
            ((preview == Preview::Settings || preview == Preview::Window) ? Page::Settings : Page::Playing)));
    }
    // 第二步：构造验证场景，使用真实机关和计分流程，菜单预览不伪造继续游戏记录。
    if (preview == Preview::Death || preview == Preview::Spikes) {
        for (int i = 0; i < 45; ++i) world.update({false, true, false}, FixedStep);
        const int steps = preview == Preview::Death ? 200 : 12;
        for (int i = 0; i < steps; ++i) world.update({}, FixedStep);
        if (preview == Preview::Death) {
            // 近距离诱发后先后退，再跳过仍存在的普通短刺。
            for (int i = 0; i < 12; ++i) world.update({true, false}, FixedStep);
            for (int i = 0; i < 180; ++i) {
                world.update({false, world.player.body.x < 365, i == 0 || i == 36}, FixedStep);
                if (i > 36 && world.player.grounded) break;
            }
            world.die(L"你坠入了深渊");
        }
    }
    if (preview == Preview::Map || preview == Preview::Traps) {
        world.player.body = preview == Preview::Map ? Rect{1640, 322, 24, 28} : Rect{1218, 452, 24, 28};
        for (int i = 0; i < 12; ++i) world.update({}, FixedStep);
    }
    // 主题预览定位到具有代表性的机关区域，真实关卡配置与正常游戏一致。
    if (preview == Preview::Bridge || preview == Preview::Enemies || preview == Preview::Lasers || preview == Preview::Boss || preview == Preview::Endless) {
        const int chapter = preview == Preview::Bridge ? 1 : (preview == Preview::Enemies ? 2 : (preview == Preview::Lasers ? 3 : 4));
        world.load(chapter, preview == Preview::Endless);
        if (preview == Preview::Boss) world.player.body = {2060, 332, 24, 28};
        else if (preview != Preview::Endless) world.player.body = {970, 397, 24, 28};
    }
    if (preview == Preview::Exhausted) {
        // 使用真实重生流程耗尽十点体力，最后一次死亡显示禁用的重生按钮。
        for (int i = 0; i < World::InitialStamina; ++i) { world.die(L"验证体力消耗"); world.respawn(); }
        world.die(L"本次挑战已用完所有重生体力");
    }
    // 第三步：记录前一帧按键，统一处理菜单与游戏的单次按下事件，防止跨页面误触发。
    if (preview == Preview::Adventure) {
        // 定位到第五关高路，真实更新触发落刺预警，便于检查角色和机关的可读性。
        world.load(4); world.player.body = {385, 195, 24, 28}; world.player.grounded = false;
        world.update({}, FixedStep);
    }
    if (preview == Preview::Death || preview == Preview::Exhausted) world.deathBurst.update(0.6f);
    // 通关预览覆盖部分星与满星，通过真实终点更新进入结算状态。
    if (preview == Preview::Victory || preview == Preview::VictoryFull) {
        world.load(0); world.elapsed = 65;
        if (preview == Preview::VictoryFull) {
            world.level.feathers.front().collected = true; world.scoring.collectFeather(0);
            for (int i = 0; i < starRules(0).coinTarget; ++i) { world.level.coins[i].collected = true; world.scoring.collectCoin(i); }
        }
        world.player.body = world.level.goal; world.update({}, FixedStep);
    }
    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    double accumulator = 0;
    bool previousKeys[256]{};
    Sound sound;
    bool pendingJump = false;
    bool running = true;
    int frames = 0;
    while (running && IsWindow(window)) {
        const bool wasDead = world.dead;
        if (!world.won) { victoryAge = 0; progressSaved = false; }
        const auto now = Clock::now();
        const double frameTime = std::min(std::chrono::duration<double>(now - previous).count(), 0.1);
        previous = now;
        const bool focused = GetForegroundWindow() == window;
        bool keys[256]{};
        for (int key : {VK_ESCAPE, VK_RETURN, VK_UP, VK_DOWN, VK_SHIFT, VK_F11, 65, 68, 74, 80, 82, 83, 87})
            keys[key] = focused && !smokeTest && down(key);
        const auto pressed = [&](int key) { return keys[key] && !previousKeys[key]; };
        const Input controls = mapControls(keys, previousKeys);
        // F11 可在任何页面切换显示模式，不重置世界进度。
        if (pressed(VK_F11)) menu.settings.windowSize = menu.settings.windowSize == 3 ? 0 : 3;
        // 第四步：收集鼠标移动和点击，菜单按钮和死亡按钮根据当前页面分别处理。
        bool clicked = false;
        int clickX = 0, clickY = 0;
        ExMessage message;
        while (peekmessage(&message)) {
            if (!focused || smokeTest) continue;
            if (message.message == WM_MOUSEMOVE || message.message == WM_LBUTTONDOWN) {
                int x = -1, y = -1;
                if (!viewport.mapPoint(message.x, message.y, x, y)) continue;
                if (message.message == WM_MOUSEMOVE && menu.page != Page::Playing) menu.hover(x, y);
                if (message.message == WM_LBUTTONDOWN) { clicked = true; clickX = x; clickY = y; }
            }
        }
        if (menu.page != Page::Playing) {
            // 第五步：菜单期间冻结模拟；闯关与无尽世界分开，继续只恢复无尽快照。
            MenuAction action = MenuAction::None;
            if (pressed(VK_ESCAPE) || pressed('A')) menu.back();
            else {
                if (pressed('W') || pressed(VK_UP)) menu.move(-1);
                if (pressed('S') || pressed(VK_DOWN)) menu.move(1);
                if (clicked) action = menu.click(clickX, clickY);
                else if (pressed(VK_RETURN) || pressed('D')) action = menu.activate();
            }
            if (action == MenuAction::Start) {
                if (world.endless) endlessSession = world;
                world.load(menu.chosenLevel, menu.endlessMode);
            }
            if (action == MenuAction::Continue && !world.endless && endlessSession) world = *endlessSession;
            if (action == MenuAction::Start || action == MenuAction::Continue) paused = false;
            if (action == MenuAction::Exit) running = false;
            accumulator = 0;
            pendingJump = false;
        } else if (pressed(VK_ESCAPE)) {
            // 第六步：游戏中返回菜单只切换页面，不复位角色、积分或机关计时。
            menu.home();
            accumulator = 0;
            pendingJump = false;
        } else {
            // 第七步：处理暂停和死亡操作，重生与重开共用按钮/快捷键动作。
            DeathAction action = world.dead && world.deathBurst.age >= 0.55f && clicked ? deathScreenClick(clickX, clickY, world.canRespawn()) : DeathAction::None;
            if (pressed('P') && !world.dead && !world.won) { paused = !paused; pendingJump = false; }
            if (pressed('R')) {
                if (world.won || keys[VK_SHIFT]) action = DeathAction::Restart;
                else if (world.canRespawn()) action = DeathAction::Respawn;
                else world.die(L"你选择了结束本次挑战");
            }
            if (world.canRespawn() && pressed(VK_RETURN)) action = DeathAction::Respawn;
            // 通关按钮与快捷键共用动作；页面切换时清除缓存输入与累计时间。
            const auto victory = world.won && clicked ? victoryScreenClick(clickX, clickY) : VictoryAction::None;
            if (victory == VictoryAction::Restart) action = DeathAction::Restart;
            if (victory == VictoryAction::Home) { menu.home(); accumulator = 0; pendingJump = false; }
            // 闯关结束后顺序进入下一关；最终关回主页面，无尽模式不会出现通关面板。
            if (world.won && (pressed(VK_RETURN) || victory == VictoryAction::Next)) {
                if (world.chapter < 14 && world.chapter + 1 < account.unlocked) { world.load(world.chapter + 1); menu.chosenLevel = world.chapter; }
                else menu.home();
                accumulator = 0; pendingJump = false;
            }
            if (action != DeathAction::None) {
                if (action == DeathAction::Restart) world.restart(); else world.respawn();
                paused = false;
                pendingJump = false;
                accumulator = 0;
            }
            // 第八步：以 120 Hz 固定步长更新，设置中的帧率只影响绘图，不影响运动速度。
            if (controls.jump && !paused && !world.dead && action == DeathAction::None) pendingJump = true;
            if (focused && !smokeTest && !paused && !world.dead && menu.page == Page::Playing) {
                accumulator += frameTime;
                while (accumulator >= FixedStep) {
                    const auto jumpSerial = world.player.jumpSerial;
                    const int coinsBefore = world.scoring.coinsCollected();
                    world.update({controls.left, controls.right, pendingJump, controls.shoot, controls.fastFall, controls.jumpHeld}, FixedStep);
                    pendingCoins += world.scoring.coinsCollected() - coinsBefore;
                    if (!world.dead && world.player.jumpSerial != jumpSerial) sound.play(false);
                    pendingJump = false;
                    accumulator -= FixedStep;
                }
            } else { accumulator = 0; pendingJump = false; }
        }
        if (!smokeTest && world.won && !world.endless && !progressSaved)
            progressSaved = account.complete(world.chapter, evaluateStars(world).count, world.scoring.total());
        if (world.won && menu.page == Page::Playing && (focused || smokeTest)) victoryAge += static_cast<float>(frameTime);
        if (smokeTest) victoryAge = 3;
        // 仅真实游玩入账，保存失败保留待存金币并重试。
        if (!smokeTest && pendingCoins > 0 && account.earn(pendingCoins)) pendingCoins = 0;
        // 死亡反馈单独推进，规则世界保持冻结；菜单、失焦和暂停不消耗动画。
        if (world.dead && !wasDead) {
            paused = false;
            if (!smokeTest) sound.play(true);
        }
        if (world.dead && menu.page == Page::Playing && (focused || smokeTest) && !paused)
            world.deathBurst.update(static_cast<float>(frameTime));
        std::copy(std::begin(keys), std::end(keys), std::begin(previousKeys));
        // 第九步：按页面绘制；切出窗口的提示只显示在进行中的游戏画面。
        // 窗口设置生效时重建实际窗口，菜单、世界和当前设置仍保存在内存中。
        // 显示预览从全屏切换三种窗口档，再回到全屏，检查重建后的尺寸和样式。
        if (preview == Preview::Window && frames > 0) {
            const int sequence[]{3, 0, 1, 2, 3, 3};
            menu.settings.windowSize = sequence[std::min(frames, 5)];
        }
        if (appliedSize != menu.settings.windowSize) {
            SetWorkingImage(); EndBatchDraw(); closegraph();
            appliedSize = menu.settings.windowSize;
            window = initgraph(widths[appliedSize], heights[appliedSize]);
            if (!window) return 1;
            applyDisplayStyle();
            viewport = fitViewport(widths[appliedSize], heights[appliedSize]);
            SetWindowTextW(window, L"game | 极限试炼"); BeginBatchDraw();
        }
        if (preview == Preview::Window) {
            RECT verifiedClient{}; GetClientRect(window, &verifiedClient);
            if (verifiedClient.right != widths[appliedSize] || verifiedClient.bottom != heights[appliedSize]) {
                std::cerr << "Window mismatch: requested=" << widths[appliedSize] << 'x' << heights[appliedSize]
                    << " actual=" << verifiedClient.right << 'x' << verifiedClient.bottom << " frame=" << frames << '\n';
                EndBatchDraw(); closegraph(); return 2;
            }
            // 全屏必须精确覆盖显示器且没有标题栏，不能只把窗口放大。
            if (appliedSize == 3) {
                RECT verifiedOuter{}; GetWindowRect(window, &verifiedOuter);
                if (!EqualRect(&verifiedOuter, &monitor.rcMonitor) || (GetWindowLongPtrW(window, GWL_STYLE) & WS_CAPTION)) {
                    std::cerr << "Fullscreen bounds/style verification failed\n";
                    EndBatchDraw(); closegraph(); return 3;
                }
            }
        }
        SetWorkingImage(&canvas);
        setbkmode(TRANSPARENT);
        if (menu.page == Page::Playing) {
            render();
            if (!focused && !smokeTest && !paused && !world.won && !world.dead)
                label(340, 280, L"点击游戏窗口继续", 26, RGB(255, 221, 132));
        } else menu.render();
        // 居中等比显示逻辑画布；两侧或上下留边，人物和按钮不会被拉伸。
        SetWorkingImage();
        setbkcolor(RGB(7, 11, 18)); cleardevice();
        SetStretchBltMode(GetImageHDC(), COLORONCOLOR);
        StretchBlt(GetImageHDC(), viewport.x, viewport.y, viewport.width, viewport.height,
            GetImageHDC(&canvas), 0, 0, ScreenWidth, ScreenHeight, SRCCOPY);
        FlushBatchDraw();
        // 第十步：截图验证保存三帧后的画面；正常运行按设置限制帧率。
        if (smokeTest && ++frames >= (preview == Preview::Window ? 6 : 3)) {
            const wchar_t* path = L"smoke-test.png";
            switch (preview) {
            case Preview::Home: path = L"menu-smoke-test.png"; break;
            case Preview::Levels: path = L"levels-smoke-test.png"; break;
            case Preview::Settings: path = L"settings-smoke-test.png"; break;
            case Preview::Death: path = L"death-smoke-test.png"; break;
            case Preview::Spikes: path = L"spike-smoke-test.png"; break;
            case Preview::Map: path = L"map-smoke-test.png"; break;
            case Preview::Traps: path = L"traps-smoke-test.png"; break;
            case Preview::Modes: path = L"modes-smoke-test.png"; break;
            case Preview::Window: path = L"window-smoke-test.png"; break;
            case Preview::Bridge: path = L"bridge-smoke-test.png"; break;
            case Preview::Enemies: path = L"enemies-smoke-test.png"; break;
            case Preview::Lasers: path = L"lasers-smoke-test.png"; break;
            case Preview::Boss: path = L"boss-smoke-test.png"; break;
            case Preview::Endless: path = L"endless-smoke-test.png"; break;
            case Preview::Adventure: path = L"adventure-smoke-test.png"; break;
            case Preview::Exhausted: path = L"exhausted-smoke-test.png"; break;
            case Preview::Victory: path = L"victory-smoke-test.png"; break;
            case Preview::VictoryFull: path = L"victory-full-smoke-test.png"; break;
            default: break;
            }
            saveimage(path);
            break;
        }
        std::this_thread::sleep_until(now + std::chrono::microseconds(1000000 / menu.settings.frameLimit));
    }
    // 第十一步：结束批量绘图并关闭仍然存在的窗口。
    if (IsWindow(window)) { EndBatchDraw(); closegraph(); }
    return 0;
}
void Game::render() const {
    // 绘制步骤一：计算世界到屏幕的水平变换；背景、HUD 和结算界面不参与滚动。
    const float camera = world.cameraX();
    const auto screenRect = [camera](Rect r) { r.x -= camera; return r; };
    // 绘制步骤二：各关使用独立的背景主题，装饰不参与碰撞。
    drawBackground(world);
    // 绘制步骤三：显示固定顶部面板，包括操作、死亡数、时间、存档与积分。
    box({0, 0, 960, 110}, RGB(22, 32, 51));
    label(28, 16, L"GAME / " + world.level.name, 28, RGB(239, 245, 255));
    if (menu.settings.showHints) {
        // 操作提示可以在设置页关闭，标题和关键数值仍保留。
        label(28, 56, L"A / D 移动    W 二段跳    S 快速下落    J 射击", 18, RGB(153, 178, 202));
        label(28, 81, L"R 重生  Shift+R 重开  P 暂停  Esc 菜单  F11 全屏", 16, RGB(153, 178, 202));
    }
    label(650, 22, L"死亡 " + std::to_wstring(world.deaths) + L"    时间 " +
        std::to_wstring(static_cast<int>(world.elapsed)) + L"s", 21, RGB(239, 245, 255));
    label(650, 60, world.endless ? L"距离 " + std::to_wstring(world.scoring.distanceMeters) + L" m" :
        (world.checkpointActive ? L"存档点已激活" : L"前往绿色旗帜存档"), 18, RGB(92, 221, 167));
    label(650, 83, L"积分 " + std::to_wstring(world.scoring.total()) + L"    障碍 " +
        std::to_wstring(world.scoring.cleared()), 18, RGB(255, 230, 90));
    // 绘制步骤四：主题材质、高路、移动轨迹和预警，让地图的可探索区域更清楚。
    drawTerrain(world);
    drawAdventureScene(world);
    // 绘制步骤五：使用尖刺活动模块的实时外框画三角形，伸长时变为更鲜明的红色。
    for (std::size_t i = 0; i < world.level.spikes.size(); ++i) {
        const auto spike = screenRect(world.spikeActivity.bounds(i, world.level.spikes[i]));
        POINT points[3] = {{static_cast<LONG>(spike.x), static_cast<LONG>(spike.bottom())},
            {static_cast<LONG>(spike.x + spike.w / 2), static_cast<LONG>(spike.y)},
            {static_cast<LONG>(spike.right()), static_cast<LONG>(spike.bottom())}};
        setfillcolor(spike.h > world.level.spikes[i].h + 1 ? RGB(255, 64, 82) : RGB(255, 105, 128));
        solidpolygon(points, 3);
    }
    // 绘制步骤六：上排尖刺向下绘制，顶部支架固定，夹击陷阱使用联动后的高度。
    for (const auto& trap : world.level.overheadTraps) {
        const auto r = screenRect(overheadBounds(trap, world));
        box({r.x, r.y - 10, r.w, 10}, RGB(52, 71, 94));
        POINT points[3]{{static_cast<LONG>(r.x), static_cast<LONG>(r.y)},
            {static_cast<LONG>(r.right()), static_cast<LONG>(r.y)},
            {static_cast<LONG>(r.x + r.w / 2), static_cast<LONG>(r.bottom())}};
        setfillcolor(trap.style == TrapStyle::Crusher ? RGB(255, 154, 90) : RGB(186, 136, 255));
        solidpolygon(points, 3);
    }
    // 绘制步骤七：存档旗帜和终点随地图滚动，旗帜颜色反映是否已激活。
    const auto flag = screenRect(world.level.checkpoint);
    box({flag.x, flag.y, 3, flag.h}, RGB(177, 207, 215));
    box({flag.x + 3, flag.y, 24, 17}, world.checkpointActive ? RGB(107, 255, 175) : RGB(55, 151, 115));
    const auto goal = screenRect(world.level.goal);
    box(goal, world.level.boss && world.bossHealth > 0 ? RGB(130, 80, 80) : RGB(234, 183, 80));
    box({goal.x + 5, goal.y + 5, 22, 47}, RGB(76, 61, 42));
    label(static_cast<int>(goal.x) - 4, static_cast<int>(goal.y) - 30, L"终点", 18, RGB(255, 221, 132));
    drawCombatScene(world);
    // 绘制步骤七：鲸鱼娘随奔跑、起跳和落地播放动作，HUD 显示本关探索奖励。
    if (!world.dead && (world.hurtCooldown <= 0 || static_cast<int>(world.hurtCooldown * 12) % 2 == 0)) drawPlayer(world, menu.settings.playerPalette);
    label(440, 52, (world.endless ? L"无限重生" : L"体力 " + std::to_wstring(world.stamina) + L" / 10"), 18, world.stamina <= 2 ? RGB(255, 128, 116) : RGB(127, 225, 170));
    label(540, 22, L"血量 " + std::to_wstring(world.health) + L" / 5", 18, world.health <= 2 ? RGB(255,100,100) : RGB(127,225,170));
    label(440, 80, L"金币 " + std::to_wstring(world.scoring.coinsCollected()), 18, RGB(255, 218, 92));
    // 血滴使用世界坐标和当前镜头，短拖尾显示飞溅方向，末段缩小消失。
    for (const auto& p : world.deathBurst.drops) {
        const float fade = std::min(1.0f, p.life * 3);
        const int x = static_cast<int>(p.x - camera), y = static_cast<int>(p.y);
        setlinecolor(RGB(145, 20, 32));
        line(x, y, static_cast<int>(p.x - camera - p.vx * 0.018f), static_cast<int>(p.y - p.vy * 0.018f));
        setfillcolor(RGB(225, 35, 48));
        solidcircle(x, y, std::max(1, static_cast<int>(p.radius * fade)));
    }
    if (!world.endless) {
        const int feathers = static_cast<int>(std::count_if(world.level.feathers.begin(), world.level.feathers.end(), [](const auto& f) { return f.collected; }));
        label(440, 22, L"米饭 " + std::to_wstring(feathers) + L" / " + std::to_wstring(world.level.feathers.size()), 18, RGB(255, 230, 133));
    }
    if (menu.settings.showHints)
        label(32, 590, world.endless ? L"W 短按小跳 / 长按高跳；S 快落，J 射击。" :
            (world.chapter == 1 ? L"裂纹桥面踩踏后会坍塌，及时起跳；避开移动锯轮。" :
            (world.chapter == 3 && !world.endless ? L"激光亮起时致命，虚线时可通行。观察节奏再出发。" :
            L"W 短按小跳 / 长按高跳；起点上方有弹簧与米饭支线。")), 18, RGB(220, 231, 237));
    // 绘制步骤八：死亡结算优先覆盖整个场景，否则显示暂停或通关面板。
    if (world.dead) {
        if (world.deathBurst.age >= 0.55f) drawDeathScreen(world);
        return;
    }
    if (world.won) { drawVictoryScreen(world, victoryAge); return; }
    if (paused) {
        box({235, 190, 490, 140}, RGB(30, 45, 67));
        label(285, 215, L"游戏已暂停", 28, RGB(255, 221, 132));
        label(285, 270, L"按 P 继续 / Esc 主页面", 19, RGB(239, 245, 255));
    }
}
}

