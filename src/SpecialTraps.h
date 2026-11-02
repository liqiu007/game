// 倒挂与夹击陷阱接口：尺寸由同一函数提供给碰撞和画面，避免视觉不一致。
#pragma once
#include "World.h"

namespace game {
// 静态倒刺保持原始尺寸，夹击倒刺读取关联地面尖刺组的伸缩比例。
Rect overheadBounds(const OverheadTrap& trap, const World& world);
// 统一缩小倒三角的尖端判定，命中后返回对应死亡原因。
bool hitOverheadTrap(const OverheadTrap& trap, World& world);
}
