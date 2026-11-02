// 啾比造型与动画的 EasyX 改写；保留上游小鸟、蓝披风、星形胸饰的设计。
// Copyright (c) 2026 JubiIWanna contributors，MIT 许可全文见 THIRD_PARTY_NOTICES.md。
#include "CharacterRenderer.h"
#pragma warning(push)
#pragma warning(disable: 4201)
#include <graphics.h>
#pragma warning(pop)
#include <cmath>

namespace game {
void drawBird(float cx, float cy, float scale, int facing, int palette,
    float time, float runPhase, bool airborne, float squash, float stretch) {
    // 第一步：呼吸、跑步与落地压缩统一作用于局部坐标，朝向镜像不改变碰撞盒。
    const float sx = scale * (1 + squash * 0.20f - stretch * 0.05f);
    const float sy = scale * (1 - squash * 0.17f + stretch * 0.10f);
    const float f = facing >= 0 ? 1.0f : -1.0f;
    cy += airborne ? 0 : std::sin(time * 7) * scale;
    const COLORREF bodies[]{RGB(255, 214, 76), RGB(112, 225, 169), RGB(108, 203, 255)};
    const COLORREF body = bodies[palette % 3], outline = RGB(74, 47, 35), beak = RGB(255, 143, 49);
    const auto point = [&](float x, float y) { return POINT{static_cast<LONG>(cx + x * sx), static_cast<LONG>(cy + y * sy)}; };
    const auto oval = [&](float x, float y, float w, float h, COLORREF color) {
        const auto a = point(x, y), b = point(x + w, y + h);
        setfillcolor(color); solidellipse(a.x, a.y, b.x, b.y);
    };
    const auto stroke = [&](float x, float y, float ex, float ey, COLORREF color, int width = 1) {
        const auto a = point(x, y), b = point(ex, ey);
        setlinecolor(color); setlinestyle(PS_SOLID, width); line(a.x, a.y, b.x, b.y);
    };
    // 第二步：先画身后的披风和尾羽，空中飞扬，落地时轻微摆动。
    const float wave = std::sin(time * 7.2f) * 4 + (airborne ? 5 : 0);
    POINT cape[]{point(-f * 8, -12), point(-f * 25, -8 + wave * .4f),
        point(-f * 32, 4 + wave), point(-f * 25, 16 + wave * .6f), point(-f * 8, 13)};
    setfillcolor(RGB(38, 72, 158)); solidpolygon(cape, 5);
    POINT inner[]{point(-f * 9, -8), point(-f * 22, -4 + wave * .3f), point(-f * 27, 6 + wave * .7f), point(-f * 9, 9)};
    setfillcolor(RGB(84, 141, 231)); solidpolygon(inner, 4);
    POINT tail[]{point(-f * 14, 6), point(-f * 27, 2), point(-f * 16, 12)};
    setfillcolor(body); solidpolygon(tail, 3);
    // 第三步：跑步交替迈腿，空中收脚；轮廓、肚皮和头顶呆毛突出圆鸟外形。
    const float step = airborne ? -2 : std::sin(runPhase) * 3;
    stroke(-6, 15, -7 + step, 21, beak, 2); stroke(6, 15, 7 - step, 21, beak, 2);
    oval(-19, -17, 38, 35, outline); oval(-17, -16, 34, 32, body);
    oval(-13, 1, 26, 16, RGB(255, 239, 177));
    stroke(-2, -16, -7, -24, outline, 2); stroke(3, -16, 7, -23, outline, 2);
    oval(-10, -27, 5, 5, body); oval(6, -26, 4, 4, body);
    // 第四步：翅膀在空中加快拍动，眼睛和鸟喙随移动方向转向。
    const float flap = airborne ? std::sin(time * 16) * 5 : std::sin(time * 4);
    oval(-f * 17 - 6, -6 - flap, 13, 16, RGB(219, 178, 69));
    oval(-f * 17 - 4, -5 - flap, 9, 11, RGB(255, 242, 178));
    oval(f * 5 - 5, -10, 10, 10, WHITE); oval(f * 7 - 2, -7, 5, 5, RGB(32, 32, 44));
    oval(f * 12 - 4, -10, 8, 8, WHITE); oval(f * 14 - 2, -8, 4, 4, RGB(32, 32, 44));
    oval(f * 7 - 2, 3, 5, 3, RGB(255, 166, 138));
    POINT mouth[]{point(f * 14, -1), point(f * 24, 2.5f), point(f * 14, 6)};
    setfillcolor(beak); solidpolygon(mouth, 3);
    // 第五步：星形胸饰与披风扣子补充角色识别特征，结束后恢复线宽。
    POINT star[10];
    for (int i = 0; i < 10; ++i) {
        const float a = -1.5707963f + i * 0.6283185f, radius = i % 2 ? 1.5f : 3.4f;
        star[i] = point(-f + std::cos(a) * radius, 10 + std::sin(a) * radius);
    }
    setfillcolor(RGB(255, 250, 216)); solidpolygon(star, 10);
    oval(-f * 9 - 2, -12, 4, 4, RGB(246, 210, 110)); setlinestyle(PS_SOLID, 1);
}
void drawPlayer(const World& w, int palette) {
    // 在脚底绘制阴影，身体中心跟随世界镜头，形变动画只影响外观。
    const auto& p = w.player;
    const float cx = p.body.x + p.body.w / 2 - w.cameraX();
    if (p.grounded) { setfillcolor(RGB(25, 31, 39)); solidellipse(static_cast<int>(cx - 14),
        static_cast<int>(p.body.bottom() - 4), static_cast<int>(cx + 14), static_cast<int>(p.body.bottom() + 2)); }
    drawBird(cx, p.body.bottom() - 14, 0.70f, w.facing, palette, p.animTime, p.runPhase, !p.grounded, p.squash, p.stretch);
}
}
