// 模块说明：关卡设计层。十五关共用一套操作，差异全部由本文件的三张表 + 两个函数决定：
//   Layouts[15] —— 扩展段地形（间距/宽度/六段高度，相邻高差 ≤80 保证二段跳可达）
//   addCampaignHazards() —— 每关独有的机关组合（按 case 0..14）
//   敌人投放 + 旗帜 + BOSS 站位 —— campaign() 尾部按编号顺序执行
// campaign() 的生成顺序很重要：先铺平台，再摆旗帜，最后放地面机关，这样旗帜不会被摆在危险物里。
// 修改某关地形或机关时，先找 Layouts 行，再找 addCampaignHazards 的对应 case。
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
// 通关后段的扩展段地形模板：三组间距、三组宽度和六段高度循环组合，
// 让十五关不仅在机关上不同，平台的高低起伏与落脚宽窄也各有节奏。
// heights 使用绝对世界坐标（Y 向下），相邻两段高差不超过 80 像素，
// 保证单跳或二段跳都能上去，不会生成跳不过去的路线。
struct StageLayout {
    float gaps[3];     // 平台之间的距离循环（像素）
    float widths[3];   // 平台宽度循环（像素）
    int heights[6];    // 平台顶面的高度循环（世界坐标 Y）
};

// 十五张地形卡：整体从平缓起伏逐渐过渡到长桥、深谷、连续爬升与起伏台阶。
const StageLayout Layouts[15] = {
    {{70, 60, 70}, {190, 175, 205}, {360, 420, 460, 420, 360, 320}},   // 1 小试牛刀：缓波
    {{85, 70, 85}, {150, 165, 140}, {380, 340, 300, 340, 380, 420}},   // 2 断桥危机：拱桥
    {{60, 80, 60}, {200, 180, 220}, {360, 420, 460, 500, 460, 420}},   // 3 敌人来袭：深谷
    {{75, 65, 75}, {170, 195, 160}, {400, 360, 320, 360, 400, 440}},   // 4 小心激光：错峰
    {{70, 85, 70}, {190, 210, 180}, {380, 340, 380, 420, 460, 420}},   // 5 终章：竞技场前
    {{80, 80, 80}, {230, 210, 250}, {360, 400, 440, 400, 360, 320}},   // 6 水平弹幕：长桥
    {{70, 90, 70}, {180, 160, 200}, {380, 300, 340, 380, 420, 460}},   // 7 隐藏拳头：拳台
    {{65, 75, 65}, {150, 130, 170}, {400, 440, 400, 360, 320, 360}},   // 8 伏击林径：树冠
    {{80, 70, 80}, {140, 160, 140}, {340, 380, 340, 300, 340, 380}},   // 9 霓虹回廊：灯廊
    {{75, 75, 90}, {200, 180, 220}, {440, 400, 360, 320, 360, 400}},   // 10 熔心终局：下沉爬升
    {{60, 70, 60}, {160, 180, 160}, {420, 380, 340, 300, 360, 420}},   // 11 苍崖逐风：连续登高
    {{90, 75, 90}, {150, 140, 170}, {420, 380, 340, 320, 300, 340}},   // 12 残桥悬梯：阶梯下坠
    {{70, 80, 70}, {210, 190, 220}, {380, 420, 460, 420, 380, 340}},   // 13 暗林猎场：缓坡
    {{65, 70, 65}, {140, 150, 130}, {360, 400, 440, 480, 440, 400}},   // 14 光栅深井：井底
    {{75, 65, 75}, {200, 220, 190}, {400, 360, 320, 360, 400, 440}},   // 15 终焉：王座台阶
};

// 开局前段的起伏表：第 2~9 块基础平台在原高度上做像素级抬升（负数为抬高），
// 让十五关的起步区各有节奏——有的先爬升，有的先下坡，有的是台阶或深谷；
// 相邻平台的高度差控制在 50 以内，单跳或二段跳都能上去。
const int EarlyLift[15][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},                    // 1 平缓入门
    {-30, -50, -20, 10, 30, 10, -10, 20},        // 2 拱桥前的爬升
    {0, -40, -60, -20, 10, -20, -50, -20},       // 3 深谷里的高台
    {20, 0, -30, -50, -20, 0, 20, 40},           // 4 缓坡下沉
    {-20, -40, -20, 10, 30, 50, 30, 10},         // 5 竞技场前的抬高
    {0, 20, 0, 20, 0, 20, 0, 20},                // 6 长桥台阶
    {-40, -60, -30, 0, 30, 50, 20, 0},           // 7 拳台高台
    {30, 10, -20, -40, -20, 0, 20, 40},          // 8 树冠起伏
    {-10, -30, -50, -30, -10, 10, 30, 50},       // 9 连续登高
    {40, 20, 0, -30, -50, -30, -10, 10},         // 10 井口下沉再爬升
    {0, -40, -60, -20, 10, 40, 50, 30},          // 11 崖壁爬升
    {50, 30, 10, -10, -30, -50, -30, -10},       // 12 残梯下坠
    {-30, 0, 30, 0, -30, 0, 30, 0},              // 13 林间台阶
    {20, -10, -40, -60, -30, 0, 20, 50},         // 14 深井起伏
    {0, -20, -40, -60, -40, -20, 0, 20},         // 15 王座前的爬升
};

// 某块平台是不是断桥（踩上去会坍塌）。断桥上不允许摆放任何东西。
bool isFragileIndex(const Level& level, std::size_t index) {
    for (const auto& piece : level.fragile)
        if (piece.index == index) return true;
    return false;
}

// 按水平位置找到机关脚下的平台；开局地形升降后用它把机关重新贴回平台顶面。
const Rect* platformUnderX(const Level& level, float x) {
    for (const auto& platform : level.platforms)
        if (x >= platform.x && x < platform.right()) return &platform;
    return nullptr;
}

// 开局地形调整之后，把原本按旧高度硬编码的尖刺、锯轮、巡逻者和光柱重新吸附到平台顶面，
// 避免出现悬空机关或半截埋进桥面的情况。
void snapBaseProps(Level& level) {
    // 地面尖刺本身就是矩形，直接按中心点找脚下的平台。
    for (auto& spike : level.spikes)
        if (const Rect* platform = platformUnderX(level, spike.x + spike.w * 0.5f))
            spike.y = platform->y - spike.h;
    for (auto& saw : level.saws)
        if (const Rect* platform = platformUnderX(level, saw.body.x + saw.body.w * 0.5f))
            saw.body.y = platform->y - 90.0f;
    for (auto& enemy : level.enemies)
        if (const Rect* platform = platformUnderX(level, enemy.body.x + enemy.body.w * 0.5f))
            enemy.body.y = platform->y - enemy.body.h;
    for (auto& laser : level.lasers)
        if (const Rect* platform = platformUnderX(level, laser.body.x))
            laser.body.y = platform->y - laser.body.h;
}

// 应用开局起伏：只改平台高度，X 坐标保持不变，因此巡逻边界与金币位置都不受影响。
void applyEarlyLayout(Level& level, int stage) {
    for (int i = 0; i < 8; ++i) {
        const std::size_t index = static_cast<std::size_t>(i + 2);
        if (index >= level.platforms.size()) break;
        const float y = level.platforms[index].y + static_cast<float>(EarlyLift[stage][i]);
        level.platforms[index].y = std::clamp(y, 260.0f, 520.0f);
    }
    snapBaseProps(level);
}

// 判断某个矩形是否压在断桥上：水平范围相交，且垂直方向处在桥面或桥面上方的机关高度内。
bool standsOnFragile(const Level& level, const Rect& body) {
    for (const auto& piece : level.fragile) {
        if (piece.index >= level.platforms.size()) continue;
        const Rect& bridge = level.platforms[piece.index];
        const bool overBridge = body.right() > bridge.x && body.x < bridge.right();
        if (overBridge && body.y < bridge.y + 4.0f && body.bottom() > bridge.y - 300.0f) return true;
    }
    return false;
}

// 断桥清理：门、旗帜、怪物、各类机关与金币都不能落在会坍塌的平台上；
// 最后一个平台承载终点门，必须保持实体，因此也从断桥名单里剔除。
void sanitizeFragile(Level& level) {
    level.fragile.erase(std::remove_if(level.fragile.begin(), level.fragile.end(),
        [&](const FragilePlatform& piece) { return piece.index + 1 >= level.platforms.size(); }),
        level.fragile.end());
    auto purge = [&](auto& props) {
        props.erase(std::remove_if(props.begin(), props.end(),
            [&](const auto& prop) { return standsOnFragile(level, prop.body); }), props.end());
    };
    purge(level.enemies);
    purge(level.lasers);
    purge(level.saws);
    purge(level.cannons);
    purge(level.fists);
    purge(level.fallingSpikes);
    purge(level.overheadTraps);
    purge(level.coins);
    // 旗帜已经在挑选阶段跳过断桥，这里只需处理意外落到桥面上的矩形。
    level.checkpoints.erase(std::remove_if(level.checkpoints.begin(), level.checkpoints.end(),
        [&](const Rect& flag) { return standsOnFragile(level, flag); }), level.checkpoints.end());
}

// 扩展段只负责铺路：先只生成平台，机关稍后再按关卡单独摆放。
// 这样存档旗帜可以先挑选落脚平台，地面类机关再避开这些平台，重生点不会落在危险物上。
void buildCampaignExpansion(Level& level, int stage) {
    const StageLayout& layout = Layouts[stage];
    // 扩展段长度随章节增加：前三档 6 段、中三档 8 段、最后三档 10 段；
    // 金币仍由 campaign() 末尾统一按平台补齐，避免这里重复发放。
    const int count = 6 + stage / 5 * 2;
    for (int i = 0; i < count; ++i) {
        const std::size_t slot = level.platforms.size();
        const float spacing = layout.gaps[slot % 3];
        const float x = level.platforms.back().right() + spacing;
        // 地形从基础路线的最后一块平台（第 10 块）开始循环六段高度。
        const float y = static_cast<float>(layout.heights[(slot - 10) % 6]);
        const float width = layout.widths[slot % 3];
        level.platforms.push_back({x, y, width, 24});
        level.platformScores.push_back(250);
        // 断桥危机与残桥悬梯两关会掺入踩踏后坍塌的平台；它们在挑选旗帜之前加入，
        // 让第二面旗帜自动避开这些会消失的落脚点。最后一块平台承载终点门，永远保持实体。
        if (((stage == 1 && i % 3 == 2) || (stage == 11 && i % 3 == 0)) && i + 1 < count)
            level.fragile.push_back({level.platforms.size() - 1});
    }
}

// 判断扩展段某块平台是否被存档旗帜占用：有旗帜的平台不摆贴地机关，
// 否则玩家重生时可能直接站在激光或尖刺里。倒挂尖刺与下落尖刺仍在安全高度，不受限制。
bool holdsCampaignFlag(const Level& level, const Rect& platform) {
    for (const auto& flag : level.checkpoints)
        if (flag.right() > platform.x && flag.x < platform.right()) return true;
    return false;
}

// 按关卡给扩展段摆放机关：每关挑选不同的机关组合与出现节奏，
// 既保留“每关都有自己的机关语言”，又让相邻关卡的通过方式明显不同。
void addCampaignHazards(Level& level, int stage) {
    // 扩展段从第 10 块平台开始；i 用于让机关按“段序”错开摆放，不依赖绝对下标。
    const std::size_t first = 10;
    for (std::size_t index = first; index < level.platforms.size(); ++index) {
        // 断桥上什么都不能有：整段跳过，连怪物密度也不往这里补。
        if (isFragileIndex(level, index)) continue;
        const Rect platform = level.platforms[index];
        // 终点平台以及终点前最后一小段保持干净：不摆贴地机关与远程机关。
        // 这样结算区有安全落脚点，终焉关的 BOSS 与守门 Claude 也不会和残留炮台互相干扰。
        if (index + 1 >= level.platforms.size() || platform.right() > level.goal.x - 420.0f) continue;
        const std::size_t i = index - first;
        const float x = platform.x, y = platform.y, w = platform.w;
        const bool reserved = holdsCampaignFlag(level, platform);
        switch (stage) {
        case 0:
            // 小试牛刀：只在部分路段立一排倒挂尖刺，留出完整地面通过。
            if (i % 3 == 2) level.overheadTraps.push_back({{x + w * 0.5f - 12, y - 170, 24, 42}, TrapStyle::Ceiling});
            // 后半程放两只近战 Grok，避免整关只有机关、没有可踩可射的靶子。
            if (!reserved && i % 3 == 1) level.enemies.push_back({{x + w * 0.5f, y - 28, 26, 28}, x + 20, x + w - 40});
            break;
        case 1:
            // 断桥危机：悬空往返锯轮配合已经加入的坍塌桥面，逼迫玩家跳得干脆。
            if (!reserved && i % 3 == 1) level.saws.push_back({{x + w * 0.5f, y - 80, 22, 22}, x + w * 0.5f, 26, 1.5f});
            // 桥面上夹两名巡逻者，断桥与锯轮之间仍需清理敌人。
            if (!reserved && i % 3 == 1) level.enemies.push_back({{x + w - 60, y - 28, 26, 28}, x + 20, x + w - 40});
            break;
        case 2:
            // 敌人来袭：巡逻者与激光交替出现，考验“先清场再跳”的节奏。
            if (i % 2 == 1) level.enemies.push_back({{x + w * 0.5f, y - 28, 26, 28}, x + 20, x + w - 40});
            if (i % 3 == 1) level.lasers.push_back({{x + w - 40, y - 115, 10, 115}, 3.8f, 1.1f, 1.2f});
            break;
        case 3:
            // 小心激光：每两段一道光柱，熄灭窗口更短，落脚点更窄。
            if (i % 2 == 0) level.lasers.push_back({{x + w * 0.5f, y - 120, 10, 120}, 3.4f, 1.0f, 0.6f});
            if (i % 3 == 2) level.enemies.push_back({{x + w - 60, y - 28, 26, 28}, x + 30, x + w - 40});
            break;
        case 4:
            // 终章（第 5 关）：竞技场之前摆一组激光、锯轮和巡逻者，当作 BOSS 前的热身。
            if (i % 3 == 0) level.lasers.push_back({{x + 30, y - 120, 10, 120}, 4.0f, 1.0f, 1.8f});
            if (i % 3 == 1) level.saws.push_back({{x + w - 50, y - 90, 22, 22}, x + w - 50, 28, 1.3f});
            if (i % 3 == 2) level.enemies.push_back({{x + w * 0.5f, y - 28, 26, 28}, x + 20, x + w - 40});
            break;
        case 5:
            // 水平弹幕：长桥上每隔一段架一门水平炮台，需要卡着炮击间隔冲刺。
            if (i % 2 == 0) level.cannons.push_back({{x + w - 40, y - 24, 24, 24}, -1, 2.6f});
            if (i % 3 == 1) level.enemies.push_back({{x + 40, y - 28, 26, 28}, x + 20, x + w - 40});
            if (i % 3 == 2) level.lasers.push_back({{x + w - 70, y - 110, 10, 110}, 4.2f, 0.9f, 2.2f});
            break;
        case 6:
            // 隐藏拳头：每段平台的入口埋着弹簧拳，踩中会被弹到高处，节奏由“踩”变成“飞”。
            if (i % 2 == 0) level.fists.push_back({{x + 60, y - 4, 32, 4}});
            if (i % 2 == 1) level.enemies.push_back({{x + w * 0.5f, y - 28, 26, 28}, x + 30, x + w - 40});
            break;
        case 7:
            // 伏击林径：树冠上的倒挂尖刺搭配定时落刺，需要观察预警再冲过去。
            if (i % 2 == 0) level.overheadTraps.push_back({{x + w * 0.5f - 12, y - 175, 24, 44}, TrapStyle::Ceiling});
            if (i % 3 == 1) level.fallingSpikes.push_back({{x + 60, y - 180, 22, 28}, y - 180});
            if (i % 3 == 2) level.enemies.push_back({{x + w - 70, y - 28, 26, 28}, x + 40, x + w - 40});
            break;
        case 8:
            // 霓虹回廊：又窄又密的灯廊，光柱与锯轮交替，站位必须精确。
            if (i % 2 == 0) level.lasers.push_back({{x + w * 0.5f, y - 105, 10, 105}, 3.6f, 0.8f, 1.4f});
            if (i % 2 == 1) level.saws.push_back({{x + w * 0.5f, y - 70, 20, 20}, x + w * 0.5f, 24, 1.7f});
            // 灯廊里也安排巡逻者，被光柱逼停时正好要面对它。
            if (i % 3 == 2) level.enemies.push_back({{x + w - 60, y - 28, 26, 28}, x + 30, x + w - 40});
            break;
        case 9:
            // 熔心终局：下沉平台配上锯轮与炮台，需要边后退边等机关冷却。
            if (i % 2 == 0) level.saws.push_back({{x + w - 60, y - 85, 22, 22}, x + w - 60, 30, 1.4f});
            if (i % 3 == 1) level.cannons.push_back({{x + w - 40, y - 24, 24, 24}, -1, 3.0f});
            if (i % 3 == 2) level.enemies.push_back({{x + w * 0.5f, y - 28, 26, 28}, x + 20, x + w - 40});
            break;
        case 10:
            // 苍崖逐风：连续登高的崖壁上落刺不断，考验向上跳的时机。
            if (i % 3 == 1) level.fallingSpikes.push_back({{x + 50, y - 185, 22, 28}, y - 185});
            if (i % 3 == 2) level.overheadTraps.push_back({{x + w - 60, y - 175, 24, 44}, TrapStyle::Ceiling});
            // 崖壁上的巡逻者负责挡住上升节奏，踩头或射击都可以清掉。
            if (!reserved && i % 3 == 0) level.enemies.push_back({{x + w * 0.5f, y - 28, 26, 28}, x + 20, x + w - 40});
            break;
        case 11:
            // 残桥悬梯：一路下坠的残梯中夹着落刺与巡逻者，踩上脆板要立刻起跳。
            if (i % 3 == 1) level.fallingSpikes.push_back({{x + 60, y - 175, 22, 28}, y - 175});
            if (i % 3 == 2) level.enemies.push_back({{x + w * 0.5f, y - 28, 26, 28}, x + 30, x + w - 40});
            break;
        case 12:
            // 暗林猎场：巡逻者、树冠倒刺与落刺混合的狩猎场。
            if (i % 2 == 1) level.enemies.push_back({{x + w * 0.5f, y - 28, 26, 28}, x + 20, x + w - 40});
            if (i % 3 == 0) level.overheadTraps.push_back({{x + w - 70, y - 170, 24, 42}, TrapStyle::Ceiling});
            if (i % 3 == 2) level.fallingSpikes.push_back({{x + 40, y - 180, 22, 28}, y - 180});
            break;
        case 13:
            // 光栅深井：井底的密集光栅，只有在熄灭的瞬间才能穿过。
            if (i % 2 == 0) level.lasers.push_back({{x + w * 0.5f - 20, y - 125, 10, 125}, 3.0f, 0.9f, 0.5f});
            if (i % 3 == 1) level.lasers.push_back({{x + w - 50, y - 125, 10, 125}, 3.6f, 0.8f, 1.8f});
            // 井底尽头安排巡逻者，躲完光栅还要处理一个正面敌人。
            if (!reserved && i % 3 == 2) level.enemies.push_back({{x + w - 60, y - 28, 26, 28}, x + 30, x + w - 40});
            break;
        default:
            // 终焉：王座台阶把前面用过的机关各取一点，作为最后的综合考验。
            if (i % 3 == 0) level.cannons.push_back({{x + w - 40, y - 24, 24, 24}, -1, 2.8f});
            if (i % 3 == 1) level.lasers.push_back({{x + 40, y - 120, 10, 120}, 3.8f, 1.0f, 1.0f});
            if (i % 3 == 2) level.fallingSpikes.push_back({{x + w - 60, y - 180, 22, 28}, y - 180});
            if (i % 2 == 1) level.enemies.push_back({{x + w * 0.5f, y - 28, 26, 28}, x + 30, x + w - 40});
            break;
        }
        // 统一增加怪物密度：机关配置只决定“这一关的机关语言”，怪物数量单独加一层，
        // 让十五关都能稳定出现 5 只以上的巡逻者。每两段补一名，且避开已有怪物的平台，
        // 同时保留上限，避免终焉关与 BOSS 战被怪堆淹没。
        const bool dense = level.enemies.size() < 12 && i % 2 == 0 && !reserved;
        if (dense) {
            const bool occupied = std::any_of(level.enemies.begin(), level.enemies.end(), [&](const Enemy& enemy) {
                return enemy.body.x < x + w && enemy.body.x + enemy.body.w > x;
            });
            if (!occupied) level.enemies.push_back({{x + w * 0.65f, y - 28, 26, 28}, x + 20, x + w - 40});
        }
    }
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
    // 十五关的背景主题各有安排：前五关固定使用 0–4（起始高路的机关按主题配置），
    // 后十关把五种主题重新打乱，避免同一片天色在相邻关卡里连续出现。
    const int themes[15]{0, 1, 2, 3, 4, 3, 0, 4, 1, 2, 2, 4, 0, 3, 1};
    result.theme = themes[stage];
    result.name = L"第" + std::to_wstring(stage + 1) + L"关 · " + names[stage];
    // 第二步：断桥关收窄桥面并加入踩踏后坍塌的平台与往返锯轮。
    if (index == 1) {
        for (std::size_t i = 1; i < result.platforms.size(); ++i) result.platforms[i].w -= 12;
        result.fragile = {{1}, {4}, {6}, {8}};
        // 锯轮悬在会坍塌的桥面上就是“断桥上摆了东西”，改挂到第 5、7 块实体平台的斜上方；
        // 这两个位置也不挡出生点高路（星羽支线）的落回路线。
        result.saws = {{{1220, 330, 24, 24}, 1220, 55, 1.7f}, {{1700, 320, 24, 24}, 1700, 48, 1.4f}};
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
    // 开局前段按关卡做起伏：十五关的起步区不再共用同一条水平走廊，
    // 有的直接向上爬、有的先下坡再爬，玩家也可以顺着抬高的平台往高处走。
    applyEarlyLayout(result, stage);
    // 高路提供星羽探索奖励，各关使用不同移动方向、定时平台或预警落刺。
    buildAdventureRoute(result);
    // 扩展段按本关地形卡铺路：每关的高度起伏、平台宽窄与间距都不一样，主路线不再是同一条走廊。
    buildCampaignExpansion(result, stage);
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
    // 存档点确定后再摆放扩展段机关：机关会跳过有旗帜的平台，玩家重生时不会直接落在危险物里。
    addCampaignHazards(result, stage);
    // 数量兜底：机关配置与旗帜占位都可能让某关怪物偏少，这里从主路中段一路补到扩展段，
    // 优先补至每关九只巡逻者，保证十五关的怪物密度大致一致；出生点附近留出安全区。
    for (std::size_t i = 3; i + 1 < result.platforms.size() && result.enemies.size() < 9; ++i) {
        const Rect& platform = result.platforms[i];
        if (isFragileIndex(result, i)) continue;
        if (platform.x < result.spawnX + 420.0f) continue;
        if (holdsCampaignFlag(result, platform)) continue;
        if (platform.right() > result.goal.x - 420.0f) continue;
        const bool occupied = std::any_of(result.enemies.begin(), result.enemies.end(), [&](const Enemy& enemy) {
            return enemy.body.x < platform.right() && enemy.body.x + enemy.body.w > platform.x;
        });
        if (occupied) continue;
        result.enemies.push_back({{platform.x + platform.w * 0.5f, platform.y - 28.0f, 26, 28},
            platform.x + 20.0f, platform.right() - 40.0f});
    }
    bool finalGptAssigned = false;
    // 敌人形象按关卡投放：第一、二关只有近战的 Grok，第三关起 GPT 登场，第七关起 Claude 也可能出现。
    for (auto& enemy : result.enemies) {
        if (stage < 2) enemy.avatar = 2;
        else if (stage < 6) enemy.avatar = 1 + avatar() % 2;
        else {
            // 第七关起逐步提高 Claude 的出现概率，最后一关只保留 Claude 与 GPT。
            static std::mt19937 typeRng(std::random_device{}());
            const int chance = 40 + (stage - 6) * 5;
            const int roll = std::uniform_int_distribution<int>(0, 99)(typeRng);
            enemy.avatar = roll < chance ? 0 : (stage == 14 ? 1 : 1 + avatar() % 2);
            if (stage == 14 && !finalGptAssigned) { enemy.avatar = 1; finalGptAssigned = true; }
        }
    }
    // 断桥终检：门、旗帜、怪物与机关都不能落在会坍塌的平台上；
    // 这一步也保证最后一个平台（承载终点门）永远保持实体。
    sanitizeFragile(result);
    // 终焉关：Claude 站在终点门右侧、与 BOSS 一起守关，站着不动但持续甩出封禁图片。
    if (stage == 14) {
        result.banStorm = true;
        result.enemies.push_back({{result.goal.x + 100.0f, result.goal.y + result.goal.h - 28.0f, 26, 28},
            result.goal.x + 100.0f, result.goal.x + 100.0f, 0.0f, 1, true, 0});
    }
    // 金币最后统一补齐；断桥上的金币会随桥面消失，所以直接跳过。
    for (std::size_t i = 0; i < result.platforms.size(); ++i)
        if (!isFragileIndex(result, i)) addRoadCoins(result, i);
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

