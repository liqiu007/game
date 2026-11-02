// 场景绘制接口：不同关卡的背景与特殊机关，全部在逻辑画布上绘制。
#pragma once
#include "World.h"
namespace game {
void drawBackground(const World& world);
void drawCombatScene(const World& world);
}
