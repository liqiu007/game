// 活动尖刺声明：相邻尖刺共用一份状态，保证整组同步伸缩。
#pragma once
#include <cstddef>
#include <vector>

namespace game {
struct Rect;
// 一条命内的状态顺序：待触发 → 伸长 → 保持 → 缩回 → 已触发。
enum class SpikePhase { Idle, Extending, Holding, Retracting, Spent };

struct SpikeTrap {
    // 每个编号对应关卡中的一枚尖刺；距离和高度以像素计，时间以秒计。
    std::vector<std::size_t> indices;
    float triggerDistance = 52;
    float extendedHeight = 148;
    float riseSeconds = 0.08f;
    float holdSeconds = 0.75f;
    float retractSeconds = 0.35f;
    // 运行时进度由整组共享；extension 为 0 时正常、为 1 时完全伸长。
    SpikePhase phase = SpikePhase::Idle;
    float elapsed = 0;
    float extension = 0;
};

class SpikeActivity {
public:
    std::vector<SpikeTrap> traps;
    // 根据底座高度与水平间隙自动分组；更换关卡布局后应重新调用。
    static SpikeActivity tutorial(const std::vector<Rect>& bases);
    // 重生时复位状态；模拟时检查靠近触发并推进各组计时。
    void reset();
    void update(const Rect& player, const std::vector<Rect>& bases, float dt);
    // 返回动态外框，供绘制和碰撞共同使用，保证二者一致。
    Rect bounds(std::size_t index, const Rect& base) const;
};
}
