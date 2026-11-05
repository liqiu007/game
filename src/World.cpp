// 模块说明：规则层实现。update() 是整个游戏的“一步物理”，按固定 1/120 秒推进，顺序不可随意调换：
// 水平移动 → 跳跃与重力 → 平台/脆弱平台碰撞 → 镜头 → 地面尖刺 → 倒挂与夹击 → 机关（激光/锯轮/炮台/敌人/子弹）
// → 终点判定 → 计时与积分。所有死亡都走 hurt()/die()，保证结算与解谜机关状态一致。
// 原 blood.cpp 的三段血量逻辑（resetBlood/updateBlood/hurt）已并入本文件末尾，blood.cpp 不再存在。
// 规则层实现：建立关卡、更新物理、处理机关和死亡，再计算积分与进度。
#include "World.h"
#include "SpecialTraps.h"
#include "Combat.h"
#include "AdventureMechanics.h"
#include <algorithm>

namespace game {
bool overlaps(const Rect& a, const Rect& b) {
    // 分别检查水平与垂直区间，两个方向都相交才发生重叠。
    return a.x < b.right() && a.right() > b.x && a.y < b.bottom() && a.bottom() > b.y;
}

Level Level::tutorial() {
    // 坐标原点在左上角，Y 向下增加。依次配置平台、尖刺、存档点与终点，
    // 最后配置出生位置、对应分值及地图宽度；修改数组时保持分值索引对应。
    return {
        {{0, 550, 275, 90}, {340, 490, 155, 24}, {540, 430, 150, 24},
         {735, 365, 165, 24}, {945, 425, 170, 24}, {1160, 480, 180, 24},
         {1390, 415, 180, 24}, {1620, 350, 180, 24}, {1850, 420, 160, 24}, {2055, 360, 185, 24}},
        {{180, 526, 24, 24}, {204, 526, 24, 24}, {625, 406, 24, 24}, {649, 406, 24, 24},
         {1245, 456, 24, 24}, {1269, 456, 24, 24}, {1705, 326, 24, 24}, {1729, 326, 24, 24}},
        {{555, 386, 24, 44}}, {2170, 308, 32, 52}, 60, 522,
        {100, 100, 100, 100, 100, 100, 100, 100}, {0, 150, 200, 250, 200, 250, 300, 300, 300, 350}, 2240,
        // 普通倒挂尖刺放在跨越路线高处；夹击上排和下方第 4、5 号尖刺联动。
        {{{1040, 150, 24, 48}, TrapStyle::Ceiling}, {{1064, 150, 24, 48}, TrapStyle::Ceiling},
         {{1088, 150, 24, 48}, TrapStyle::Ceiling}, {{1245, 230, 24, 24}, TrapStyle::Crusher, 4, 120, 0},
         {{1269, 230, 24, 24}, TrapStyle::Crusher, 5, 120, 0}}
    };
}
// 创建世界后按出生点放置角色，并准备所有尖刺组。
World::World(std::uint32_t seed) : spikeRandom(seed) { resetPlayer(); }
float World::cameraX() const {
    // 让角色位于画面约 45% 处，再裁剪镜头范围；窄地图不滚动。
    return std::clamp(player.body.x + player.body.w / 2 - ScreenWidth * 0.45f,
        0.0f, std::max(0.0f, level.width - ScreenWidth));
}
bool World::respawn() {
    // 扣费与复位作为一次操作：连点、活着时调用或体力耗尽都不会多扣分。
    if (!canRespawn()) return false;
    if (!endless) { --stamina; scoring.spendRespawn(); }
    else { for (auto& coin : level.coins) coin.collected = false; scoring.resetCoins(); }
    resetPlayer();
    return true;
}
void World::resetPlayer() {
    // 重生恢复战斗机关，积分记录仍保留，避免靠反复重生刷分。
    for (auto& cannon : level.cannons) cannon.timer = 0;
    for (auto& fist : level.fists) fist.timer = 0;
    projectiles.clear(); bossHealth = 12; bossTimer = 0; shootCooldown = 0; facing = 1;
    for (auto& f : level.fragile) { f.timer = 0.65f; f.touched = false; f.collapsed = false; }
    for (auto& e : level.enemies) { e.body.x = e.left; e.alive = true; e.direction = 1; e.attackTimer = e.axeFlash = 0; }
    // 第一步：复位机关与死亡标记，保留本局累计积分和存档点。
    spikeActivity.reset(level.spikes, spikeRandom);
    resetAdventure(*this);
    resetBlood();
    dead = false;
    deathBurst.clear();
    deathReason.clear();
    // 第二步：重置速度和跳跃次数，在出生点或已激活的存档点放置角色。
    player = Player{};
    if (checkpointActive && checkpointIndex < level.checkpoints.size()) {
        const Rect& start = level.checkpoints[checkpointIndex];
        player.body.x = start.x;
        player.body.y = start.bottom() - player.body.h;
    } else {
        player.body.x = level.spawnX;
        player.body.y = level.spawnY;
    }
    // 第三步：检查脚下是否有平台，避免重生被错误识别成一次新落地得分。
    for (const auto& platform : level.platforms) {
        if (player.body.bottom() == platform.y && player.body.right() > platform.x && player.body.x < platform.right())
            player.grounded = true;
    }
}
void World::die(const wchar_t* reason) {
    // 已死亡或已通关不重复处理；记录原因、死亡次数和分数快照。
    if (dead || won) return;
    health = 0;
    dead = true;
    // 深渊死亡将喷溅位置限制在画面底部，让反馈仍可见。
    deathBurst.emit(player.body.x + player.body.w / 2,
        std::clamp(player.body.y + player.body.h / 2, 8.0f, ScreenHeight - 16.0f));
    deathReason = reason;
    ++deaths;
    scoring.settleDeath();
}
void World::restart() {
    // 根据当前模式重新构建关卡，无尽模式也会清除已生成的旧路段。
    load(chapter, endless);
}
void World::update(const Input& requestedInput, float dt) {
    // 第一步：结算后冻结世界；正常更新时保存上一步状态用于判断新得分。
    if (won || dead) return;
    // 眩晕仅屏蔽操作，重力和机关仍推进；暂停时不会倒计时。
    Input input = requestedInput;
    if (player.stun > 0) { input = Input{}; player.jumpBuffer = 0; }
    player.stun = std::max(0.0f, player.stun - dt);
    updateBlood(dt);
    extendEndless();
    const Player before = player;
    elapsed += dt;
    advanceAdventure(*this, dt);
    // 第二步：把方向输入转换成水平速度；只有剩余跳跃次数时才响应跳跃。
    constexpr float Speed = 250, Gravity = 1550;
    player.vx = (static_cast<int>(input.right) - static_cast<int>(input.left)) * Speed;
    updateJumpFeel(player, input, dt);
    // 第三步：累积重力并限制下落速度，控制每个固定步长的移动距离。
    player.vy = std::min(player.vy + Gravity * dt, 800.0f);
    // S 在空中转为快速下落；不穿透地面，W 与 S 同时按下时优先起跳。
    if (input.fastFall && !player.grounded && !input.jump) player.vy = std::max(player.vy, 600.0f);

    // 第四步：先水平移动并推出相交平台，再限制在地图范围内。
    player.body.x += player.vx * dt;
    for (std::size_t i = 0; i < level.platforms.size(); ++i) {
        const auto& tile = level.platforms[i];
        if (!platformSolid(i)) continue;
        if (!overlaps(player.body, tile)) continue;
        if (player.vx > 0) player.body.x = tile.x - player.body.w;
        else if (player.vx < 0) player.body.x = tile.right();
        player.vx = 0;
    }
    player.body.x = std::clamp(player.body.x, 0.0f, std::max(0.0f, level.width - player.body.w));
    // 第五步：处理垂直移动。落地时补充跳跃次数，撞到平台底面时停止上升。
    const float previousBottom = player.body.bottom();
    const float landingSpeed = player.vy;
    player.body.y += player.vy * dt;
    player.grounded = false;
    player.standingSky = -1;
    for (std::size_t i = 0; i < level.platforms.size(); ++i) {
        const auto& tile = level.platforms[i];
        if (!platformSolid(i)) continue;
        if (!overlaps(player.body, tile)) continue;
        if (player.vy > 0) {
            player.body.y = tile.y - player.body.h;
            player.grounded = true;
            player.jumpsLeft = 2;
        } else if (player.vy < 0) player.body.y = tile.bottom();
        player.vy = 0;
    }
    // 单向高路与移动平台只阻挡向下落地，随后统一恢复跳跃次数和落地动画。
    landOnSky(*this, previousBottom);
    if (player.grounded) {
        player.jumpCut = 0;
        if (!before.grounded && landingSpeed > 250) player.squash = 1;
    }

    scoring.advanceDistance(player.body.x - level.spawnX);
    // 第六步：先更新整组尖刺的动态尺寸，再用相同外框检查危险碰撞。
    spikeActivity.update(player.body, level.spikes, dt);
    for (std::size_t i = 0; i < level.spikes.size(); ++i) {
        const auto spike = spikeActivity.bounds(i, level.spikes[i]);
        // 碰撞矩形略小于绘制三角形外框，减少边缘接触造成的误伤。
        const Rect hit{spike.x + 5, spike.y + 6, spike.w - 10, spike.h - 6};
        if (overlaps(player.body, hit)) {
            hurt(spike.h > level.spikes[i].h + 1 ? L"你被突然伸长的尖刺刺中了" : L"你撞上了尖刺");
            return;
        }
    }
    // 第七步：检查倒挂尖刺和上下夹击；上排尺寸跟随同一组进度，死亡当步不加分。
    for (const auto& trap : level.overheadTraps) if (hitOverheadTrap(trap, *this)) return;
    if (!updateCombat(*this, input, before, dt)) return;
    if (!finishAdventure(*this, dt)) return;
    // 第八步：越出底部也算死亡；死亡立即返回，不在同一步额外发放积分。
    if (player.body.y > ScreenHeight + 80) { die(L"你坠入了深渊"); return; }
    // 第九步：存活时更新得分，再激活存档点或触发终点。
    scoring.update(before, player, level);
    // 只有存活时接触金币才发分，死亡当步不领取；已吃金币不会因重生再出现。
    for (std::size_t i = 0; i < level.coins.size(); ++i) {
        auto& coin = level.coins[i];
        if (!coin.collected && overlaps(player.body, coin.body)) { coin.collected = true; scoring.collectCoin(i); }
    }
    for (std::size_t i = 0; i < level.checkpoints.size(); ++i) {
        if (!overlaps(player.body, level.checkpoints[i])) continue;
        // 只向更靠后的旗帜推进，回头触碰旧旗帜不会覆盖已经取得的进度。
        if (!checkpointActive || level.checkpoints[i].x > level.checkpoints[checkpointIndex].x) {
            checkpointIndex = i;
            checkpointActive = true;
        }
    }
    // 终点判定：第 5、10 关的 BOSS 可以绕过，第 15 关必须击败 BOSS 才开门。
    if (!endless && overlaps(player.body, level.goal) && (!level.boss || !level.bossGate || bossHealth <= 0)) won = true;
}
// 血量系统（原 blood.cpp 的三段逻辑并入本文件）：初始一滴血，受伤后 1 秒无敌，血量归零立即死亡。
void World::resetBlood() { health = MaxHealth; hurtCooldown = 0; }
void World::updateBlood(float dt) { hurtCooldown = std::max(0.0f, hurtCooldown - dt); }
void World::hurt(const wchar_t* reason, int damage) {
    // 同一次接触和同时命中的机关在无敌期内不会连续扣血。
    if (dead || won || hurtCooldown > 0) return;
    health = std::max(0, health - std::max(0, damage));
    hurtCooldown = 1.0f;
    if (health == 0) die(reason);
}
}

