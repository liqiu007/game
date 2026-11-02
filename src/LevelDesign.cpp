// 五关配置与无尽地图生成：关卡共享基础操作，主题、机关和通关目标各不相同。
#include "World.h"
#include "AdventureMechanics.h"
#include <algorithm>

namespace game {
Level Level::campaign(int index) {
    // 第一步：复用基础跳跃间距，按关卡加入不同机制，避免随机生成不可达路线。
    Level result = tutorial();
    const wchar_t* names[]{L"小试牛刀", L"断桥危机", L"敌人来袭", L"小心激光", L"终章：BOSS战"};
    result.theme = std::clamp(index, 0, 4);
    result.name = names[result.theme];
    // 第二步：断桥关收窄桥面并加入踩踏后坍塌的平台与往返锯轮。
    if (index == 1) {
        for (std::size_t i = 1; i < result.platforms.size(); ++i) result.platforms[i].w -= 12;
        result.fragile = {{1}, {4}, {6}, {8}};
        result.saws = {{{985, 340, 24, 24}, 985, 55, 1.7f}, {{1880, 330, 24, 24}, 1880, 48, 1.4f}};
    }
    // 第三步：敌人关布置巡逻者，玩家可从上方踩击或使用 J 发射子弹。
    if (index == 2) {
        result.enemies = {{{395, 462, 26, 28}, 365, 460}, {{790, 337, 26, 28}, 755, 860},
            {{1430, 387, 26, 28}, 1410, 1510}, {{2095, 332, 26, 28}, 2075, 2160}};
    }
    // 第四步：激光关使用错峰开启的光柱；虚线是冷却，亮线为危险状态。
    if (index == 3) {
        result.lasers = {{{430, 335, 12, 155}, 3.4f, 1.2f, 0}, {{1035, 270, 12, 155}, 3.8f, 1.3f, 1.8f},
            {{1460, 255, 12, 160}, 3.6f, 1.2f, 0.7f}, {{1930, 265, 12, 155}, 3.8f, 1.1f, 2.0f}};
    }
    // 第五步：最后一关保留前半段挑战，在终点平台设置需要击败的 BOSS。
    if (index == 4) {
        result.boss = true;
        result.enemies = {{{790, 337, 26, 28}, 755, 860}};
        result.lasers = {{{1450, 260, 12, 155}, 4.0f, 1.0f, 2.0f}};
        result.saws = {{{980, 335, 24, 24}, 980, 40, 1.3f}};
    }
    // 高路提供星羽探索奖励，各关使用不同移动方向、定时平台或预警落刺。
    buildAdventureRoute(result);
    return result;
}
void World::load(int index, bool endlessMode) {
    // 加载模式时清除上一局状态；继续游戏不调用这里，因此不会丢失进度。
    chapter = std::clamp(index, 0, 4);
    endless = endlessMode;
    level = Level::campaign(chapter);
    if (endless) {
        // 无尽模式从安全起点开始，前方按固定规则生成，永远没有终点判定。
        level = Level{};
        level.name = L"无尽模式";
        level.theme = 5;
        level.platforms = {{0, 550, 275, 90}};
        level.platformScores = {0};
        level.spawnX = 60; level.spawnY = 522;
        level.checkpoint = {-1000, 0, 1, 1}; level.goal = {-1000, 0, 1, 1};
        level.width = 275;
    }
    checkpointActive = false; won = false; elapsed = 0; deaths = 0;
    scoring.reset();
    spikeActivity = SpikeActivity::tutorial(level.spikes);
    respawn();
    if (endless) extendEndless();
}
void World::extendEndless() {
    // 保持角色前方至少一屏半有地形；旧平台不改编号，积分可稳定防止重复领取。
    if (!endless || level.width >= player.body.x + 1440) return;
    const auto oldGroups = spikeActivity.traps;
    while (level.width < player.body.x + 1440) {
        const std::size_t index = level.platforms.size();
        const float heights[]{500, 440, 400, 460, 520};
        const float x = level.platforms.back().right() + 60;
        const float y = heights[(index - 1) % 5];
        level.platforms.push_back({x, y, 180, 24});
        level.platformScores.push_back(100);
        // 每三段设置一组伸缩尖刺，平台落脚区留出引诱机关的安全位置。
        if (index % 3 == 0) {
            level.spikes.push_back({x + 90, y - 24, 24, 24});
            level.spikes.push_back({x + 114, y - 24, 24, 24});
            level.spikeScores.push_back(100); level.spikeScores.push_back(100);
        }
        level.width = x + 180;
    }
    // 新区域分组后恢复旧组计时，防止生成地形时已经触发的机关重新启动。
    spikeActivity = SpikeActivity::tutorial(level.spikes);
    for (auto& group : spikeActivity.traps)
        for (const auto& old : oldGroups) if (group.indices == old.indices) { group = old; break; }
}
bool World::platformSolid(std::size_t index) const {
    // 坍塌平台保留原编号，只跳过碰撞和绘制，不挤动其余计分索引。
    for (const auto& fragile : level.fragile) if (fragile.index == index && fragile.collapsed) return false;
    return true;
}
}
