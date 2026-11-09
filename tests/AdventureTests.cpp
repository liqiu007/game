// 联合版本回归：验证真实键盘长按语义、跳跃容错、支线机关以及星羽完整获取路线。
#include "AdventureMechanics.h"
#include "Controls.h"
#include <cmath>
#include <stdexcept>
#include <string>

namespace game {
namespace {
void ensure(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void ticks(World& w, int count, Input input = {}) { for (int i = 0; i < count; ++i) w.update(input, FixedStep); }
World emptyWorld() {
    // 独立物理测试只移除无关危险；完整关卡路线测试使用实际关卡，不移除任何机关。
    World w; w.level = Level{}; w.level.platforms = {{0, 550, 275, 90}};
    w.level.spawnX = 60; w.level.spawnY = 522; w.level.goal = {-1000, 0, 1, 1};
    w.level.checkpoints = {{-1000, 0, 1, 1}}; return w;
}
bool reachSky(World& world, int index) {
    // 搜索等待和二段跳时机，以真实 WASD 输入抵达指定平台，不改变角色位置或生命。
    for (int delay = 0; delay <= 240; delay += 16) {
        for (int second : {-1, 18, 30, 42, 54}) {
            World candidate = world; ticks(candidate, delay);
            for (int t = 0; t < 210 && !candidate.dead; ++t) {
                const auto& target = candidate.level.skyPlatforms[index].body;
                const float destination = target.x + target.w * 0.5f - 12;
                candidate.update({candidate.player.body.x > destination + 3, candidate.player.body.x < destination - 3,
                    t == 0 || t == second, false, false, true}, FixedStep);
                if (t > 12 && candidate.player.standingSky == index) { world = candidate; return true; }
            }
        }
    }
    return false;
}
}
void runAdventureTests() {
    // 第一组：短按 W 的最高点更低，长按保持原有路线高度，松开不会反复截断。
    World shortJump = emptyWorld(), fullJump = emptyWorld();
    float shortTop = 522, fullTop = 522;
    for (int i = 0; i < 90; ++i) {
        shortJump.update({false, false, i == 0, false, false, i < 2}, FixedStep);
        fullJump.update({false, false, i == 0, false, false, true}, FixedStep);
        shortTop = std::min(shortTop, shortJump.player.body.y); fullTop = std::min(fullTop, fullJump.player.body.y);
    }
    ensure(shortTop > fullTop + 35, "Short W press must produce a lower jump than holding W");
    bool keys[256]{}, previous[256]{}; keys['W'] = true;
    ensure(mapControls(keys, previous).jumpHeld, "Keyboard must pass W held state into physics");
    keys['W'] = false; ensure(!mapControls(keys, previous).jumpHeld, "Releasing W must reach physics");
    // 第二组：离边后短时间仍能地面跳，过期只留空中跳；用完次数后可缓存落地跳。
    World edge = emptyWorld(); edge.player.body.x = 274; ticks(edge, 6, {false, true});
    edge.update({false, false, true}, FixedStep);
    ensure(edge.player.vy < 0 && edge.player.jumpsLeft == 1, "Coyote jump must preserve the air jump");
    edge = emptyWorld(); edge.player.body.x = 274; ticks(edge, 20, {false, true});
    edge.update({false, false, true}, FixedStep);
    ensure(edge.player.jumpsLeft == 0, "After coyote time only the air jump remains");
    World buffered = emptyWorld(); buffered.player.body.y = 510; buffered.player.vy = 300;
    buffered.player.grounded = false; buffered.player.jumpsLeft = 0;
    buffered.update({false, false, true}, FixedStep); ticks(buffered, 7);
    ensure(buffered.player.vy < 0 && buffered.player.jumpsLeft == 1, "Pressing W before landing must buffer a jump");
    // 第三组：上升穿过单向平台、下落落在顶面，移动平台携带玩家且离开后停止携带。
    World mover = emptyWorld(); mover.level.skyPlatforms = {{{40, 500, 100, 16}, 40, 500, 60, -20, 4}};
    mover.player.body = {60, 472, 24, 28}; mover.player.standingSky = 0;
    ticks(mover, 60);
    ensure(mover.player.standingSky == 0 && std::abs(mover.player.body.x - mover.level.skyPlatforms[0].body.x - 20) < 0.1f,
        "Moving platform must carry its rider horizontally and vertically");
    const float departure = mover.player.body.x; mover.update({false, false, true}, FixedStep); ticks(mover, 12);
    ensure(mover.player.standingSky == -1 && std::abs(mover.player.body.x - departure) < 1, "Jumping must release moving-platform attachment");
    World oneWay = emptyWorld(); oneWay.level.skyPlatforms = {{{40, 480, 110, 16}, 40, 480}};
    oneWay.update({false, false, true}, FixedStep); ticks(oneWay, 25);
    ensure(oneWay.player.body.y < 452 && oneWay.player.vy < 0, "One-way platform must allow upward passage");
    ticks(oneWay, 65); ensure(oneWay.player.standingSky == 0, "One-way platform must catch a falling player");
    mover.level.skyPlatforms[0].timed = true; mover.elapsed = 3.1f;
    mover.player.body = {mover.level.skyPlatforms[0].body.x + 20, mover.level.skyPlatforms[0].body.y - 28, 24, 28};
    mover.player.grounded = true; mover.player.standingSky = 0; mover.player.vy = 0; ticks(mover, 1);
    ensure(!mover.player.grounded && mover.player.standingSky == -1, "Inactive timed platform must release its rider");
    // 第四组：弹簧从上方触发，落刺先预警、再致命，重生恢复机关。
    World spring = emptyWorld(); spring.level.springs = {{{60, 542, 30, 8}}}; ticks(spring, 1);
    ensure(spring.player.vy == -820 && spring.player.jumpsLeft == 1 && !spring.player.grounded, "Spring must launch and retain an air jump");
    World falling = emptyWorld(); falling.health = 1; falling.level.fallingSpikes = {{{60, 150, 24, 32}, 150}};
    falling.player.body = {60, 230, 24, 28}; falling.player.grounded = false;
    finishAdventure(falling, FixedStep);
    ensure(falling.level.fallingSpikes[0].phase == FallingPhase::Warning && !falling.dead, "Falling spike must warn before moving");
    for (int i = 0; i < 100 && !falling.dead; ++i) finishAdventure(falling, FixedStep);
    ensure(falling.dead, "Falling spike must collide after warning");
    falling.respawn(); ensure(falling.level.fallingSpikes[0].phase == FallingPhase::Idle, "Respawn must reset falling spike");
    // 第五组：每关从真实出生点跳上起始高路、踩弹簧，再经后续平台拾取星羽。
    for (int chapter = 0; chapter < 5; ++chapter) {
        World route; route.load(chapter);
        for (int t = 0; t < 180; ++t) {
            route.update({false, false, t == 0 || t == 36}, FixedStep);
            if (t > 36 && route.player.standingSky == 0) break;
        }
        ensure(route.player.standingSky == 0, "High route entrance must be reachable from spawn");
        ticks(route, 6, {false, true});
        ensure(route.player.vy < -700, "High route must actually use the spring");
        for (int platform = 1; platform <= 3; ++platform)
            if (!reachSky(route, platform)) throw std::runtime_error("Star route blocked: chapter=" + std::to_string(chapter + 1) + ", platform=" + std::to_string(platform));
        // 平台末端跳起触及星羽；按实际位置向奖励中心靠近。
        for (int t = 0; t < 100 && !route.level.feathers[0].collected && !route.dead; ++t) {
            const float target = route.level.feathers[0].body.x - 3;
            route.update({route.player.body.x > target + 2, route.player.body.x < target - 2, t == 0}, FixedStep);
        }
        ensure(!route.dead && route.level.feathers[0].collected, "Each actual star route must reach its feather");
        // 收集后走出高台右沿并落回存档平台，验证支线有可用出口而非死胡同。
        // 存档平台的高度随关卡开局的起伏变化，这里按它当前的实际高度校验落点。
        const float landing = route.level.platforms[2].y;
        for (int t = 0; t < 180 && !route.dead; ++t) {
            route.update({false, route.player.body.x < 590, false, false, true}, FixedStep);
            if (route.player.grounded && route.player.standingSky == -1) break;
        }
        ensure(!route.dead && route.player.grounded && std::abs(route.player.body.bottom() - landing) < 0.1f,
            "Feather route must rejoin the main checkpoint platform safely");
        const int score = route.scoring.total(); ensure(score >= 500, "Feather must grant 500 points");
        route.die(L"test"); route.respawn();
        ensure(route.level.feathers[0].collected && route.scoring.total() == score - 100, "Respawn must preserve feather and deduct 100 points");
        route.scoring.collectFeather(0); ensure(route.scoring.total() == score - 100, "Feather cannot award twice");
        route.restart(); ensure(!route.level.feathers[0].collected && route.scoring.total() == 0, "Restart must reset feather and score");
    }
}
}
