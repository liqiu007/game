// 积分与死亡界面的公共接口，具体实现位于 system.cpp。
#pragma once
#include <cstddef>
#include <vector>

namespace game {
struct Player;
struct Level;
class World;
// 将按钮点击转换为动作，实际重生或重开交给游戏主循环执行。
enum class DeathAction { None, Respawn, Restart };

class ScoreSystem {
public:
    static constexpr int CoinPoints = 50;
    static constexpr int RespawnCost = 100;
    int coinsCollected() const { return coinCount; }
    void collectCoin(std::size_t index);
    // 分数最多扣到零，不清除已领奖编号，防止重生刷金币或重复奖励。
    void spendRespawn();
    // 查询实时总分、已计分障碍数和死亡时冻结的分数。
    int total() const { return score; }
    // 无尽只按最远前进距离计分，回头和重生不会重复刷分。
    bool distanceOnly = false;
    int distanceMeters = 0;
    void advanceDistance(float pixels);
    // 金币单独显示计数，不混入 HUD 的障碍奖励数量。
    int cleared() const { return static_cast<int>(awarded.size()) - coinCount; }
    int deathScore() const { return finalScore; }
    // 比较一步前后的玩家位置，只对新越过或落上的障碍计分。
    void update(const Player& before, const Player& after, const Level& level);
    // 死亡保存分数快照，重新开始才清除分数与防重复记录。
    void settleDeath();
    void reset();
    // 战斗奖励也按稳定编号只发放一次，死亡重生后重复击败不重复刷分。
    void defeatEnemy(std::size_t index);
    void defeatBoss();
    // 从一次性奖励记录查询击杀，死亡重生不丢失，重新挑战会清除。
    bool hasDefeatedEnemy() const;
    bool hasDefeatedBoss() const;
    // 星羽为额外探索奖励，与普通障碍编号分开，重生后不重复发分。
    void collectFeather(std::size_t index);
private:
    // awarded 存储已发分的障碍编号，重生时保留以防重复刷分。
    int score = 0;
    int finalScore = 0;
    int coinCount = 0;
    std::vector<std::size_t> awarded;
    // 统一检查分值及障碍编号，再累计一次奖励。
    void award(std::size_t id, int points);
};

// 结算绘制与按钮命中区域共用 system.cpp 中的布局，避免显示和点击错位。
void drawDeathScreen(const World& world);
DeathAction deathScreenClick(int x, int y, bool canRespawn = true);
}
