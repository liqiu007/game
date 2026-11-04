// 模式与机关回归测试：验证五关配置、战斗结果、断桥以及真实无尽通行路线。
#include "World.h"
#include "Menu.h"
#include "Account.h"
#include "Combat.h"
#include "Controls.h"
#include "DisplayLayout.h"
#include <stdexcept>
#include <utility>
#include <cmath>

namespace game {
namespace {
void require(bool result, const char* message) { if (!result) throw std::runtime_error(message); }
void steps(World& w, int count, Input input = {}) { for (int i = 0; i < count; ++i) w.update(input, FixedStep); }
void baitNearby(World& w) {
    // 用真实移动靠近至最小随机触发半径内，不修改陷阱参数或生命状态。
    for (const auto& spike : w.level.spikes) {
        if (std::abs(w.player.body.bottom() - spike.bottom()) > 1 || spike.x < w.player.body.right() || spike.x - w.player.body.right() > 110) continue;
        for (int i = 0; i < 120 && w.player.body.x < spike.x - 28 && !w.dead; ++i) w.update({false, true, false, true}, FixedStep);
        steps(w, 200, {false, false, false, true});
        for (int i = 0; i < 60 && w.player.body.x > spike.x - 50 && !w.dead; ++i) w.update({true, false, false, true}, FixedStep);
        break;
    }
}
bool travelTo(World& world, float target) {
    // 尝试不同等待和二段跳时机，使用真实物理与危险判定寻找一条可行路线。
    // 眩晕期间必须等待恢复后才能尝试下一次跳跃。
    if (world.player.stun > 0) steps(world, static_cast<int>(world.player.stun / FixedStep) + 1);
    baitNearby(world);
    for (int approach : {0, -4, -8, 16, 32, 48}) for (int delay = 0; delay <= 240; delay += 6) {
        for (int secondJump : {24, 36, 48, 60, 18, 30, 42, 54}) {
            World candidate = world;
            // 可在平台上后退起跳，或先走过头顶锯轮/倒刺再跳，保持真实碰撞。
            steps(candidate, std::abs(approach), {approach < 0, approach > 0, false, true});
            steps(candidate, delay, {false, false, false, true});
            for (int tick = 0; tick < 210 && !candidate.dead; ++tick) {
                candidate.update({false, candidate.player.body.x < target, tick == 0 || tick == secondJump, true}, FixedStep);
                if (!candidate.dead && tick > secondJump && candidate.player.grounded && candidate.player.body.x >= target - 4) {
                    world = candidate; return true;
                }
            }
        }
    }
    return false;
}
}
void runExtendedTests() {
    // 三种武器命中效果独立于普通弹丸，并严格限制 Claude 的出现关卡。
    for (int level = 0; level < 6; ++level) {
        World early; early.load(level);
        for (const auto& enemy : early.level.enemies) require(enemy.avatar != 0, "Claude appeared before chapter seven");
    }
    // Claude 在旧射程以外且存在高度差时仍能朝玩家甩出放大图片。
    World ranged; ranged.level.platforms.clear(); ranged.level.spikes.clear(); ranged.level.overheadTraps.clear();
    ranged.player.body = {800,200,24,28};
    ranged.level.enemies = {{{100,100,26,28},100,150,0,1,true,0,4.0f}};
    updateCombat(ranged,{},ranged.player,FixedStep);
    require(ranged.projectiles.size() == 1 && ranged.projectiles[0].kind == ShotKind::Ban && ranged.projectiles[0].vx > 0 && ranged.projectiles[0].vy > 0, "Claude ranged throw missing");
    require(ranged.projectiles[0].body.w == BanWidth && ranged.projectiles[0].body.h == BanHeight, "Ban image collision size mismatch");
    World ban; ban.level.enemies.clear(); ban.player.body = {50,200,24,28}; ban.hurtCooldown = 1;
    ban.projectiles.push_back({ban.player.body,0,0,2,true,ShotKind::Ban});
    updateCombat(ban, {}, ban.player, FixedStep);
    require(ban.dead && ban.health == 0 && ban.banPending, "Ban must bypass health and immunity");
    World stun; stun.player.body = {50,200,24,28};
    stun.projectiles.push_back({stun.player.body,0,0,2,true,ShotKind::Equation});
    updateCombat(stun, {}, stun.player, FixedStep);
    require(stun.player.stun == 10 && stun.health == World::MaxHealth, "GPT must stun for ten seconds without damage");
    const float oldX = stun.player.body.x; stun.update({false,true,true,true},FixedStep);
    require(stun.player.body.x == oldX && stun.player.stun < 10, "Stun must block movement and count down");
    stun.player.body = {60,522,24,28}; stun.player.vy = 0;
    steps(stun, 1201); require(stun.player.stun == 0 && !stun.dead, "Stun must end after ten seconds");
    stun.update({false,true},FixedStep); require(stun.player.body.x > 60, "Movement did not resume after stun");
    World axe; axe.player.body = {60,200,24,28};
    axe.level.enemies = {{{80,200,26,28},70,100,0,1,true,2,1.4f}};
    updateCombat(axe,{},axe.player,FixedStep);
    require(axe.dead && axe.health == 0, "Grok axe must kill a one-health player");
    // 一滴血、致命伤害和重生恢复均由独立血量系统管理。
    World blood; require(blood.health == 1, "Initial health must be one");
    blood.hurt(L"测试受伤");
    require(blood.health == 0 && blood.dead && blood.deaths == 1, "Single hit must kill");
    blood.hurt(L"重复死亡"); require(blood.deaths == 1, "Death must be idempotent");
    blood.respawn(); require(blood.health == 1 && blood.hurtCooldown == 0, "Respawn must restore health");

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
        require(w.level.theme == i && w.level.name.find(names[i]) != std::wstring::npos, "Five chapters must have exact names and distinct themes");
        require(!w.dead && !w.won && w.player.body.x == 60, "Loading a chapter must start cleanly");
    }
    // 新章节保持主题循环、地图递增，所有敌人形象均来自三选一。
    for (int i = 0; i < 15; ++i) {
        World expanded; expanded.load(i);
        require(expanded.chapter == i && expanded.level.width > 3600, "Expanded chapter missing");
        for (const auto& enemy : expanded.level.enemies) require(enemy.avatar >= 0 && enemy.avatar < 3, "Invalid AI monster");
    }
    Menu cards; cards.page = Page::Levels;
    // 覆盖所有卡片文字，防止扩充关卡后旧名称数组发生越界。
    for (int i = 0; i < 15; ++i)
        require(cards.itemText(i) == std::to_wstring(i + 1) + L" 关", "Level card title invalid");
    require(cards.itemText(15) == L"返回游戏模式", "Back card title invalid");
    require(cards.itemText(-1).empty() && cards.itemText(16).empty(), "Invalid card index accepted");
    cards.selected = 0; cards.hover(165,211); require(cards.selected == 0, "Card gap changed selection");
    // 定时炮台必须水平发射；重生会清除弹丸与发射计时。
    World cannon; cannon.load(5);
    require(!cannon.level.cannons.empty(), "Chapter six cannon missing");
    const auto gun = cannon.level.cannons.front();
    cannon.player.body = {gun.body.x - 200, gun.body.y - 100, 24, 28};
    cannon.level.cannons.front().timer = gun.period - FixedStep;
    const Player beforeGun = cannon.player;
    require(updateCombat(cannon, {}, beforeGun, FixedStep), "Cannon update killed distant player");
    require(!cannon.projectiles.empty() && cannon.projectiles.back().hostile && cannon.projectiles.back().vy == 0 && cannon.projectiles.back().vx < 0, "Cannon must fire horizontally");
    cannon.die(L"测试复位"); cannon.respawn();
    require(cannon.projectiles.empty() && cannon.level.cannons.front().timer == 0, "Cannon reset failed");
    // 隐藏拳头脚踩即弹飞，冷却期间不能连续触发，重生重新隐藏。
    World punch; punch.load(6); require(!punch.level.fists.empty(), "Chapter seven fist missing");
    const auto pad = punch.level.fists.front().body;
    punch.player.body = {pad.x, pad.bottom() - 28, 24, 28}; punch.player.grounded = true;
    const Player beforePunch = punch.player;
    require(updateCombat(punch, {}, beforePunch, FixedStep), "Fist update failed");
    require(punch.player.vy == -900 && !punch.player.grounded && punch.level.fists.front().timer > 1, "Fist did not launch player");
    punch.die(L"测试复位"); punch.respawn();
    require(punch.level.fists.front().timer == 0, "Fist did not hide on respawn");
    Menu menu; menu.page = Page::Modes; menu.selected = 1;
    require(menu.activate() == MenuAction::Start && menu.endlessMode, "Endless mode must start directly");
    menu.home(); menu.page = Page::Levels; menu.selected = 4;
    const int oldUnlocked = account.unlocked;
    require(menu.activate() == MenuAction::None, "Locked chapter must reject activation");
    account.unlocked = 5;
    require(menu.activate() == MenuAction::Start && menu.chosenLevel == 4 && !menu.endlessMode, "Campaign selection must select the boss chapter");
    account.unlocked = oldUnlocked;
    // 第二组：断桥踩踏后自动坍塌，重生恢复；锯轮碰撞致命。
    World bridge; bridge.load(1); bridge.player.body = {365, 462, 24, 28};
    steps(bridge, 90);
    require(bridge.level.fragile[0].collapsed && bridge.player.body.y > 462, "Fragile bridge must collapse and let player fall");
    bridge.die(L"验证断桥重生"); bridge.respawn();
    require(!bridge.level.fragile[0].collapsed, "Respawn must restore fragile platforms");
    bridge.health = 1; bridge.player.body = bridge.level.saws[0].body;
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
    World laser; laser.load(3); laser.health = 1; laser.player.body = {430, 462, 24, 28}; steps(laser, 1);
    require(laser.dead, "Active laser must kill player");
    laser.restart(); laser.elapsed = 2; laser.player.body = {430, 462, 24, 28}; steps(laser, 1);
    require(!laser.dead, "Inactive laser must allow passage");
    // 第五组：BOSS 存活时锁定终点，命中最后一点生命后开放出口，弹幕仍可伤人。
    World boss; boss.load(4); boss.player.body = boss.level.goal; steps(boss, 1);
    require(!boss.won, "Living boss must lock goal");
    boss.restart(); boss.bossHealth = 1; boss.player.body = {2070, 332, 24, 28};
    steps(boss, 25, {false, false, false, true});
    require(boss.bossHealth == 0 && !boss.dead && boss.scoring.total() >= 1000, "Shooting must defeat boss and grant reward");
    boss.player.body = boss.level.goal; steps(boss, 1);
    require(boss.won, "Defeated boss must unlock goal");
    boss.restart(); boss.player.body = {2070, 332, 24, 28};
    boss.health = 1;
    boss.projectiles.push_back({boss.player.body, 0, 0, 1, true}); steps(boss, 1);
    require(boss.dead, "Hostile boss bullet must kill player");
    // 第六组：真实移动越过十二段无尽平台，验证生成会延伸且不会意外通关。
    World endless; endless.load(0, true);
    const float initialWidth = endless.level.width;
    steps(endless, 65, {false, true, false});
    for (std::size_t i = 1; i <= 24; ++i) {
        endless.extendEndless();
        const float target = endless.level.platforms[i].x + 24;
        require(travelTo(endless, target), "Endless mixed trap route blocked");
        require(!endless.dead && endless.player.body.x >= target - 5, "Generated endless platform must be reachable");
        baitNearby(endless); // 根据新的近距离阈值主动引诱，等待整组缩回再出发。
    }
    require(endless.level.width > initialWidth && !endless.won && endless.scoring.total() > 0, "Endless generation must continue with stable scoring");
    const int score = endless.scoring.total();
    endless.extendEndless(); steps(endless, 1);
    require(endless.scoring.total() == score, "Generation must not award old platforms again");
    endless.restart();
    require(endless.endless && endless.scoring.total() == 0 && endless.player.body.x == 60, "Endless restart must reset run but keep mode");
    // 第七组：十五关实际配置均使用真实等待、跳跃、射击走到终点，包含新增高路的影响。
    for (int chapter = 0; chapter < 15; ++chapter) {
        World route; route.load(chapter);
        baitNearby(route);
        for (float target : {365.0f, 555.0f, 780.0f, 970.0f, 1180.0f, 1410.0f, 1640.0f, 1870.0f, 2080.0f}) {
            if (!travelTo(route, target)) throw std::runtime_error("Campaign route blocked: chapter=" + std::to_string(chapter + 1) + ", target=" + std::to_string(static_cast<int>(target)) + ", x=" + std::to_string(route.player.body.x) + ", dead=" + std::to_string(route.dead));
        }
        if (chapter % 5 == 4) {
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
        for (std::size_t i = 10; i < route.level.platforms.size(); ++i)
            require(travelTo(route, route.level.platforms[i].x + 25), "Expanded route blocked");
        steps(route, 60, {false, true, false, true});
        require(route.won && !route.dead, "Each campaign chapter must reach its actual finish");
    }
}
}
