// 六种主题背景与战斗画面：五个闯关主题加无尽星空，不依赖外部素材文件。
#include "SceneRenderer.h"
#include "Combat.h"
#pragma warning(push)
#pragma warning(disable: 4201)
#include <graphics.h>
#pragma warning(pop)
#include <cmath>

namespace game {
namespace {
void fill(int x, int y, int r, int b, COLORREF color) { setfillcolor(color); solidrectangle(x, y, r, b); }
void triangle(int x, int y, int width, int height, COLORREF color) {
    POINT points[3]{{x, y + height}, {x + width / 2, y}, {x + width, y + height}};
    setfillcolor(color); solidpolygon(points, 3);
}
void text(int x, int y, const std::wstring& value, COLORREF color) {
    settextstyle(18, 0, L"Microsoft YaHei"); settextcolor(color); outtextxy(x, y, value.c_str());
}
}
void drawBackground(const World& w) {
    // 第一步：按关卡选择天空底色，逐行渐变，所有图案都只影响显示、不参与碰撞。
    const COLORREF skies[]{RGB(31, 74, 95), RGB(74, 43, 67), RGB(13, 41, 42), RGB(18, 18, 48), RGB(49, 18, 29), RGB(12, 20, 48)};
    const int theme = w.level.theme;
    const COLORREF sky = skies[theme];
    for (int y = 0; y < ScreenHeight; ++y) {
        const int shade = y / 22;
        setlinecolor(RGB(GetRValue(sky) + shade, GetGValue(sky) + shade, GetBValue(sky) + shade));
        line(0, y, ScreenWidth, y);
    }
    const int drift = static_cast<int>(w.cameraX() * 0.22f) % 240;
    // 第二步：每个主题使用独有的轮廓、配色与装饰，镜头移动时产生轻微视差。
    if (theme == 0) {
        setfillcolor(RGB(245, 221, 139)); solidcircle(780, 185, 36);
        for (int x = -240; x < 1200; x += 240) {
            triangle(x - drift, 255, 330, 390, RGB(39, 100, 106));
            triangle(x - drift + 70, 360, 280, 290, RGB(31, 77, 79));
        }
        fill(0, 615, 960, 640, RGB(37, 94, 65));
    } else if (theme == 1) {
        setfillcolor(RGB(241, 151, 113)); solidcircle(750, 220, 54);
        for (int x = -240; x < 1200; x += 240) {
            fill(x - drift, 330, x - drift + 95, 640, RGB(99, 62, 71));
            fill(x - drift + 105, 450, x - drift + 190, 640, RGB(66, 45, 61));
            setlinecolor(RGB(136, 90, 93)); line(x - drift, 375, x - drift + 94, 375);
        }
    } else if (theme == 2) {
        setfillcolor(RGB(161, 207, 166)); solidcircle(775, 185, 31);
        for (int x = -240; x < 1200; x += 120) {
            fill(x - drift + 32, 320, x - drift + 45, 640, RGB(20, 49, 46));
            triangle(x - drift - 20, 195 + (x / 120 % 2) * 25, 120, 330, RGB(23, 66, 56));
        }
    } else if (theme == 3) {
        setlinecolor(RGB(41, 39, 78));
        for (int x = 0; x < 960; x += 48) line(x, 110, x, 640);
        for (int y = 110; y < 640; y += 48) line(0, y, 960, y);
        for (int x = -240; x < 1200; x += 240) {
            fill(x - drift + 40, 180, x - drift + 130, 235, RGB(27, 35, 69));
            fill(x - drift + 48, 191, x - drift + 119, 194, RGB(66, 154, 192));
            fill(x - drift + 48, 207, x - drift + 90, 210, RGB(124, 83, 172));
        }
    } else if (theme == 4) {
        for (int x = -240; x < 1200; x += 240) {
            triangle(x - drift, 240, 280, 400, RGB(67, 28, 38));
            fill(x - drift + 20, 150, x - drift + 42, 640, RGB(38, 19, 29));
        }
        fill(0, 605, 960, 640, RGB(172, 48, 36));
        setlinecolor(RGB(255, 141, 57));
        for (int x = 0; x < 960; x += 12) line(x, 612 + static_cast<int>(std::sin(x * 0.03f + w.elapsed) * 4), x + 8, 615);
    } else {
        for (int i = 0; i < 65; ++i) {
            setfillcolor(RGB(109, 145, 201)); solidcircle((i * 137 + 31) % 960, 120 + (i * 59) % 420, i % 3 == 0 ? 2 : 1);
        }
        for (int x = -240; x < 1200; x += 120)
            fill(x - drift, 450 + (x / 120 % 3) * 25, x - drift + 70, 640, RGB(24, 36, 67));
    }
}
void drawCombatScene(const World& w) {
    // 第一步：将敌人、锯轮与子弹的世界坐标转换到屏幕，使用与碰撞相同的外框。
    const float camera = w.cameraX();
    const auto box = [camera](const Rect& r, COLORREF c) {
        fill(static_cast<int>(r.x - camera), static_cast<int>(r.y), static_cast<int>(r.right() - camera), static_cast<int>(r.bottom()), c);
    };
    for (const auto& enemy : w.level.enemies) if (enemy.alive) {
        box(enemy.body, RGB(239, 133, 76));
        box({enemy.body.x + 4, enemy.body.y + 7, 5, 5}, RGB(255, 239, 194));
        box({enemy.body.x + 17, enemy.body.y + 7, 5, 5}, RGB(255, 239, 194));
    }
    for (const auto& saw : w.level.saws) {
        const int x = static_cast<int>(saw.body.x + 12 - camera), y = static_cast<int>(saw.body.y + 12);
        setfillcolor(RGB(179, 197, 207)); solidcircle(x, y, 12);
        setlinecolor(RGB(240, 246, 255));
        for (int i = 0; i < 8; ++i) {
            const float angle = i * 0.7854f + w.elapsed * 5;
            line(x, y, x + static_cast<int>(std::cos(angle) * 17), y + static_cast<int>(std::sin(angle) * 17));
        }
    }
    // 第二步：激光开启时画实体亮柱，冷却时画暗色虚线，帮助玩家判断通行时机。
    for (const auto& laser : w.level.lasers) {
        box({laser.body.x - 6, laser.body.y - 10, 24, 10}, RGB(100, 116, 149));
        if (laserActive(laser, w.elapsed)) {
            box(laser.body, RGB(255, 63, 128));
            box({laser.body.x + 4, laser.body.y, 4, laser.body.h}, RGB(255, 227, 239));
        } else {
            for (float y = laser.body.y; y < laser.body.bottom(); y += 16)
                box({laser.body.x + 5, y, 2, 6}, RGB(97, 88, 131));
        }
    }
    // 第三步：BOSS 显示身体与独立血条，生命归零后开放终点。
    if (w.level.boss && w.bossHealth > 0) {
        box({2115, 288, 56, 72}, RGB(180, 50, 70));
        box({2123, 304, 12, 10}, RGB(255, 215, 100)); box({2150, 304, 12, 10}, RGB(255, 215, 100));
        fill(330, 120, 630, 134, RGB(68, 27, 36));
        fill(330, 120, 330 + w.bossHealth * 25, 134, RGB(233, 75, 91));
        text(345, 140, L"BOSS  " + std::to_wstring(w.bossHealth) + L" / 12   J 射击", RGB(255, 214, 183));
    }
    for (const auto& shot : w.projectiles) box(shot.body, shot.hostile ? RGB(255, 98, 99) : RGB(255, 231, 102));
}
}
