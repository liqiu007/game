// 特殊陷阱实现：普通倒挂尖刺与地面联动的上下夹击，共用现有关卡。
#include "SpecialTraps.h"
#include <algorithm>

namespace game {
Rect overheadBounds(const OverheadTrap& trap, const World& world) {
    // 第一步：普通倒刺或失效的关联编号保持原尺寸，不访问无效尖刺。
    Rect bounds = trap.body;
    if (trap.style != TrapStyle::Crusher || trap.linkedSpike >= world.level.spikes.size()) return bounds;
    // 第二步：读取所属地面尖刺组的统一进度，固定上方底座，向下伸长。
    for (const auto& group : world.spikeActivity.traps) {
        if (std::find(group.indices.begin(), group.indices.end(), trap.linkedSpike) == group.indices.end()) continue;
        bounds.h += std::max(0.0f, trap.extendedHeight - bounds.h) * group.extension;
        break;
    }
    return bounds;
}
bool hitOverheadTrap(const OverheadTrap& trap, World& world) {
    // 先计算动态尺寸，再检查角色；危险时交给世界统一冻结并结算分数。
    const auto bounds = overheadBounds(trap, world);
    const Rect hit{bounds.x + 5, bounds.y, bounds.w - 10, bounds.h - 6};
    if (!overlaps(world.player.body, hit)) return false;
    world.hurt(trap.style == TrapStyle::Crusher ? L"你被上下夹击陷阱击中了" : L"你撞上了倒挂尖刺");
    return world.dead;
}
}
