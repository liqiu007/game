#pragma once
#include <cstddef>
#include <vector>

namespace game {
struct Rect;
enum class SpikePhase { Idle, Extending, Holding, Retracting, Spent };

struct SpikeTrap {
    std::vector<std::size_t> indices; // Adjacent spikes share one timer and phase.
    float triggerDistance = 52;    // Horizontal distance from player edge, in pixels.
    float extendedHeight = 148;
    float riseSeconds = 0.08f;
    float holdSeconds = 0.75f;
    float retractSeconds = 0.35f;
    SpikePhase phase = SpikePhase::Idle;
    float elapsed = 0;
    float extension = 0;          // 0 = normal size, 1 = fully extended.
};

class SpikeActivity {
public:
    std::vector<SpikeTrap> traps;
    static SpikeActivity tutorial(const std::vector<Rect>& bases);
    void reset();
    void update(const Rect& player, const std::vector<Rect>& bases, float dt);
    Rect bounds(std::size_t index, const Rect& base) const;
};
}
