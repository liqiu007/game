#include "SpikeActivity.h"
#include "World.h"
#include <algorithm>
#include <cmath>

namespace game {
SpikeActivity SpikeActivity::tutorial(const std::vector<Rect>& bases) {
    SpikeActivity activity;
    std::vector<bool> grouped(bases.size(), false);
    // Flood-fill adjacency, including chains and spikes stored out of spatial order.
    for (std::size_t seed = 0; seed < bases.size(); ++seed) {
        if (grouped[seed]) continue;
        SpikeTrap group;
        group.indices.push_back(seed);
        grouped[seed] = true;
        for (std::size_t head = 0; head < group.indices.size(); ++head) {
            const auto& a = bases[group.indices[head]];
            for (std::size_t next = 0; next < bases.size(); ++next) {
                const auto& b = bases[next];
                const float gap = std::max({a.x - b.right(), b.x - a.right(), 0.0f});
                if (!grouped[next] && gap <= 4 && std::abs(a.bottom() - b.bottom()) <= 1) {
                    group.indices.push_back(next);
                    grouped[next] = true;
                }
            }
        }
        activity.traps.push_back(group);
    }
    return activity;
}

void SpikeActivity::reset() {
    for (auto& trap : traps) {
        trap.phase = SpikePhase::Idle;
        trap.elapsed = 0;
        trap.extension = 0;
    }
}

void SpikeActivity::update(const Rect& player, const std::vector<Rect>& bases, float dt) {
    if (dt <= 0) return;
    for (auto& trap : traps) {
        if (trap.phase == SpikePhase::Idle) {
            bool nearby = false;
            for (const auto index : trap.indices) {
                if (index >= bases.size()) continue;
                const auto& base = bases[index];
                const float gap = std::max({base.x - player.right(), player.x - base.right(), 0.0f});
                const bool sameArea = player.y < base.bottom() &&
                    player.bottom() > base.bottom() - std::max(base.h, trap.extendedHeight) - 48;
                if (gap <= trap.triggerDistance && sameArea) { nearby = true; break; }
            }
            if (!nearby) continue;
            trap.phase = SpikePhase::Extending;
        }
        if (trap.phase == SpikePhase::Spent) continue;
        trap.elapsed += dt;
        const float rise = std::max(trap.riseSeconds, 0.001f);
        const float hold = std::max(trap.holdSeconds, 0.0f);
        const float retract = std::max(trap.retractSeconds, 0.001f);
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
            // One activation per life: bait the trap, wait, then cross safely.
            trap.phase = SpikePhase::Spent;
            trap.extension = 0;
        }
    }
}

Rect SpikeActivity::bounds(std::size_t index, const Rect& base) const {
    for (const auto& trap : traps) {
        if (std::find(trap.indices.begin(), trap.indices.end(), index) == trap.indices.end()) continue;
        const float height = base.h + (std::max(base.h, trap.extendedHeight) - base.h) * trap.extension;
        // Anchor the base to the floor; grow upward without changing width or score ID.
        return {base.x, base.bottom() - height, base.w, height};
    }
    return base;
}
}
