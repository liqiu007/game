// 按键映射：WASD 控制横版角色，跳跃只响应按下瞬间，J 可持续射击。
#pragma once
#include "World.h"
namespace game {
inline Input mapControls(const bool* current, const bool* previous) {
    // 水平方向与快速下落使用持续状态；松开 W 再按才能触发二段跳。
    return {current['A'], current['D'], current['W'] && !previous['W'], current['J'], current['S'], current['W']};
}
}
