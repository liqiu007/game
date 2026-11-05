// 十五关配置与无尽地图生成：关卡共享基础操作，主题、机关和通关目标各不相同。
#include "World.h"
#include "AdventureMechanics.h"
#include <algorithm>
#include <random>
// 每次创建怪物独立抽签，重生保留形象。
namespace { int avatar() { static std::mt19937 rng(std::random_device{}()); return std::uniform_int_distribution<int>(0,2)(rng); } }

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
// 无尽模式的怪物形象随深度放开：开头只有近战 Grok，中段加入 GPT，很远之后 Claude 也可能出现。
int endlessAvatar(std::size_t index) {
    if (index < 6) return 2;
    if (index < 30) return 1 + avatar() % 2;
    return avatar();
}
}
Level Level::campaign(int index) {
    // 第一步：复用基础跳跃间距，按关卡加入不同机制，避免随机生成不可达路线。
    const int stage = std::clamp(index, 0, 14);
    index = stage % 5;
    Level result = tutorial();
    // 十五关各有独立关名：前五关沿用入门命名，第六关起按机关与场景递进重新取名。
    const wchar_t* names[]{L"小试牛刀", L"断桥危机", L"敌人来袭", L"小心激光", L"终章：BOSS战",
        L"水平弹幕", L"隐藏拳头", L"伏击林径", L"霓虹回廊", L"熔心终局",
        L"苍崖逐风", L"残桥悬梯", L"暗林猎场", L"光栅深井", L"终焉：封禁王座"};
    result.theme = std::clamp(stage % 5, 0, 4);
    result.name = L"第" + std::to_wstring(stage + 1) + L"关 · " + names[stage];
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
    // 第五步：BOSS 主题关保留前半段挑战；BOSS 本身在计算完终点后放到关卡最后。
    if (index == 4) {
        result.enemies = {{{790, 337, 26, 28}, 755, 860}};
        result.lasers = {{{1450, 260, 12, 155}, 4.0f, 1.0f, 2.0f}};
        result.saws = {{{980, 335, 24, 24}, 980, 40, 1.3f}};
    }
    // 高路提供星羽探索奖励，各关使用不同移动方向、定时平台或预警落刺。
    buildAdventureRoute(result);
    // 扩大地图，保持安全跳跃间距，后续章节增加路线长度。
    for (int i = 0; i < 6 + stage / 5 * 3; ++i) {
        float x = result.platforms.back().right() + 60, y = 360 + (i % 3) * 40.0f;
        result.platforms.push_back({x, y, 190, 24}); result.platformScores.push_back(250);
        // 扩展路段交替配置机关，入口保留落脚空间，出口平台留给结算。
        if (i % 3 == 0) result.fragile.push_back({result.platforms.size() - 1});
        else if (i % 3 == 1) result.lasers.push_back({{x + 130, y - 105, 10, 105}, 4.2f, 0.7f, 1.5f});
        else result.overheadTraps.push_back({{x + 100, y - 190, 24, 42}, TrapStyle::Ceiling});
        if (stage == 5 && i % 3 == 1) result.cannons.push_back({{x + 160, y - 24, 24, 24}, -1, 2.6f});
        if (stage == 6 && i % 3 == 1) result.fists.push_back({{x + 80, y - 4, 32, 4}});
        if (i % 2 == 0) result.enemies.push_back({{x + 80, y - 28, 26, 28}, x + 30, x + 155});
    }
    const auto end = result.platforms.back();
    result.goal = {end.x + 130, end.y - 52, 32, 52}; result.width = end.right();
    // BOSS 与终点同在最后一关平台上，站在终点门右侧：第 5、10 关可以从容绕过，第 15 关必须击败它才开门。
    if (index == 4) {
        result.boss = true;
        // 终点平台向右加长，给 BOSS 与终焉关的 Claude 留出站位。
        result.platforms.back().w += 80.0f;
        result.width = result.platforms.back().right();
        result.bossBody = {result.goal.x + 40.0f, result.goal.y + result.goal.h - 72.0f, 56, 72};
        result.bossGate = stage == 14;
    }
    // 每关最多两面旗帜：第一面在出生区，第二面接近终点前；BOSS 关第二面提前收在竞技场之前。
    result.checkpoints.clear();
    const float flagStart = 555.0f;
    const float flagEnd = result.goal.x - (result.boss ? 900.0f : 600.0f);
    const float flagSpan = std::max(0.0f, flagEnd - flagStart);
    for (int slot = 0; slot < 2; ++slot) {
        // 两面旗帜把可用区间二等分：0 号在起点，1 号在终点前。
        const float wanted = flagStart + flagSpan * slot;
        // 旗帜要立在实地平台上：跳过会坍塌的平台，找离目标最近的稳定平台顶面。
        const Rect* surface = nullptr;
        float bestDistance = 1.0e9f;
        for (std::size_t i = 0; i < result.platforms.size(); ++i) {
            bool fragile = false;
            for (const auto& piece : result.fragile) if (piece.index == i) fragile = true;
            if (fragile) continue;
            const Rect& platform = result.platforms[i];
            const float distance = wanted < platform.x ? platform.x - wanted : (wanted > platform.right() ? wanted - platform.right() : 0.0f);
            if (distance < bestDistance) { bestDistance = distance; surface = &platform; }
        }
        if (!surface) continue;
        const float x = std::clamp(wanted, surface->x + 10.0f, surface->right() - 34.0f);
        result.checkpoints.push_back({x, surface->y - 44.0f, 24, 44});
    }
    // 敌人形象按关卡投放：第一、二关只有近战的 Grok，第三关起 GPT 登场，第七关起 Claude 也可能出现。
    for (auto& enemy : result.enemies) {
        if (stage < 2) enemy.avatar = 2;
        else if (stage < 6) enemy.avatar = 1 + avatar() % 2;
        else enemy.avatar = avatar();
    }
    // 终焉关：Claude 站在终点门右侧、与 BOSS 一起守关，站着不动但持续甩出封禁图片。
    if (stage == 14) {
        result.banStorm = true;
        result.enemies.push_back({{result.goal.x + 100.0f, result.goal.y + result.goal.h - 28.0f, 26, 28},
            result.goal.x + 100.0f, result.goal.x + 100.0f, 0.0f, 1, true, 0});
    }
    for (std::size_t i = 0; i < result.platforms.size(); ++i) addRoadCoins(result, i);
    return result;
}
void World::load(int index, bool endlessMode) {
    // 加载模式时清除上一局状态；继续游戏不调用这里，因此不会丢失进度。
    chapter = std::clamp(index, 0, 14);
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
        level.checkpoints.clear(); level.goal = {-1000, 0, 1, 1};
        level.width = 275;
        // 无尽模式没有高路平台，允许落刺在角色正上方直接进入预警。
        level.fallingSpikeGroundTrigger = true;
        addRoadCoins(level, 0);
    }
    checkpointActive = false; checkpointIndex = 0; won = false; elapsed = 0; deaths = 0;
    stamina = InitialStamina;
    scoring.reset();
    scoring.distanceOnly = endless;
    spikeActivity = SpikeActivity::tutorial(level.spikes);
    resetPlayer();
    if (endless) extendEndless();
}
void World::extendEndless() {
    // 保持角色前方至少一屏半有地形；旧平台不改编号，积分可稳定防止重复领取。
    if (!endless || level.width >= player.body.x + 1440) return;
    while (level.width < player.body.x + 1440) {
        const std::size_t index = level.platforms.size();
        // 第一步：平台高低起伏、宽窄交替，间距保持固定，保证每一段都还能跳过去。
        const float heights[]{500, 440, 400, 460, 520, 430, 480};
        const float widths[]{180, 210, 200, 170, 220};
        const float x = level.platforms.back().right() + (index % 4 == 0 ? 80 : 60);
        const float y = heights[(index - 1) % 7];
        const float width = widths[index % 5];
        level.platforms.push_back({x, y, width, 24});
        level.platformScores.push_back(100);
        addRoadCoins(level, index);
        // 第二步：每三段设置一组伸缩尖刺，平台落脚区留出引诱机关的安全位置。
        if (index % 3 == 0) {
            level.spikes.push_back({x + 90, y - 24, 24, 24});
            level.spikes.push_back({x + 114, y - 24, 24, 24});
            level.spikeScores.push_back(100); level.spikeScores.push_back(100);
        }
        // 第三步：部分平台在正上方布置预警落刺，跑过去要迅速，长时间停留会被砸中。
        if (index > 4 && index % 5 == 2) level.fallingSpikes.push_back({{x + 140, y - 200, 22, 28}, y - 200});
        // 第四步：分段组合避免每个平台同时堆叠机关；首段留作熟悉操作。
        if (index > 3 && index % 3 != 0) {
            // 用 index % 7 轮换机关类型：它与平台的“每三段一组”错开，保证七种都真的会出现。
            switch (index % 7) {
            case 0: level.fragile.push_back({index}); break;
            case 1: {
                // 巡逻者形象按深度投放；足够深之后才追加第二名，形成上下夹击。
                const float left = x + 30, right = x + width - 30;
                level.enemies.push_back({{x + 110, y - 28, 26, 28}, left, right, 65, 1, true, endlessAvatar(index)});
                if (index >= 30) level.enemies.push_back({{x + width - 70, y - 28, 26, 28}, left, right, 65, 1, true, endlessAvatar(index)});
                break;
            }
            case 2: level.lasers.push_back({{x + 135, y - 120, 10, 120}, 4.4f, 0.9f, 1.5f}); break;
            case 3: level.saws.push_back({{x + 140, y - 100, 20, 20}, x + 140, 22, 1.4f}); break;
            case 4: level.overheadTraps.push_back({{x + 115, y - 190, 24, 40}, TrapStyle::Ceiling}); break;
            case 5: level.cannons.push_back({{x + width - 40, y - 24, 24, 24}, -1, 2.6f}); break;
            default: level.fallingSpikes.push_back({{x + 70, y - 190, 22, 28}, y - 190}); break;
            }
        }
        level.width = x + width;
        // 第五步：每八段切换一次背景主题，让无尽长跑沿途风景不断变化。
        if (index >= 8) level.theme = static_cast<int>((index / 8) % 6);
    }
    for (auto& enemy : level.enemies) if (enemy.avatar < 0) enemy.avatar = 1 + avatar() % 2;
    // 新区域分组后恢复旧组计时，防止生成地形时已经触发的机关重新启动。
    spikeActivity.rebuild(level.spikes, spikeRandom);
}
bool World::platformSolid(std::size_t index) const {
    // 坍塌平台保留原编号，只跳过碰撞和绘制，不挤动其余计分索引。
    for (const auto& fragile : level.fragile) if (fragile.index == index && fragile.collapsed) return false;
    return true;
}
}
