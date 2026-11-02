// 规则层实现：建立关卡、更新物理、处理机关和死亡，再计算积分与进度。
#include "World.h"
#include "SpecialTraps.h"
#include "Combat.h"
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
        {555, 386, 24, 44}, {2170, 308, 32, 52}, 60, 522,
        {100, 100, 100, 100, 100, 100, 100, 100}, {0, 150, 200, 250, 200, 250, 300, 300, 300, 350}, 2240,
        // 普通倒挂尖刺放在跨越路线高处；夹击上排和下方第 4、5 号尖刺联动。
        {{{1040, 150, 24, 48}, TrapStyle::Ceiling}, {{1064, 150, 24, 48}, TrapStyle::Ceiling},
         {{1088, 150, 24, 48}, TrapStyle::Ceiling}, {{1245, 230, 24, 24}, TrapStyle::Crusher, 4, 120, 0},
         {{1269, 230, 24, 24}, TrapStyle::Crusher, 5, 120, 0}}
    };
}
// 创建世界后按出生点放置角色，并准备所有尖刺组。
World::World() { respawn(); }
float World::cameraX() const {
    // 让角色位于画面约 45% 处，再裁剪镜头范围；窄地图不滚动。
    return std::clamp(player.body.x + player.body.w / 2 - ScreenWidth * 0.45f,
        0.0f, std::max(0.0f, level.width - ScreenWidth));
}
void World::respawn() {
    // 重生恢复战斗机关，积分记录仍保留，避免靠反复重生刷分。
    projectiles.clear(); bossHealth = 12; bossTimer = 0; shootCooldown = 0; facing = 1;
    for (auto& f : level.fragile) { f.timer = 0.65f; f.touched = false; f.collapsed = false; }
    for (auto& e : level.enemies) { e.body.x = e.left; e.alive = true; e.direction = 1; }
    // 第一步：复位机关与死亡标记，保留本局累计积分和存档点。
    spikeActivity.reset();
    dead = false;
    deathReason.clear();
    // 第二步：重置速度和跳跃次数，在出生点或已激活的存档点放置角色。
    player = Player{};
    player.body.x = checkpointActive ? level.checkpoint.x : level.spawnX;
    player.body.y = checkpointActive ? level.checkpoint.bottom() - player.body.h : level.spawnY;
    // 第三步：检查脚下是否有平台，避免重生被错误识别成一次新落地得分。
    for (const auto& platform : level.platforms) {
        if (player.body.bottom() == platform.y && player.body.right() > platform.x && player.body.x < platform.right())
            player.grounded = true;
    }
}
void World::die(const wchar_t* reason) {
    // 已死亡或已通关不重复处理；记录原因、死亡次数和分数快照。
    if (dead || won) return;
    dead = true;
    deathReason = reason;
    ++deaths;
    scoring.settleDeath();
}
void World::restart() {
    // 根据当前模式重新构建关卡，无尽模式也会清除已生成的旧路段。
    load(chapter, endless);
}
void World::update(const Input& input, float dt) {
    // 第一步：结算后冻结世界；正常更新时保存上一步状态用于判断新得分。
    if (won || dead) return;
    extendEndless();
    const Player before = player;
    elapsed += dt;
    // 第二步：把方向输入转换成水平速度；只有剩余跳跃次数时才响应跳跃。
    constexpr float Speed = 250, Gravity = 1550, JumpSpeed = 540;
    player.vx = (static_cast<int>(input.right) - static_cast<int>(input.left)) * Speed;
    if (input.jump && player.jumpsLeft > 0) {
        player.vy = -JumpSpeed;
        --player.jumpsLeft;
        player.grounded = false;
    }
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
    player.body.y += player.vy * dt;
    player.grounded = false;
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
    // 直接走出平台边缘只保留一次空中跳跃，避免获得额外的地面跳跃。
    if (!player.grounded && player.vy > 0 && player.jumpsLeft == 2) player.jumpsLeft = 1;

    // 第六步：先更新整组尖刺的动态尺寸，再用相同外框检查危险碰撞。
    spikeActivity.update(player.body, level.spikes, dt);
    for (std::size_t i = 0; i < level.spikes.size(); ++i) {
        const auto spike = spikeActivity.bounds(i, level.spikes[i]);
        // 碰撞矩形略小于绘制三角形外框，减少边缘接触造成的误伤。
        const Rect hit{spike.x + 5, spike.y + 6, spike.w - 10, spike.h - 6};
        if (overlaps(player.body, hit)) {
            die(spike.h > level.spikes[i].h + 1 ? L"你被突然伸长的尖刺刺中了" : L"你撞上了尖刺");
            return;
        }
    }
    // 第七步：检查倒挂尖刺和上下夹击；上排尺寸跟随同一组进度，死亡当步不加分。
    for (const auto& trap : level.overheadTraps) if (hitOverheadTrap(trap, *this)) return;
    if (!updateCombat(*this, input, before, dt)) return;
    // 第八步：越出底部也算死亡；死亡立即返回，不在同一步额外发放积分。
    if (player.body.y > ScreenHeight + 80) { die(L"你坠入了深渊"); return; }
    // 第九步：存活时更新得分，再激活存档点或触发终点。
    scoring.update(before, player, level);
    if (overlaps(player.body, level.checkpoint)) checkpointActive = true;
    if (!endless && overlaps(player.body, level.goal) && (!level.boss || bossHealth <= 0)) won = true;
}
}
