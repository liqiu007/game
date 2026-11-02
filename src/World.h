// 游戏规则与关卡数据：坐标、角色物理、碰撞、积分、重生和镜头计算。
#pragma once
#include "system.h"
#include "SpikeActivity.h"
#include <string>
#include <vector>

namespace game {
// 固定窗口尺寸与模拟步长；地图宽度独立于窗口宽度。
inline constexpr int ScreenWidth = 960;
inline constexpr int ScreenHeight = 640;
inline constexpr float FixedStep = 1.0f / 120.0f;

// 世界矩形以左上角定位，右边缘和下边缘用于碰撞与距离计算。
struct Rect {
    float x, y, w, h;
    float right() const { return x + w; }
    float bottom() const { return y + h; }
};
// 只有面积相交才算重叠；单纯边缘接触不算进入障碍。
bool overlaps(const Rect& a, const Rect& b);

// 移动是持续状态；jump 由主循环转换为单次按下事件。
struct Input { bool left = false, right = false, jump = false; };
// 玩家包含碰撞盒、速度和跳跃次数；落地后补满两次跳跃。
struct Player {
    Rect body{60, 522, 24, 28};
    float vx = 0, vy = 0;
    int jumpsLeft = 2;
    bool grounded = false;
};
// 关卡使用世界坐标；分值数组与对应的尖刺/平台数组按索引匹配。
struct Level {
    std::vector<Rect> platforms;
    std::vector<Rect> spikes;
    Rect checkpoint;
    Rect goal;
    float spawnX, spawnY;
    std::vector<int> spikeScores;
    std::vector<int> platformScores;
    float width = 2240;
    // 创建默认扩展关卡：10 个平台、4 组尖刺和一个存档点。
    static Level tutorial();
};

// 规则层不直接调用 EasyX，便于在不创建窗口的情况下验证物理与通关。
class World {
public:
    // 关卡必须先初始化，随后才能依据尖刺布局构建活动分组。
    Level level = Level::tutorial();
    Player player;
    int deaths = 0;
    bool checkpointActive = false;
    bool won = false;
    bool dead = false;
    std::wstring deathReason;
    ScoreSystem scoring;
    SpikeActivity spikeActivity = SpikeActivity::tutorial(level.spikes);
    float elapsed = 0;

    // 构造时放置角色；每个固定步长推进一次物理与机关。
    World();
    void update(const Input& input, float dt);
    // 重生保留积分和存档点；死亡结算只记录一次；重开清空本局。
    void respawn();
    void die(const wchar_t* reason);
    void restart();
    // 镜头跟随角色但不会超出地图左右边界。
    float cameraX() const;
};
// 测试入口返回 0 表示通过，非零表示发现问题。
int runSelfTests();
}
