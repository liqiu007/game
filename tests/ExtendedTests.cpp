// 模式与机关回归测试：验证五关配置、战斗结果、断桥以及真实无尽通行路线。
#include "World.h"
#include "Menu.h"
#include "Combat.h"
#include "Controls.h"
#include "DisplayLayout.h"
#include <stdexcept>
#include <utility>

namespace game {
namespace {
void require(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
void steps(World& w, int count, Input input = {}) { for (int i = 0; i < count; ++i) w.update(input, FixedStep); }
bool travelTo(World& world, float target) {
    // 尝试不同等待和二段跳时机，使用真实物理与危险判定寻找一条可行路线。
    for (int delay = 0; delay <= 240; delay += 12) {
        for (int secondJump : {24, 36, 48, 60}) {
            World candidate = world;
            steps(candidate, delay, {false, false, false, true});
            for (int tick = 0; tick < 210 && !candidate.dead; ++tick) {
                candidate.update({false, candidate.player.body.x < target, tick == 0 || tick == secondJump, true}, FixedStep);
                if (tick > secondJump && candidate.player.grounded && candidate.player.body.x >= target - 4) {
                    world = candidate; return true;
                }
            }
        }
    }
    return false;
}
}
void runExtendedTests() {
    // 输入回归：W 单次起跳、A/D 左右、S 快落，旧方向键和空格不再操纵角色。
    bool keys[256]{}, previous[256]{};
    keys['W'] = keys['A'] = keys['S'] = true;
    auto controls = mapControls(keys, previous);
    require(controls.jump && controls.left && !controls.right && controls.fastFall, "WASD mapping must match platform controls");
    previous['W'] = true;
    require(!mapControls(keys, previous).jump, "Holding W must not repeatedly jump");
    keys['W'] = keys['A'] = keys['S'] = false;
    keys['D'] = keys['J'] = true;
    require(mapControls(keys, previous).right && mapControls(keys, previous).shoot, "D and J must support held movement and shooting");
    keys['D'] = keys['J'] = false;
    keys[32] = keys[37] = keys[39] = keys['Z'] = true;
    controls = mapControls(keys, previous);
    require(!controls.left && !controls.right && !controls.jump, "Legacy gameplay bindings must not trigger movement or jump");
    World fall;
    fall.update({false, false, true}, FixedStep);
    steps(fall, 10); // 先离开地面，再验证空中快落速度，避免同一步已经落地归零。
    fall.update({false, false, false, false, true}, FixedStep);
    require(fall.player.vy >= 600, "S must start a fast downward movement in the air");
    steps(fall, 60, {false, false, false, false, true});
    require(fall.player.grounded && fall.player.body.bottom() == 550, "Fast fall must still collide with platforms");
    // 显示回归：宽屏和竖屏保持比例，黑边不触发点击，菜单与死亡按钮坐标正确。
    for (const auto& size : {std::pair<int,int>{1920,1080}, {1600,900}, {900,1600}, {960,640}}) {
        const auto viewport = fitViewport(size.first, size.second);
        require(viewport.width * 2 == viewport.height * 3, "Viewport must maintain 3:2 aspect ratio");
        int x = -1, y = -1;
        require(viewport.mapPoint(viewport.x + viewport.width / 2, viewport.y + viewport.height / 2, x, y) && x == 480 && y == 320,
            "Fullscreen center must map to logical center");
        require(!viewport.mapPoint(viewport.x - 1, viewport.y, x, y), "Letterbox must not trigger buttons");
        require(viewport.mapPoint(viewport.x + viewport.width * 400 / 960, viewport.y + viewport.height * 360 / 640, x, y) &&
            deathScreenClick(x, y) == DeathAction::Respawn, "Scaled death button must remain clickable");
    }
    require(Menu{}.settings.windowSize == 3, "Game must default to fullscreen");
    // 第一组：五关名称、背景索引和专属机关完整，无尽入口不经过关卡选择。
    const wchar_t* names[]{L"小试牛刀", L"断桥危机", L"敌人来袭", L"小心激光", L"终章：BOSS战"};
    for (int i = 0; i < 5; ++i) {
        World w; w.load(i);
        require(w.level.theme == i && w.level.name == names[i], "Five chapters must have exact names and distinct themes");
        require(!w.dead && !w.won && w.player.body.x == 60, "Loading a chapter must start cleanly");
    }
    Menu menu; menu.page = Page::Modes; menu.selected = 1;
    require(menu.activate() == MenuAction::Start && menu.endlessMode, "Endless mode must start directly");
    menu.home(); menu.page = Page::Levels; menu.selected = 4;
    require(menu.activate() == MenuAction::Start && menu.chosenLevel == 4 && !menu.endlessMode, "Campaign selection must select the boss chapter");
    // 第二组：断桥踩踏后自动坍塌，重生恢复；锯轮碰撞致命。
    World bridge; bridge.load(1); bridge.player.body = {365, 462, 24, 28};
    steps(bridge, 90);
    require(bridge.level.fragile[0].collapsed && bridge.player.body.y > 462, "Fragile bridge must collapse and let player fall");
    bridge.respawn();
    require(!bridge.level.fragile[0].collapsed, "Respawn must restore fragile platforms");
    bridge.player.body = bridge.level.saws[0].body;
    steps(bridge, 1);
    require(bridge.dead, "Moving saw must be lethal");
    // 第三组：子弹击败巡逻敌人并计分；从上方踩击也可以消灭敌人。
    World enemy; enemy.load(2); enemy.player.body = {340, 462, 24, 28};
    steps(enemy, 45, {false, false, false, true});
    require(!enemy.dead && !enemy.level.enemies[0].alive && enemy.scoring.total() >= 200, "Player shots must defeat enemies and grant reward");
    enemy.restart();
    enemy.player.body = {enemy.level.enemies[0].body.x, 433, 24, 28}; enemy.player.vy = 90;
    steps(enemy, 5);
    require(!enemy.dead && !enemy.level.enemies[0].alive && enemy.player.vy < 0, "Stomping must defeat enemy and bounce player");
    // 第四组：激光在亮起期致命，在熄灭期允许同一位置通过。
    World laser; laser.load(3); laser.player.body = {430, 462, 24, 28}; steps(laser, 1);
    require(laser.dead, "Active laser must kill player");
    laser.restart(); laser.elapsed = 2; laser.player.body = {430, 462, 24, 28}; steps(laser, 1);
    require(!laser.dead, "Inactive laser must allow passage");
    // 第五组：BOSS 存活时锁定终点，命中最后一点生命后开放出口，弹幕仍可伤人。
    World boss; boss.load(4); boss.player.body = {2180, 332, 24, 28}; steps(boss, 1);
    require(!boss.won, "Living boss must lock goal");
    boss.restart(); boss.bossHealth = 1; boss.player.body = {2070, 332, 24, 28};
    steps(boss, 25, {false, false, false, true});
    require(boss.bossHealth == 0 && !boss.dead && boss.scoring.total() >= 1000, "Shooting must defeat boss and grant reward");
    boss.player.body = {2180, 332, 24, 28}; steps(boss, 1);
    require(boss.won, "Defeated boss must unlock goal");
    boss.restart(); boss.player.body = {2070, 332, 24, 28};
    boss.projectiles.push_back({boss.player.body, 0, 0, 1, true}); steps(boss, 1);
    require(boss.dead, "Hostile boss bullet must kill player");
    // 第六组：真实移动越过十二段无尽平台，验证生成会延伸且不会意外通关。
    World endless; endless.load(0, true);
    const float initialWidth = endless.level.width;
    steps(endless, 65, {false, true, false});
    for (std::size_t i = 1; i <= 12; ++i) {
        endless.extendEndless();
        const float target = endless.level.platforms[i].x + 24;
        for (int tick = 0; tick < 200; ++tick) {
            endless.update({false, endless.player.body.x < target, tick == 0 || tick == 36}, FixedStep);
            if (tick > 36 && endless.player.grounded) break;
        }
        require(!endless.dead && endless.player.body.x >= target - 5, "Generated endless platform must be reachable");
        steps(endless, 180); // 落脚位置会引诱同平台机关，等待整组缩回再出发。
    }
    require(endless.level.width > initialWidth && !endless.won && endless.scoring.total() > 0, "Endless generation must continue with stable scoring");
    const int score = endless.scoring.total();
    endless.extendEndless(); steps(endless, 1);
    require(endless.scoring.total() == score, "Generation must not award old platforms again");
    endless.restart();
    require(endless.endless && endless.scoring.total() == 0 && endless.player.body.x == 60, "Endless restart must reset run but keep mode");
    // 第七组：其余四关也使用真实等待、跳跃、射击走到终点，确保机制组合没有封死路线。
    for (int chapter = 1; chapter < 5; ++chapter) {
        World route; route.load(chapter);
        steps(route, 34, {false, true, false}); steps(route, 180);
        for (float target : {365.0f, 555.0f, 780.0f, 970.0f, 1180.0f, 1410.0f, 1640.0f, 1870.0f, 2080.0f}) {
            if (!travelTo(route, target)) throw std::runtime_error("Campaign route blocked: chapter=" + std::to_string(chapter + 1) + ", target=" + std::to_string(static_cast<int>(target)));
        }
        if (chapter == 4) {
            // 在竞技场尝试不同跳跃节奏躲弹，必须击败完整血量的 BOSS 才继续。
            bool defeated = false;
            for (int period : {70, 90, 110, 130, 160, 200}) {
                World fight = route;
                for (int tick = 0; tick < 1800 && !fight.dead && fight.bossHealth > 0; ++tick)
                    fight.update({false, false, tick % period == 0, true}, FixedStep);
                if (!fight.dead && fight.bossHealth <= 0) { route = fight; defeated = true; break; }
            }
            require(defeated, "Full-health BOSS must be defeatable while dodging projectiles");
        }
        steps(route, 50, {false, true, false, true});
        require(route.won && !route.dead, "Each campaign chapter must reach its actual finish");
    }
}
}
