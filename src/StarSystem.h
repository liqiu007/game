// 星级结算接口：集中配置每关目标，计算独立星级条件，绘制通关页面。
#pragma once
#include <array>

namespace game {
class World;
struct StarRules { int timeLimit; int coinTarget; };
struct StarResult {
    std::array<bool, 3> achieved{};
    int count = 0;
};
enum class VictoryAction { None, Next, Restart, Home };
StarRules starRules(int chapter);
StarResult evaluateStars(const World& world);
void drawVictoryScreen(const World& world, float age = 3);
VictoryAction victoryScreenClick(int x, int y);
void runStarTests();
}
