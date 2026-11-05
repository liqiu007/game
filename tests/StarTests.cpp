// 星级回归：覆盖独立条件组合、门槛边界、击杀保留与真实通关冻结。
#include "StarSystem.h"
#include "World.h"
#include <stdexcept>

namespace game {
namespace {
void requireStar(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
void runStarTests() {
    // 第一组：每关枚举八种条件组合，确认不是必须先取得前一星才能取得后一星。
    for (int chapter = 0; chapter < 5; ++chapter) {
        const auto rules = starRules(chapter);
        for (int mask = 0; mask < 8; ++mask) {
            World w; w.load(chapter); w.won = true;
            requireStar(rules.coinTarget <= static_cast<int>(w.level.coins.size()), "Star coin quota exceeds available coins");
            w.elapsed = (mask & 1) ? static_cast<float>(rules.timeLimit) : rules.timeLimit + 0.1f;
            if (chapter == 3 && (mask & 1)) w.scoring.defeatEnemy(0);
            if (chapter == 4 && (mask & 1)) w.scoring.defeatBoss();
            w.level.feathers.front().collected = (mask & 2) != 0;
            const int coins = rules.coinTarget - ((mask & 4) ? 0 : 1);
            for (int i = 0; i < coins; ++i) w.scoring.collectCoin(i);
            const auto result = evaluateStars(w);
            requireStar(result.count == !!(mask & 1) + !!(mask & 2) + !!(mask & 4), "Independent star combination failed");
            for (int i = 0; i < 3; ++i) requireStar(result.achieved[i] == ((mask & (1 << i)) != 0), "Wrong star objective state");
            w.won = false; requireStar(evaluateStars(w).count == 0, "Unfinished challenge cannot settle stars");
            w.won = true; w.endless = true; requireStar(evaluateStars(w).count == 0, "Endless mode cannot settle stars");
        }
    }
    // 第二组：第四关必须存在可击败的敌人；击杀、收集和累计时间跨重生保留，重开清除。
    World w; w.load(3);
    requireStar(!w.level.enemies.empty(), "Laser chapter needs a combat star target");
    w.scoring.defeatEnemy(0); w.scoring.collectCoin(0); w.level.feathers.front().collected = true; w.elapsed = 23;
    w.die(L"星级重生测试"); w.respawn();
    requireStar(w.scoring.hasDefeatedEnemy() && w.scoring.coinsCollected() == 1 &&
        w.level.feathers.front().collected && w.elapsed == 23, "Respawn lost challenge objective progress");
    w.restart();
    requireStar(!w.scoring.hasDefeatedEnemy() && w.scoring.coinsCollected() == 0 &&
        !w.level.feathers.front().collected && w.elapsed == 0, "Restart retained old objectives");
    // 第三组：通过实际终点触发结算，随后规则更新不得改变分数、用时与星级。
    w.load(0); w.elapsed = 30; w.player.body = w.level.goal; w.update({}, FixedStep);
    requireStar(w.won && evaluateStars(w).count == 1, "Goal did not award timed completion star");
    const auto time = w.elapsed; const auto score = w.scoring.total();
    w.update({false, true, true}, 10);
    requireStar(w.elapsed == time && w.scoring.total() == score && evaluateStars(w).count == 1, "Victory did not freeze result");
    // 第 5 关的 BOSS 现在可以选择绕过；第 15 关仍必须击败 BOSS 才开放终点。
    w.load(4); w.player.body = w.level.goal; w.update({}, FixedStep);
    requireStar(w.won, "Optional boss must not lock the fifth chapter goal");
    w.load(14); w.player.body = w.level.goal; w.update({}, FixedStep);
    requireStar(!w.won, "Final chapter must require the boss before victory");
    // 第四组：按钮边界及空白点击映射，防止全屏坐标转换后误操作。
    requireStar(victoryScreenClick(160, 506) == VictoryAction::Next &&
        victoryScreenClick(400, 530) == VictoryAction::Restart &&
        victoryScreenClick(700, 530) == VictoryAction::Home &&
        victoryScreenClick(360, 530) == VictoryAction::None, "Victory button hit regions mismatch");
}
}
