// 反馈数据与音色合成：不依赖窗口或音频设备，便于无窗口验证。
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace game {
struct BloodDrop { float x, y, vx, vy, life, radius; };
class DeathBurst {
public:
    std::vector<BloodDrop> drops;
    float age = 0;
    // 每次死亡只创建一批固定数量的血滴，不干扰游戏使用的随机数。
    void emit(float x, float y) {
        clear();
        for (int i = 0; i < 48; ++i) {
            const float angle = i * 2.399963f;
            const float speed = 90.0f + (i * 37 % 190);
            drops.push_back({x, y, std::cos(angle) * speed,
                std::sin(angle) * speed - 150, 0.7f + (i % 9) * 0.08f, 2.0f + i % 3});
        }
    }
    // 仅推进表现计时；世界死亡后仍可播放，重生立即清理。
    void update(float dt) {
        age += dt;
        for (auto& p : drops) {
            p.x += p.vx * dt;
            p.y += p.vy * dt + 360 * dt * dt;
            p.vy += 720 * dt;
            p.life -= dt;
        }
        drops.erase(std::remove_if(drops.begin(), drops.end(),
            [](const BloodDrop& p) { return p.life <= 0; }), drops.end());
    }
    void clear() { drops.clear(); age = 0; }
};

// 生成单声道 16 位 PCM WAV：跳跃上扬，死亡低沉下滑并叠加短促噪声。
inline std::vector<unsigned char> synthesizeSound(bool death) {
    constexpr int rate = 22050;
    const int samples = death ? rate / 2 : rate / 7;
    std::vector<unsigned char> wav(44 + samples * 2, 0);
    const auto put = [&](int at, std::uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) wav[at + i] = static_cast<unsigned char>(value >> (i * 8));
    };
    const auto tag = [&](int at, const char* value) {
        for (int i = 0; i < 4; ++i) wav[at + i] = value[i];
    };
    tag(0, "RIFF"); put(4, 36 + samples * 2, 4); tag(8, "WAVE");
    tag(12, "fmt "); put(16, 16, 4); put(20, 1, 2); put(22, 1, 2);
    put(24, rate, 4); put(28, rate * 2, 4); put(32, 2, 2); put(34, 16, 2);
    tag(36, "data"); put(40, samples * 2, 4);
    double phase = 0;
    std::uint32_t noise = 1234567;
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / (samples - 1);
        phase += 6.283185307179586 * (death ? 180 - 135 * t : 380 + 560 * t) / rate;
        noise = noise * 1664525u + 1013904223u;
        const double hiss = static_cast<double>(noise >> 16) / 32767.5 - 1;
        const double envelope = std::min(1.0, t * 40) * (1 - t) * (1 - t);
        const double tone = death ? 0.7 * std::sin(phase) + 0.3 * hiss * (1 - t) : std::sin(phase);
        const auto pcm = static_cast<std::int16_t>(tone * envelope * 14000);
        put(44 + i * 2, static_cast<std::uint16_t>(pcm), 2);
    }
    return wav;
}
}
