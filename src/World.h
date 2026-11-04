// 游戏规则与关卡数据：坐标、角色物理、碰撞、积分、重生和镜头计算。
#pragma once
#include "system.h"
#include "Feedback.h"
#include "SpikeActivity.h"
#include <string>
#include <vector>
#include <cstdint>

namespace game {
// 固定窗口尺寸与模拟步长；地图宽度独立于窗口宽度。
inline constexpr int ScreenWidth = 960;
inline constexpr int ScreenHeight = 640;
// 封禁图片绘制与碰撞共用尺寸，保持原图三比二比例。
inline constexpr int BanWidth = 240, BanHeight = 160;
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
struct Input {
    bool left = false, right = false, jump = false, shoot = false, fastFall = false;
    // 模拟输入默认长按；真实键盘传入 W 当前状态，用于短按小跳。
    bool jumpHeld = true;
};
// 支线机关使用单向平台，不改变原有实体平台的碰撞和积分编号。
struct SkyPlatform {
    Rect body;
    float homeX, homeY, travelX = 0, travelY = 0, period = 4;
    bool timed = false;
    float dx = 0, dy = 0;
};
struct Spring { Rect body; float flash = 0; };
enum class FallingPhase { Idle, Warning, Falling, Resting };
struct FallingSpike {
    Rect body;
    float homeY, speed = 0, timer = 0;
    FallingPhase phase = FallingPhase::Idle;
};
struct Feather { Rect body; bool collected = false; };
// 金币位置属于世界坐标，已领取标记在重生时保留，重新挑战时恢复。
struct Coin { Rect body; bool collected = false; };
// 各类机关的数据与运行状态分开保存，重生时恢复初始状态。
struct FragilePlatform { std::size_t index; float timer = 0.65f; bool touched = false, collapsed = false; };
struct Enemy { Rect body; float left, right, speed = 65; int direction = 1; bool alive = true; int avatar = -1; float attackTimer = 0, axeFlash = 0; };
struct Laser { Rect body; float period = 3.4f, activeSeconds = 1.2f, offset = 0; };
struct Saw { Rect body; float originX, range, rate; };
// 水平炮台按固定周期射击；拳头藏在平台内，踩踏后弹出并进入冷却。
struct HorizontalCannon { Rect body; int direction = -1; float period = 2.6f, timer = 0; };
struct SpringFist { Rect body; float timer = 0; };
enum class ShotKind { Normal, Ban, Equation, Smoothness };
struct Projectile { Rect body; float vx, vy, life = 3; bool hostile = false; ShotKind kind = ShotKind::Normal; };
// 特殊陷阱保持世界坐标；夹击类型关联现有地面尖刺，使上下两侧同步动作。
enum class TrapStyle { Ceiling, Crusher };
struct OverheadTrap {
    Rect body;
    TrapStyle style = TrapStyle::Ceiling;
    std::size_t linkedSpike = 0;
    float extendedHeight = 120;
    // 普通倒刺独立奖励；上下夹击与关联的地面尖刺合并计分，配置为 0。
    int points = 100;
};
// 玩家包含碰撞盒、速度和跳跃次数；落地后补满两次跳跃。
struct Player {
    Rect body{60, 522, 24, 28};
    float vx = 0, vy = 0;
    int jumpsLeft = 2;
    // 只在实际起跳时递增，供表现层触发一次音效。
    unsigned jumpSerial = 0;
    bool grounded = false;
    // 参考 JubilWanna：离边容错、落地前缓存，以及起跳/落地的形变动画。
    float coyote = 0, jumpBuffer = 0, jumpCut = 0;
    float animTime = 0, runPhase = 0, squash = 0, stretch = 0, springCooldown = 0;
    float stun = 0;
    int standingSky = -1;
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
    std::vector<OverheadTrap> overheadTraps;
    int theme = 0;
    std::wstring name = L"小试牛刀";
    std::vector<FragilePlatform> fragile;
    std::vector<Enemy> enemies;
    std::vector<Laser> lasers;
    std::vector<Saw> saws;
    bool boss = false;
    std::vector<SkyPlatform> skyPlatforms;
    std::vector<Spring> springs;
    std::vector<FallingSpike> fallingSpikes;
    std::vector<Feather> feathers;
    std::vector<Coin> coins;
    std::vector<HorizontalCannon> cannons;
    std::vector<SpringFist> fists;
    // 创建第一关的基础路线：10 个平台、4 组地面尖刺、倒刺与夹击机关。
    static Level tutorial();
    // 闯关模式按编号创建十五关、循环五种主题；无尽模式在前方继续生成可跳跃的平台。
    static Level campaign(int index);
};

// 规则层不直接调用 EasyX，便于在不创建窗口的情况下验证物理与通关。
class World {
public:
    // 关卡必须先初始化，随后才能依据尖刺布局构建活动分组。
    Level level = Level::tutorial();
    Player player;
    // 血量由 blood.cpp 管理，与重生体力分别计算。
    static constexpr int MaxHealth = 5;
    int health = MaxHealth;
    float hurtCooldown = 0;
    void resetBlood();
    void updateBlood(float dt);
    void hurt(const wchar_t* reason, int damage = 1);
    int deaths = 0;
    static constexpr int InitialStamina = 10;
    int stamina = InitialStamina;
    bool checkpointActive = false;
    bool won = false;
    bool dead = false;
    bool banPending = false;
    DeathBurst deathBurst;
    std::wstring deathReason;
    ScoreSystem scoring;
    SpikeActivity spikeActivity = SpikeActivity::tutorial(level.spikes);
    float elapsed = 0;
    int chapter = 0;
    bool endless = false;
    int facing = 1, bossHealth = 12;
    float shootCooldown = 0, bossTimer = 0;
    std::vector<Projectile> projectiles;

    // 构造时放置角色；每个固定步长推进一次物理与机关。
    explicit World(std::uint32_t seed = 0xC0FFEEu);
    void update(const Input& input, float dt);
    // 死亡结算只记录一次；重生扣费并保留存档点；重开清空本局、恢复体力。
    // 只有死亡且有体力时才执行付费重生；初始化和换关走独立复位入口。
    bool respawn();
    bool canRespawn() const { return dead && (endless || stamina > 0); }
    void die(const wchar_t* reason);
    void restart();
    void load(int index, bool endlessMode = false);
    void extendEndless();
    bool platformSolid(std::size_t index) const;
    // 镜头跟随角色但不会超出地图左右边界。
    float cameraX() const;
private:
    // 复制世界时随机引擎也被复制，使路线搜索和故障复现稳定。
    std::mt19937 spikeRandom;
    void resetPlayer();
};
// 测试入口返回 0 表示通过，非零表示发现问题。
int runSelfTests();
void runExtendedTests();
void runAdventureTests();
void runEconomyTests();
}

