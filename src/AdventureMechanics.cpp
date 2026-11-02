// 冒险机关与手感适配：沿用本项目的 120 Hz 物理、WASD、死亡结算和五关主题。
#include "AdventureMechanics.h"
#include <algorithm>
#include <cmath>

namespace game {
bool skyActive(const SkyPlatform& p, float elapsed) {
    // 定时平台前三秒实体、后一秒消失；绘制也使用同一判断。
    return !p.timed || std::fmod(elapsed, p.period) < p.period * 0.75f;
}
void buildAdventureRoute(Level& level) {
    // 第一步：出生地上方有一条可主动探索的高路，主线和存档点仍可正常到达。
    const int theme = level.theme;
    level.skyPlatforms = {{{40, 380, 100, 16}, 40, 380},
        {{210, 260, 85, 16}, 210, 260}, {{355, 240, 85, 16}, 355, 240},
        {{490, 225, 90, 16}, 490, 225}};
    level.springs = {{{85, 372, 28, 8}}};
    level.feathers = {{{530, 185, 18, 26}}};
    // 第二步：每一关高路的几何和机制独立配置，奖励终点可跳回原有主线。
    if (theme == 1) {
        level.skyPlatforms[1].travelX = 45; level.skyPlatforms[1].period = 3;
        level.skyPlatforms[2].body.y = level.skyPlatforms[2].homeY = 215;
        level.skyPlatforms[3].body.y = level.skyPlatforms[3].homeY = 245;
        level.feathers[0].body.y = 205;
    } else if (theme == 2) {
        level.skyPlatforms[1].body.y = level.skyPlatforms[1].homeY = 285;
        level.skyPlatforms[2].timed = true;
        level.fallingSpikes = {{{390, 135, 24, 32}, 135}};
    } else if (theme == 3) {
        level.skyPlatforms[1].travelY = -50;
        level.skyPlatforms[2].timed = true;
        level.skyPlatforms[3].travelY = 25;
    } else if (theme == 4) {
        level.skyPlatforms[1].travelX = 40;
        level.skyPlatforms[2].travelY = -30;
        // 落刺放在平台左沿，右半边保留安全落脚点，允许观察预警后继续起跳。
        level.fallingSpikes = {{{365, 135, 24, 32}, 135}};
    }
}
void resetAdventure(World& w) {
    // 移动平台从当前位置对应的时间相位复原；站立引用随玩家一起清除。
    for (auto& p : w.level.skyPlatforms) {
        const float phase = 0.5f - 0.5f * std::cos(w.elapsed * 6.2831853f / p.period);
        p.body.x = p.homeX + p.travelX * phase; p.body.y = p.homeY + p.travelY * phase;
        p.dx = p.dy = 0;
    }
    for (auto& s : w.level.springs) s.flash = 0;
    for (auto& f : w.level.fallingSpikes) { f.body.y = f.homeY; f.speed = f.timer = 0; f.phase = FallingPhase::Idle; }
}
void advanceAdventure(World& w, float dt) {
    // 第一步：平滑往返，记录真实位移；角色站在活动平台时随平台移动。
    for (auto& p : w.level.skyPlatforms) {
        const float phase = 0.5f - 0.5f * std::cos(w.elapsed * 6.2831853f / p.period);
        const float x = p.homeX + p.travelX * phase, y = p.homeY + p.travelY * phase;
        p.dx = x - p.body.x; p.dy = y - p.body.y; p.body.x = x; p.body.y = y;
    }
    const int index = w.player.standingSky;
    if (index >= 0 && static_cast<std::size_t>(index) < w.level.skyPlatforms.size()) {
        const auto& p = w.level.skyPlatforms[index];
        if (skyActive(p, w.elapsed)) { w.player.body.x += p.dx; w.player.body.y += p.dy; }
        else { w.player.grounded = false; w.player.standingSky = -1; }
    }
    // 第二步：视觉反馈计时使用模拟时间，暂停和死亡时不会继续闪动。
    for (auto& s : w.level.springs) s.flash = std::max(0.0f, s.flash - dt);
}
void updateJumpFeel(Player& p, const Input& input, float dt) {
    // 第一步：参考上游的 0.1 秒土狼时间和 0.12 秒跳跃缓冲，按本项目高度适配。
    p.animTime += dt; p.runPhase += std::abs(p.vx) * dt * 0.055f;
    p.squash = std::max(0.0f, p.squash - dt * 5);
    p.stretch = std::max(0.0f, p.stretch - dt * 5.5f);
    p.springCooldown = std::max(0.0f, p.springCooldown - dt);
    p.jumpBuffer = input.jump ? 0.12f : std::max(0.0f, p.jumpBuffer - dt);
    p.coyote = p.grounded ? 0.1f : std::max(0.0f, p.coyote - dt);
    if (!p.grounded && p.coyote <= 0 && p.jumpsLeft == 2) p.jumpsLeft = 1;
    // 第二步：优先地面/离边起跳，其次二段跳；没有剩余次数时保留缓冲等待落地。
    if (p.jumpBuffer > 0 && (p.grounded || p.coyote > 0 || p.jumpsLeft > 0)) {
        if (p.grounded || p.coyote > 0) p.jumpsLeft = 1; else --p.jumpsLeft;
        p.vy = -540; p.grounded = false; p.standingSky = -1;
        p.coyote = p.jumpBuffer = 0; p.jumpCut = 0.13f; p.stretch = 1;
    }
    // 第三步：松开 W 只截断一次上升，短按小跳；弹簧没有此截断窗口。
    if (!input.jumpHeld && p.jumpCut > 0 && p.vy < 0) { p.vy *= 0.45f; p.jumpCut = 0; }
    p.jumpCut = std::max(0.0f, p.jumpCut - dt);
}
void landOnSky(World& w, float previousBottom) {
    // 仅从上方下落时落在单向平台；从下方起跳不撞头，多个候选取最上层。
    if (w.player.vy < 0) return;
    for (std::size_t i = 0; i < w.level.skyPlatforms.size(); ++i) {
        const auto& p = w.level.skyPlatforms[i];
        if (!skyActive(p, w.elapsed) || previousBottom > p.body.y + 0.5f ||
            w.player.body.bottom() < p.body.y || w.player.body.x >= p.body.right() || w.player.body.right() <= p.body.x) continue;
        w.player.body.y = p.body.y - w.player.body.h; w.player.vy = 0;
        w.player.grounded = true; w.player.jumpsLeft = 2; w.player.standingSky = static_cast<int>(i);
    }
}
bool finishAdventure(World& w, float dt) {
    // 第一步：弹簧必须从顶面接触，弹起时补充一次空中跳跃并设置防连触冷却。
    for (auto& s : w.level.springs) {
        if (w.player.grounded && w.player.springCooldown <= 0 && w.player.body.right() > s.body.x &&
            w.player.body.x < s.body.right() && std::abs(w.player.body.bottom() - s.body.bottom()) < 1) {
            w.player.vy = -820; w.player.grounded = false; w.player.jumpsLeft = 1;
            w.player.standingSky = -1; w.player.coyote = w.player.jumpCut = 0;
            w.player.springCooldown = 0.3f; w.player.stretch = 1; s.flash = 0.3f;
        }
    }
    // 第二步：落刺先预警 0.45 秒，再下落，碰平台停留后复位，不穿越多层地形。
    for (auto& f : w.level.fallingSpikes) {
        if (f.phase == FallingPhase::Idle) {
            if (std::abs(w.player.body.x + 12 - f.body.x - 12) < 48 &&
                w.player.body.y > f.body.bottom() && w.player.body.y < 300) {
                f.phase = FallingPhase::Warning; f.timer = 0.45f;
            }
        } else if (f.phase == FallingPhase::Warning) {
            f.timer -= dt;
            if (f.timer <= 0) { f.phase = FallingPhase::Falling; f.speed = 160; }
        } else if (f.phase == FallingPhase::Falling) {
            const float oldBottom = f.body.bottom();
            f.speed = std::min(900.0f, f.speed + 1600 * dt); f.body.y += f.speed * dt;
            const auto stopOn = [&](const Rect& p) {
                if (f.body.right() > p.x && f.body.x < p.right() && oldBottom <= p.y && f.body.bottom() >= p.y) {
                    f.body.y = p.y - f.body.h; f.phase = FallingPhase::Resting; f.timer = 1.1f;
                }
            };
            for (std::size_t i = 0; i < w.level.platforms.size(); ++i) if (w.platformSolid(i)) stopOn(w.level.platforms[i]);
            for (const auto& p : w.level.skyPlatforms) if (skyActive(p, w.elapsed)) stopOn(p.body);
            if (f.body.y > ScreenHeight + 80) { f.phase = FallingPhase::Resting; f.timer = 0.5f; }
        } else {
            f.timer -= dt;
            if (f.timer <= 0) { f.body.y = f.homeY; f.speed = 0; f.phase = FallingPhase::Idle; }
        }
        if (overlaps(w.player.body, {f.body.x + 4, f.body.y + 4, f.body.w - 8, f.body.h - 4})) {
            w.die(L"你被预警后落下的尖刺击中了"); return false;
        }
    }
    // 第三步：存活后才收集星羽；每关一枚，死亡保留，重开通过重建关卡恢复。
    for (std::size_t i = 0; i < w.level.feathers.size(); ++i) {
        auto& f = w.level.feathers[i];
        if (!f.collected && overlaps(w.player.body, f.body)) { f.collected = true; w.scoring.collectFeather(i); }
    }
    return true;
}
}
