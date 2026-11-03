// 活动尖刺声明：相邻尖刺共用一份状态，保证整组同步伸缩。
#pragma once
#include <cstddef>
#include <vector>
#include <random>

namespace game {
struct Rect;
// 一条命内的状态顺序：待触发 → 伸长 → 保持 → 缩回 → 已触发。
enum class SpikePhase { Idle, Extending, Holding, Retracting, Spent };

struct SpikeTrap {
    // 每个编号对应关卡中的一枚尖刺；距离和高度以像素计，时间以秒计。
    std::vector<std::size_t> indices;
    float triggerDistance = 10;
    float extendedHeight = 148;
    // 速度单位为像素/60 Hz 参考步；实际更新按秒换算，与渲染帧率无关。
    float extensionSpeed = 10;
    float riseSeconds = 124.0f / 600.0f;
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
    void reset(const std::vector<Rect>& bases, std::mt19937& random);
    // 无尽新增尖刺时只随机化新组，旧组参数和进度完整保留。
    void rebuild(const std::vector<Rect>& bases, std::mt19937& random);
    void update(const Rect& player, const std::vector<Rect>& bases, float dt);
    // 返回动态外框，供绘制和碰撞共同使用，保证二者一致。
    Rect bounds(std::size_t index, const Rect& base) const;
};
}
