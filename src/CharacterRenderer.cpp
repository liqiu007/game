// 像素鲸鱼娘与米饭绘制：使用内嵌图集，运行时无需外部图片文件。
#include "CharacterRenderer.h"
#include "CharacterSprites.h"
#pragma warning(push)
#pragma warning(disable: 4201)
#include <graphics.h>
#pragma warning(pop)
#include <algorithm>
#include <cmath>

namespace game {
namespace {
// 最近邻绘制保留像素边缘；镜像、形变与配色仅改变外观。
void drawSprite(int frame, float cx, float bottom, float sx, float sy, int facing, int palette) {
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) {
        const int index = sprites::index(sprites::Frames[frame][y][x]);
        if (!index) continue;
        const auto color = sprites::Palette[index];
        int r = (color >> 16) & 255, g = (color >> 8) & 255, b = color & 255;
        // 两种可选海洋配色只调整蓝色区域，白围裙、肤色和米饭不受影响。
        if (palette && b > r + 25 && b > g + 10) {
            if (palette == 1) { g = std::min(255, g + 55); b = std::max(g, b - 10); }
            else { r = std::min(255, r + 45); }
        }
        const int px = facing >= 0 ? x : 63 - x;
        const int left = static_cast<int>(std::round(cx + (px - 32) * sx));
        const int top = static_cast<int>(std::round(bottom + (y - 58) * sy));
        const int right = static_cast<int>(std::round(cx + (px - 31) * sx));
        const int lower = static_cast<int>(std::round(bottom + (y - 57) * sy));
        if (right <= left || lower <= top) continue;
        setfillcolor(RGB(r, g, b));
        solidrectangle(left, top, right - 1, lower - 1);
    }
}
}
void drawWhaleGirl(float cx, float cy, float scale, int facing, int palette,
    float time, float runPhase, bool airborne, float squash, float stretch, bool running, bool falling) {
    // 待机轻微呼吸；跑步交替两帧，空中区分上升和下落姿势。
    const int frame = airborne ? (falling ? 4 : 3) : (running ? 1 + (static_cast<int>(runPhase * 2) & 1) : 0);
    const float bob = airborne ? 0 : std::sin(running ? runPhase * 4 : time * 3) * scale;
    drawSprite(frame, cx, cy + 29 * scale + bob,
        scale * (1 + squash * 0.16f - stretch * 0.04f),
        scale * (1 - squash * 0.14f + stretch * 0.08f), facing, palette);
}
void drawRiceBowl(float cx, float cy, float scale) {
    // 碗的内容是白米饭；与角色共用像素图集，悬浮位置由场景层提供。
    drawSprite(5, cx, cy + 23 * scale, scale, scale, 1, 0);
}
void drawPlayer(const World& w, int palette) {
    // 脚底与原碰撞盒对齐，长发与鲸鱼尾巴不扩大受伤范围。
    const auto& p = w.player;
    const float cx = p.body.x + p.body.w / 2 - w.cameraX();
    if (p.grounded) {
        setfillcolor(RGB(25, 31, 39));
        solidellipse(static_cast<int>(cx - 14), static_cast<int>(p.body.bottom() - 3),
            static_cast<int>(cx + 14), static_cast<int>(p.body.bottom() + 2));
    }
    constexpr float scale = 0.85f;
    drawWhaleGirl(cx, p.body.bottom() - 29 * scale, scale, w.facing, palette,
        p.animTime, p.runPhase, !p.grounded, p.squash, p.stretch, std::abs(p.vx) > 1, p.vy > 0);
}
}
