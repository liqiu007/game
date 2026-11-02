// 场景绘制接口：不同关卡的背景与特殊机关，全部在逻辑画布上绘制。
#pragma once
#include "World.h"
namespace game {
// 地形按主题绘制纹理；高路机关显示运动轨迹、预警和星羽位置。
void drawTerrain(const World& world);
void drawAdventureScene(const World& world);
void drawBackground(const World& world);
void drawCombatScene(const World& world);
}
