// 无窗口回归测试：验证物理、完整通关、计分、死亡、镜头及尖刺组同步。
#include "World.h"
#include "Menu.h"
#include "SpecialTraps.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace game {
namespace {
// 断言失败时抛出带定位信息的异常，由测试入口统一输出并返回非零退出码。
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
// 以固定输入推进指定数量的物理步，避免测试依赖真实时间和键盘。
void step(World& w, int count, Input input = {}) { for (int i = 0; i < count; ++i) w.update(input, FixedStep); }
void baitFirstTrap(World& w) {
    // 从出生点走到安全触发距离，再等待第一组尖刺完成整个伸缩周期。
    step(w, 34, {false, true, false});
    step(w, 180);
}
void jumpTo(World& w, float target) {
    // 朝目标平台前进，起跳后补一次二段跳；安全落地后结束本段路线。
    for (int i = 0; i < 180; ++i) {
        w.update({false, w.player.body.x < target, i == 0 || i == 36}, FixedStep);
        if (i > 36 && w.player.grounded) break;
    }
}
}
int runSelfTests() {
    try {
        // 反馈回归：实际起跳计数、死亡去重、冻结规则、粒子重力与生命周期。
        World feedback;
        feedback.update({false, false, true}, FixedStep);
        check(feedback.player.jumpSerial == 1, "Jump feedback missing");
        feedback.update({false, false, true}, FixedStep);
        feedback.update({false, false, true}, FixedStep);
        check(feedback.player.jumpSerial == 2, "Rejected jump played feedback");
        feedback.die(L"反馈测试");
        feedback.die(L"重复死亡");
        check(feedback.deaths == 1 && feedback.deathBurst.drops.size() == 48, "Death burst duplicated");
        const auto frozen = feedback.player.body;
        const auto drop = feedback.deathBurst.drops.front();
        feedback.update({}, FixedStep);
        feedback.deathBurst.update(0.1f);
        check(feedback.player.body.y == frozen.y && feedback.deathBurst.drops.front().vy > drop.vy,
            "Death physics must freeze while particles fall");
        feedback.deathBurst.update(1.5f);
        check(feedback.deathBurst.drops.empty(), "Particles did not expire");
        feedback.respawn();
        check(feedback.deathBurst.age == 0 && feedback.deathBurst.drops.empty(), "Respawn left feedback behind");
        // 检查两种合成音色的 WAV 标识、长度和首尾静音包络。
        for (bool death : {false, true}) {
            const auto wav = synthesizeSound(death);
            check(wav.size() == 44 + (death ? 22050 / 2 : 22050 / 7) * 2, "WAV size mismatch");
            check(wav[0] == 'R' && wav[8] == 'W' && wav[34] == 16, "Invalid PCM header");
            check(wav[44] == 0 && wav[45] == 0 && wav.back() == 0 && wav[wav.size() - 2] == 0,
                "Sound envelope must start/end at silence");
        }

        // 测试一：落地位置、第一次跳跃、二段跳、拒绝第三跳和落地补充次数。
        World w;
        step(w, 20);
        check(w.player.grounded && std::abs(w.player.body.y - 522) < 0.01f, "Ground landing failed");
        w.update({false, false, true}, FixedStep);
        check(w.player.vy < 0 && w.player.jumpsLeft == 1, "First jump failed");
        step(w, 5);
        w.update({false, false, true}, FixedStep);
        check(w.player.jumpsLeft == 0, "Double jump failed");
        const float before = w.player.vy;
        w.update({false, false, true}, FixedStep);
        check(w.player.vy > before, "Third jump must be rejected");
        step(w, 150);
        check(w.player.grounded && w.player.jumpsLeft == 2, "Landing must replenish jumps");
        // 测试二：触碰尖刺后冻结物理与计时，重复死亡请求不重复增加次数。
        w.player.body.x = 185;
        w.update({}, FixedStep);
        check(w.dead && w.deaths == 1 && w.player.body.x == 185, "Spike must show death screen before respawn");
        const float deathTime = w.elapsed;
        step(w, 30, {false, true, true});
        w.die(L"Repeated death");
        check(w.deaths == 1 && w.elapsed == deathTime && w.player.body.x == 185, "Death state must freeze simulation");
        // 测试三：手动重生不重复记死亡；激活存档后掉落应回到存档点。
        w.respawn();
        check(!w.dead && w.deaths == 1 && w.player.body.x == w.level.spawnX, "Respawn must not count death twice");
        w.player.body = {555, 402, 24, 28};
        w.update({}, FixedStep);
        check(w.checkpointActive, "Checkpoint activation failed");
        w.player.body.y = 800;
        w.update({}, FixedStep);
        check(w.dead && w.deaths == 2, "Falling must settle death");
        w.respawn();
        check(!w.dead && w.player.body.x == 555, "Checkpoint respawn failed");
        // 测试四：触碰新终点后冻结世界，重新开始清除本局状态。
        w.player.body = {w.level.goal.x, w.level.goal.bottom() - 28, 24, 28};
        w.update({}, FixedStep);
        check(w.won, "Goal detection failed");
        const float elapsed = w.elapsed;
        step(w, 10, {true, false, false});
        check(w.elapsed == elapsed && w.player.body.x == w.level.goal.x, "Win must freeze simulation");
        w.restart();
        check(!w.won && !w.checkpointActive && w.deaths == 0 && w.elapsed == 0, "Restart failed");
        // 测试五：使用独立平台检查水平撞墙和上升撞到平台底部的处理。
        w.level.platforms = {{100, 100, 100, 20}};
        w.level.spikes.clear();
        w.player.body = {73, 100, 24, 28};
        step(w, 4, {false, true, false});
        check(w.player.body.right() <= 100.01f, "Side collision failed");
        w.player.body = {120, 121, 24, 28};
        w.player.vy = -400;
        w.update({}, FixedStep);
        check(w.player.body.y >= 120 && w.player.vy == 0, "Ceiling collision failed");
        // 测试六：通过真实输入走完整个扩展地图，逐段引诱机关、跳跃并检查总分。
        World route;
        baitFirstTrap(route);
        jumpTo(route, 365);
        check(route.player.grounded && route.player.body.x >= 340, "First gap is not traversable");
        check(route.scoring.total() == 350 && route.scoring.cleared() == 3, "First two spikes and first landing should score 350");
        jumpTo(route, 555);
        check(route.checkpointActive, "Tutorial route cannot reach checkpoint");
        step(route, 180); // 等待存档点旁的整组尖刺缩回。
        jumpTo(route, 780);
        check(!route.won, "Old endpoint must no longer finish the expanded map");
        jumpTo(route, 970);
        check(!route.dead && route.player.body.x > ScreenWidth, "Player must move beyond the old screen edge");
        jumpTo(route, 1180);
        step(route, 180);
        jumpTo(route, 1410);
        jumpTo(route, 1640);
        step(route, 180);
        jumpTo(route, 1870);
        jumpTo(route, 2080);
        step(route, 50, {false, true, false});
        check(route.won && route.deaths == 0, "Tutorial must be completable without dying");
        check(route.scoring.total() == 3400 && route.scoring.cleared() == 20, "Expanded route must score floor spikes, ceiling spikes and platforms");
        // 测试七：镜头停在地图边界，重生与重开后立即返回正确视野，窄地图不滚动。
        check(route.cameraX() == route.level.width - ScreenWidth, "Camera must stop at the map's right edge");
        route.respawn();
        check(route.cameraX() < 200, "Checkpoint respawn must reset camera position");
        route.restart();
        check(route.cameraX() == 0, "Restart must reset camera to map start");
        route.level.width = 500;
        check(route.cameraX() == 0, "A narrow level must not produce a negative camera offset");
        // 测试八：死亡分数快照不变，重生重复经过同一障碍不能刷分。
        World scored;
        baitFirstTrap(scored);
        jumpTo(scored, 365);
        scored.die(L"Test death");
        check(scored.scoring.deathScore() == 350, "Death screen should snapshot score");
        step(scored, 20, {false, true, true});
        check(scored.scoring.total() == 350, "Cannot score while dead");
        scored.respawn();
        baitFirstTrap(scored);
        jumpTo(scored, 365);
        check(scored.scoring.total() == 350 && scored.scoring.cleared() == 3, "Respawn and repeated obstacles must not farm points");
        // 测试九：重开后清空分数与已奖励集合，同样的障碍可以重新获得奖励。
        scored.restart();
        check(scored.scoring.total() == 0 && scored.scoring.cleared() == 0 && scored.scoring.deathScore() == 0 && !scored.dead,
            "Restart must clear score, settlement and awarded obstacles");
        baitFirstTrap(scored);
        jumpTo(scored, 365);
        check(scored.scoring.total() == 350, "New game should allow earning points again");
        // 测试十：尖刺允许自定义分值，从底部经过不能被当作成功跨越。
        World custom;
        custom.level.spikeScores[0] = 75;
        Player beforePlayer, afterPlayer;
        beforePlayer.body = {203, 550, 24, 28};
        afterPlayer.body = {205, 550, 24, 28};
        custom.scoring.update(beforePlayer, afterPlayer, custom.level);
        check(custom.scoring.total() == 0, "Passing underneath a spike should not score");
        beforePlayer.body.y = afterPlayer.body.y = 480;
        custom.scoring.update(beforePlayer, afterPlayer, custom.level);
        check(custom.scoring.total() == 75, "Obstacle-specific point values should be honored");
        // 测试十一：结算按钮内的坐标返回正确动作，按钮外的点击不执行动作。
        check(deathScreenClick(400, 360) == DeathAction::Respawn && deathScreenClick(400, 440) == DeathAction::Restart &&
            deathScreenClick(10, 10) == DeathAction::None, "Death screen button hitboxes failed");
        // 测试十二：远处和平台下方不触发，靠近后整组伸长、底座不动、远处组不受影响。
        World trapTest;
        step(trapTest, 20);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Idle, "Distant players must not trigger traps");
        trapTest.spikeActivity.update({210, 560, 24, 28}, trapTest.level.spikes, FixedStep);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Idle, "Players below platform must not trigger traps");
        step(trapTest, 22, {false, true, false});
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Extending, "Approaching must trigger growth");
        step(trapTest, 12);
        const auto extended = trapTest.spikeActivity.bounds(1, trapTest.level.spikes[1]);
        check(std::abs(extended.h - 148) < 0.01f && extended.bottom() == 550, "Trap must extend upward with fixed base");
        check(trapTest.spikeActivity.bounds(0, trapTest.level.spikes[0]).h == extended.h, "Adjacent spikes must extend together");
        check(trapTest.spikeActivity.bounds(2, trapTest.level.spikes[2]).h == 24, "Distant groups must not activate together");
        // 测试十三：经过保持阶段后整组同步缩回，完成后本条命不再重复触发。
        step(trapTest, 100);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Retracting, "Trap should retract after hold");
        check(trapTest.spikeActivity.bounds(0, trapTest.level.spikes[0]).h ==
            trapTest.spikeActivity.bounds(1, trapTest.level.spikes[1]).h, "Grouped spikes must retract together");
        step(trapTest, 100);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Spent &&
            trapTest.spikeActivity.bounds(1, trapTest.level.spikes[1]).h == 24, "Trap should return to normal size");
        step(trapTest, 180);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Spent, "Trap must not retrigger continuously");
        trapTest.respawn();
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Idle, "Respawn should rearm traps");
        // 测试十四：原本位于短尖刺上方的角色会被伸长部分击中；死亡后机关冻结且不加分。
        trapTest.player.body = {210, 440, 24, 28};
        trapTest.player.grounded = false;
        step(trapTest, 12);
        check(trapTest.dead && trapTest.deathReason == L"你被突然伸长的尖刺刺中了", "Extended spike collision must be lethal");
        check(trapTest.scoring.total() == 0, "Trap death must not grant points");
        const float frozenExtension = trapTest.spikeActivity.traps[0].extension;
        step(trapTest, 60);
        check(trapTest.spikeActivity.traps[0].extension == frozenExtension, "Trap animation must freeze on death");
        // 测试十五：重开恢复机关状态，存档点重生位置在附近机关伸长时仍安全。
        trapTest.restart();
        check(!trapTest.dead && trapTest.spikeActivity.traps[0].extension == 0 &&
            trapTest.spikeActivity.traps[0].phase == SpikePhase::Idle, "Restart must reset trap state");
        trapTest.checkpointActive = true;
        trapTest.respawn();
        step(trapTest, 20);
        check(!trapTest.dead && trapTest.spikeActivity.traps[1].phase == SpikePhase::Holding,
            "Checkpoint spawn must remain safe while nearby trap activates");
        // 测试十六：乱序存储的相邻尖刺链也应合并，不同楼层或远处分开成组。
        const std::vector<Rect> unorderedSpikes{{148, 526, 24, 24}, {100, 526, 24, 24}, {124, 526, 24, 24},
            {124, 406, 24, 24}, {300, 526, 24, 24}};
        auto groups = SpikeActivity::tutorial(unorderedSpikes);
        check(groups.traps.size() == 3 && groups.traps[0].indices.size() == 3,
            "Grouping must join an unsorted adjacent chain, but separate floors and distant spikes");
        // 测试十七：从整组右侧靠近也能触发，逐步检查整个周期内所有成员高度一致。
        groups.update({190, 522, 24, 28}, unorderedSpikes, FixedStep);
        check(groups.traps[0].phase == SpikePhase::Extending && groups.traps[1].phase == SpikePhase::Idle,
            "Approaching the rightmost group member must trigger the entire group only");
        for (int i = 0; i < 150; ++i) {
            groups.update({190, 522, 24, 28}, unorderedSpikes, FixedStep);
            check(groups.bounds(0, unorderedSpikes[0]).h == groups.bounds(1, unorderedSpikes[1]).h &&
                groups.bounds(1, unorderedSpikes[1]).h == groups.bounds(2, unorderedSpikes[2]).h,
                "Group members must remain synchronized throughout the whole cycle");
        }
        // 测试十八：禁用不存在的继续进度，经模式页面进入真实的五关选择列表。
        Menu menu;
        check(menu.page == Page::Home && !menu.hasSession && !menu.enabled(1), "Fresh menu must disable continue");
        check(menu.click(100, 310) == MenuAction::None && menu.page == Page::Home, "Disabled continue must not start a game");
        menu.move(1);
        check(menu.selected == 2, "Keyboard navigation should skip disabled continue");
        menu.activate();
        check(menu.page == Page::Modes && menu.itemCount() == 3, "Home must open game modes before level selection");
        menu.activate();
        check(menu.page == Page::Levels && menu.itemCount() == 6, "Campaign must contain five levels and back");
        check(menu.activate() == MenuAction::Start && menu.page == Page::Playing && menu.hasSession, "Level selection must start the only level");
        menu.home();
        check(menu.selected == 1 && menu.activate() == MenuAction::Continue, "Returning home must allow continuing current session");
        // 测试十九：三项设置真实切换，返回页面不影响是否能继续游戏。
        menu.home();
        menu.selected = 3;
        menu.activate();
        check(menu.page == Page::Settings, "Settings page must open");
        menu.activate();
        check(!menu.settings.showHints, "Hint toggle must change value");
        menu.selected = 1;
        menu.activate(); menu.activate(); menu.activate();
        // 验证新增三个人物可被选择，最后循环回鲸鱼娘。
        check(menu.itemText(1).find(L"Claude") != std::wstring::npos, "Claude selection missing");
        menu.activate();
        check(menu.itemText(1).find(L"GPT") != std::wstring::npos, "GPT selection missing");
        menu.activate();
        check(menu.itemText(1).find(L"Grok") != std::wstring::npos, "Grok selection missing");
        menu.activate();
        check(menu.settings.playerPalette == 0, "Character selection must wrap after six options");
        menu.selected = 2;
        menu.activate();
        check(menu.settings.frameLimit == 60, "Frame limit toggle must switch to 60 FPS");
        menu.selected = 3; menu.activate();
        check(menu.settings.windowSize == 0, "Display mode must switch from default fullscreen to windowed");
        menu.selected = 4;
        menu.activate();
        check(menu.page == Page::Home && menu.hasSession, "Leaving settings must retain session");
        check(menu.click(100, 250) == MenuAction::None && menu.page == Page::Modes, "Mouse start must open modes");
        menu.home(); menu.selected = 4;
        check(menu.activate() == MenuAction::Exit, "Exit button must return exit action");
        // 测试二十：倒挂尖刺判定致命，夹击上排与下排使用完全相同的伸缩进度。
        World special;
        const auto ceiling = special.level.overheadTraps[0];
        special.player.body = {ceiling.body.x + 6, ceiling.body.y + 20, 24, 28};
        check(hitOverheadTrap(ceiling, special) && special.dead, "Ceiling spike must kill on contact");
        special.restart();
        const auto& crusher = special.level.overheadTraps[3];
        special.player.body = {1180, 452, 24, 28};
        step(special, 12);
        check(!special.dead && overheadBounds(crusher, special).h == 120, "Crusher must close when its floor group extends");
        check(overheadBounds(ceiling, special).h == 48, "Static ceiling must not extend with other traps");
        step(special, 180);
        check(overheadBounds(crusher, special).h == 24, "Crusher must retract with its floor group");
        special.respawn();
        check(overheadBounds(crusher, special).h == 24, "Respawn must reset upper crusher geometry");
        // 所有检查通过返回成功；任意断言异常都在下方转换为失败提示和退出码。
        runExtendedTests();
        runAdventureTests();
        std::cout << "PASS: five campaign routes, five feather routes, jump feel, adventure hazards, menu, fullscreen mapping, combat, boss, endless, scoring\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
}

