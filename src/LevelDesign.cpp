// 五关配置与无尽地图生成：关卡共享基础操作，主题、机关和通关目标各不相同。
#include "World.h"
#include "AdventureMechanics.h"
#include <algorithm>

namespace game {
namespace {
void addRoadCoins(Level& level, std::size_t index) {
    // 在平台安全入口和跳跃弧线上放金币，追加顺序作为稳定编号供无尽模式使用。
    const auto& p = level.platforms[index];
    for (float offset : {32.0f, 62.0f}) level.coins.push_back({{p.x + offset + (index == 0 ? 68 : 0), p.y - 26, 16, 16}});
    if (index > 0) {
        const auto& previous = level.platforms[index - 1];
        level.coins.push_back({{(previous.right() + p.x) * 0.5f - 8, std::min(previous.y, p.y) - 70, 16, 16}});
    }
}
}
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
        // 激光关新增一名可踩踏或射击的巡逻者，供战斗星目标使用。
        result.enemies = {{{790, 337, 26, 28}, 755, 860}};
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
    for (std::size_t i = 0; i < result.platforms.size(); ++i) addRoadCoins(result, i);
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
        addRoadCoins(level, 0);
    }
    checkpointActive = false; won = false; elapsed = 0; deaths = 0;
    stamina = InitialStamina;
    scoring.reset();
    spikeActivity = SpikeActivity::tutorial(level.spikes);
    resetPlayer();
    if (endless) extendEndless();
}
void World::extendEndless() {
    // 保持角色前方至少一屏半有地形；旧平台不改编号，积分可稳定防止重复领取。
    if (!endless || level.width >= player.body.x + 1440) return;
    while (level.width < player.body.x + 1440) {
        const std::size_t index = level.platforms.size();
        const float heights[]{500, 440, 400, 460, 520};
        const float x = level.platforms.back().right() + (index % 4 == 0 ? 80 : 60);
        const float y = heights[(index - 1) % 5];
        const float width = index % 4 == 1 ? 210.0f : 180.0f;
        level.platforms.push_back({x, y, width, 24});
        level.platformScores.push_back(100);
        addRoadCoins(level, index);
        // 每三段设置一组伸缩尖刺，平台落脚区留出引诱机关的安全位置。
        if (index % 3 == 0) {
            level.spikes.push_back({x + 90, y - 24, 24, 24});
            level.spikes.push_back({x + 114, y - 24, 24, 24});
            level.spikeScores.push_back(100); level.spikeScores.push_back(100);
        }
                // 分段组合避免每个平台同时堆叠机关；首段留作熟悉操作。
        if (index > 3 && index % 3 != 0) {
            switch (index % 8) {
            case 0: level.fragile.push_back({index}); break;
            case 1: level.enemies.push_back({{x + 110, y - 28, 26, 28}, x + 90, x + width - 30}); break;
            case 2: level.lasers.push_back({{x + 135, y - 120, 10, 120}, 4.4f, 0.9f, 1.5f}); break;
            case 3: level.saws.push_back({{x + 140, y - 100, 20, 20}, x + 140, 22, 1.4f}); break;
            case 4: level.overheadTraps.push_back({{x + 115, y - 190, 24, 40}, TrapStyle::Ceiling}); break;
            case 5: level.fallingSpikes.push_back({{x + 140, y - 200, 22, 28}, y - 200}); break;
            default: break;
            }
        }
        level.width = x + width;
    }
    // 新区域分组后恢复旧组计时，防止生成地形时已经触发的机关重新启动。
    spikeActivity.rebuild(level.spikes, spikeRandom);
}
bool World::platformSolid(std::size_t index) const {
    // 坍塌平台保留原编号，只跳过碰撞和绘制，不挤动其余计分索引。
    for (const auto& fragile : level.fragile) if (fragile.index == index && fragile.collapsed) return false;
    return true;
}
}
