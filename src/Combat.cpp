// 战斗与复合陷阱：坍塌平台、锯轮、巡逻敌人、激光和 BOSS 弹幕。
#include "Combat.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace game {
bool laserActive(const Laser& laser, float elapsed) {
    // 周期前半段亮起为危险状态，其余时间提供安全通过的窗口。
    return std::fmod(elapsed + laser.offset, laser.period) < laser.activeSeconds;
}
bool updateCombat(World& w, const Input& input, const Player& before, float dt) {
    // 第一步：踩过的断桥持续倒计时，离开桥面也不会取消坍塌。
    for (auto& f : w.level.fragile) {
        const auto& p = w.level.platforms[f.index];
        if (w.player.grounded && std::abs(w.player.body.bottom() - p.y) < 0.1f &&
            w.player.body.right() > p.x && w.player.body.x < p.right()) f.touched = true;
        if (f.touched && !f.collapsed) { f.timer -= dt; if (f.timer <= 0) f.collapsed = true; }
    }
    // 第二步：更新锯轮的位置，检查开启中的激光和锯轮是否击中角色。
    for (auto& saw : w.level.saws) {
        saw.body.x = saw.originX + std::sin(w.elapsed * saw.rate) * saw.range;
        if (overlaps(w.player.body, saw.body)) { w.hurt(L"你碰到了移动锯轮"); if (w.dead) return false; }
    }
    for (const auto& laser : w.level.lasers)
        if (laserActive(laser, w.elapsed) && overlaps(w.player.body, laser.body)) { w.hurt(L"你被激光击中了"); if (w.dead) return false; }
    // 第三步：敌人在各自平台来回巡逻；从上往下踩中会消灭敌人并让角色弹起。
    for (auto& enemy : w.level.enemies) {
        if (!enemy.alive) continue;
        enemy.body.x += enemy.speed * enemy.direction * dt;
        if (enemy.body.x <= enemy.left || enemy.body.x >= enemy.right) {
            enemy.body.x = std::clamp(enemy.body.x, enemy.left, enemy.right); enemy.direction *= -1;
        }
        if (overlaps(w.player.body, enemy.body)) {
            if (w.player.vy > 0 && before.body.bottom() <= enemy.body.y + 6) {
                enemy.alive = false; w.player.vy = -360; w.player.jumpsLeft = 1;
                w.scoring.defeatEnemy(static_cast<std::size_t>(&enemy - w.level.enemies.data()));
            } else if (enemy.avatar != 2) { w.hurt(L"你撞上了巡逻敌人"); if (w.dead) return false; }
        }
        if (!enemy.alive) continue;
        enemy.axeFlash = std::max(0.0f, enemy.axeFlash - dt);
        const float dx = w.player.body.x - enemy.body.x;
        const bool rangedBan = enemy.avatar == 0;
        if (std::abs(dx) > (rangedBan ? 900 : 520) ||
            std::abs(w.player.body.y - enemy.body.y) > (rangedBan ? 320 : 48)) continue;
        enemy.attackTimer += dt;
        const int direction = dx < 0 ? -1 : 1;
        if (enemy.avatar == 2) {
            // 斧头每次挥击只判定一次，近战覆盖前后近身范围。
            if (enemy.attackTimer >= 1.4f && std::abs(dx) < 64) {
                enemy.attackTimer = 0; enemy.axeFlash = 0.35f;
                if (overlaps(w.player.body, {enemy.body.x - 48, enemy.body.y - 12, 122, 52})) w.hurt(L"你被 Grok 的斧头击中了", 2);
                if (w.dead) return false;
            }
        } else if (enemy.attackTimer >= 4.0f) {
            enemy.attackTimer = 0;
            static std::mt19937 rng(std::random_device{}());
            const auto kind = enemy.avatar == 0 ? ShotKind::Ban : (std::uniform_int_distribution<int>(0,1)(rng) ? ShotKind::Equation : ShotKind::Smoothness);
            const float width = kind == ShotKind::Ban ? static_cast<float>(BanWidth) : (kind == ShotKind::Equation ? 150.0f : 270.0f);
            const float height = kind == ShotKind::Ban ? static_cast<float>(BanHeight) : 40.0f;
            // Claude 出手时瞄准玩家所在方向；投掷后沿固定方向飞行，可跳跃躲开。
            float vx = 130.0f * direction, vy = 0, life = 4;
            if (rangedBan) {
                const float dy = w.player.body.y - enemy.body.y;
                const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
                vx = dx / length * 240; vy = dy / length * 240; life = 5;
                enemy.axeFlash = 0.35f;
            }
            w.projectiles.push_back({{direction > 0 ? enemy.body.right() : enemy.body.x - width, enemy.body.y - height + 28, width, height}, vx, vy, life, true, kind});
        }
    }
    // 第四步：J 可连续射击，朝向跟随最后一次水平移动；射击有冷却时间。
    if (w.player.vx != 0) w.facing = w.player.vx > 0 ? 1 : -1;
    w.shootCooldown = std::max(0.0f, w.shootCooldown - dt);
    if (input.shoot && w.shootCooldown <= 0) {
        w.projectiles.push_back({{w.player.body.x + (w.facing > 0 ? 24 : -10), w.player.body.y + 10, 10, 5},
            600.0f * w.facing, 0, 2, false});
        w.shootCooldown = 0.22f;
    }
    // 第五步：BOSS 仅在竞技场附近开火，血量过半后加快弹幕频率。
    const Rect boss{2115, 288, 56, 72};
    if (w.level.boss && w.bossHealth > 0 && w.player.body.x > 1730) {
        w.bossTimer += dt;
        if (overlaps(w.player.body, boss)) { w.hurt(L"你撞上了 BOSS"); if (w.dead) return false; }
        if (w.bossTimer >= (w.bossHealth > 6 ? 1.2f : 0.7f)) {
            w.bossTimer = 0;
            const float dx = w.player.body.x - boss.x, dy = w.player.body.y - (boss.y + 32);
            const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
            for (float spread : {-45.0f, 0.0f, 45.0f})
                w.projectiles.push_back({{boss.x, boss.y + 32, 10, 10}, dx / length * 230, dy / length * 230 + spread, 4, true});
        }
    }
    // 炮台在附近按模拟时间射击，子弹只沿 X 轴飞行；暂停时计时冻结。
    for (auto& cannon : w.level.cannons) {
        if (std::abs(w.player.body.x - cannon.body.x) > 850) continue;
        cannon.timer += dt;
        if (cannon.timer >= cannon.period) {
            cannon.timer -= cannon.period;
            w.projectiles.push_back({{cannon.body.x - 12, cannon.body.y + 8, 12, 8}, 260.0f * cannon.direction, 0, 2.5f, true});
        }
    }
    // 隐藏拳头只在脚踩顶面时触发，提供一次空中跳跃以便落回平台。
    for (auto& fist : w.level.fists) {
        fist.timer = std::max(0.0f, fist.timer - dt);
        if (fist.timer <= 0 && w.player.grounded && w.player.body.right() > fist.body.x &&
            w.player.body.x < fist.body.right() && std::abs(w.player.body.bottom() - fist.body.bottom()) < 1) {
            fist.timer = 1.5f; w.player.vy = -900; w.player.grounded = false;
            w.player.jumpsLeft = 1; w.player.standingSky = -1;
            w.player.coyote = w.player.jumpCut = w.player.jumpBuffer = 0;
            w.player.stretch = 1;
        }
    }
    // 第六步：推进子弹，处理命中并删除过期对象，避免弹丸无限累积。
    for (auto& shot : w.projectiles) {
        shot.body.x += shot.vx * dt; shot.body.y += shot.vy * dt; shot.life -= dt;
        // 大型武器弹幕碰到实体地形即消失，避免穿越整张地图封锁路线。
        if (shot.kind != ShotKind::Normal) {
            for (const auto& tile : w.level.platforms) if (overlaps(shot.body, tile)) { shot.life = 0; break; }
            if (shot.life <= 0) continue;
        }
        if (shot.hostile && overlaps(shot.body, w.player.body)) { shot.life = 0;
            if (shot.kind == ShotKind::Ban) { w.banPending = true; w.die(L"你被 Claude 封禁了"); return false; }
            if (shot.kind == ShotKind::Equation || shot.kind == ShotKind::Smoothness) w.player.stun = 10;
            else w.hurt(L"你被敌方子弹击中了");
            if (w.dead) return false; }
        if (!shot.hostile) {
            for (auto& enemy : w.level.enemies)
                if (enemy.alive && overlaps(shot.body, enemy.body)) {
                    enemy.alive = false; shot.life = 0;
                    w.scoring.defeatEnemy(static_cast<std::size_t>(&enemy - w.level.enemies.data())); break;
                }
            if (shot.life > 0 && w.level.boss && w.bossHealth > 0 && overlaps(shot.body, boss)) {
                --w.bossHealth; shot.life = 0;
                if (w.bossHealth == 0) w.scoring.defeatBoss();
            }
        }
    }
    w.projectiles.erase(std::remove_if(w.projectiles.begin(), w.projectiles.end(), [](const auto& p) { return p.life <= 0; }), w.projectiles.end());
    return true;
}
}
