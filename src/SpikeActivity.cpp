// 尖刺活动实现：自动寻找相邻组，通过一个共享计时器控制整组伸缩。
#include "SpikeActivity.h"
#include "World.h"
#include <algorithm>
#include <cmath>

namespace game {
namespace {
void randomize(SpikeTrap& trap, const std::vector<Rect>& bases, std::mt19937& random) {
    // 每条命每组仅抽取一次距离和速度，全部组员共用，避免每帧阈值跳变。
    std::uniform_int_distribution<int> range(5, 15);
    trap.triggerDistance = static_cast<float>(range(random));
    trap.extensionSpeed = static_cast<float>(range(random));
    float travel = 0;
    for (const auto i : trap.indices) if (i < bases.size()) travel = std::max(travel, trap.extendedHeight - bases[i].h);
    trap.riseSeconds = std::max(0.001f, travel / (trap.extensionSpeed * 60.0f));
    trap.phase = SpikePhase::Idle; trap.elapsed = trap.extension = 0;
}
}
SpikeActivity SpikeActivity::tutorial(const std::vector<Rect>& bases) {
    SpikeActivity activity;
    // 第一步：记录已经分组的尖刺，从每枚尚未分组的尖刺开始搜索。
    std::vector<bool> grouped(bases.size(), false);
    for (std::size_t seed = 0; seed < bases.size(); ++seed) {
        if (grouped[seed]) continue;
        SpikeTrap group;
        group.indices.push_back(seed);
        grouped[seed] = true;
        // 第二步：逐个检查组内成员的邻居，支持连续排列和无序存储的尖刺链。
        for (std::size_t head = 0; head < group.indices.size(); ++head) {
            const auto& a = bases[group.indices[head]];
            for (std::size_t next = 0; next < bases.size(); ++next) {
                const auto& b = bases[next];
                const float gap = std::max({a.x - b.right(), b.x - a.right(), 0.0f});
                // 只有底座同高、水平相邻的尖刺才加入；不同平台不会错误合并。
                if (!grouped[next] && gap <= 4 && std::abs(a.bottom() - b.bottom()) <= 1) {
                    group.indices.push_back(next);
                    grouped[next] = true;
                }
            }
        }
        // 第三步：保存完整分组；每组随后独立计时，组内所有成员共用进度。
        activity.traps.push_back(group);
    }
    return activity;
}

void SpikeActivity::reset(const std::vector<Rect>& bases, std::mt19937& random) {
    // 重生或重开时生成新的伪随机参数；固定种子可以复现整条随机序列用于回归。
    for (auto& trap : traps) randomize(trap, bases, random);
}
void SpikeActivity::rebuild(const std::vector<Rect>& bases, std::mt19937& random) {
    // 先按地图重新分组，再恢复旧组；新地图生成不会使旧陷阱重新触发。
    auto expanded = tutorial(bases);
    for (auto& group : expanded.traps) {
        const auto old = std::find_if(traps.begin(), traps.end(), [&](const auto& candidate) { return candidate.indices == group.indices; });
        if (old != traps.end()) group = *old; else randomize(group, bases, random);
    }
    traps = std::move(expanded.traps);
}

void SpikeActivity::update(const Rect& player, const std::vector<Rect>& bases, float dt) {
    // 第一步：忽略无效时间；尚未启动的组检查角色是否靠近任意成员。
    if (dt <= 0) return;
    for (auto& trap : traps) {
        if (trap.phase == SpikePhase::Idle) {
            bool nearby = false;
            for (const auto index : trap.indices) {
                if (index >= bases.size()) continue;
                const auto& base = bases[index];
                const float gap = std::max({base.x - player.right(), player.x - base.right(), 0.0f});
                // 同时约束高度，避免隔着平台或远离所在楼层时误触发。
                const bool sameArea = player.y < base.bottom() &&
                    player.bottom() > base.bottom() - std::max(base.h, trap.extendedHeight) - 48;
                if (gap <= trap.triggerDistance && sameArea) { nearby = true; break; }
            }
            if (!nearby) continue;
            trap.phase = SpikePhase::Extending;
        }
        // 第二步：已经完成伸缩的组本条命内不再触发，其余组累积活动时间。
        if (trap.phase == SpikePhase::Spent) continue;
        trap.elapsed += dt;
        const float rise = std::max(trap.riseSeconds, 0.001f);
        const float hold = std::max(trap.holdSeconds, 0.0f);
        const float retract = std::max(trap.retractSeconds, 0.001f);
        // 第三步：根据累计时间划分伸长、保持、缩回阶段，插值计算共同进度。
        if (trap.elapsed < rise) {
            trap.phase = SpikePhase::Extending;
            trap.extension = trap.elapsed / rise;
        } else if (trap.elapsed < rise + hold) {
            trap.phase = SpikePhase::Holding;
            trap.extension = 1;
        } else if (trap.elapsed < rise + hold + retract) {
            trap.phase = SpikePhase::Retracting;
            trap.extension = 1 - (trap.elapsed - rise - hold) / retract;
        } else {
            // 一个周期结束后恢复普通大小，玩家可等待后安全跨越。
            trap.phase = SpikePhase::Spent;
            trap.extension = 0;
        }
    }
}

Rect SpikeActivity::bounds(std::size_t index, const Rect& base) const {
    // 找到尖刺所属组，用共享进度计算高度；没有匹配组则保留原始外框。
    for (const auto& trap : traps) {
        if (std::find(trap.indices.begin(), trap.indices.end(), index) == trap.indices.end()) continue;
        const float height = base.h + (std::max(base.h, trap.extendedHeight) - base.h) * trap.extension;
        // 固定底座、向上延伸，宽度和障碍编号保持不变，不影响计分标识。
        return {base.x, base.bottom() - height, base.w, height};
    }
    return base;
}
}
