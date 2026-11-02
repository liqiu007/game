// 扩展机关和战斗接口：独立于绘图，便于无窗口验证死亡与攻击。
#pragma once
#include "World.h"
namespace game {
bool laserActive(const Laser& laser, float elapsed);
bool updateCombat(World& world, const Input& input, const Player& before, float dt);
}
