// 六种主题背景与战斗画面：五个闯关主题加无尽星空，不依赖外部素材文件。
#include "SceneRenderer.h"
#include "CharacterRenderer.h"
#include "Combat.h"
#include "AdventureMechanics.h"
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
    // 远处云层和近处微粒沿不同速度移动，参考上游程序化层次背景的做法。
    if (theme < 3) {
        for (int i = 0; i < 5; ++i) {
            const int x = static_cast<int>(std::fmod(i * 251.0f + w.elapsed * (3 + i), 1250.0f)) - 160 - drift / 3;
            const int y = 160 + i * 34;
            setfillcolor(theme == 1 ? RGB(111, 72, 91) : RGB(52, 89, 105));
            solidellipse(x, y, x + 160, y + 24); solidellipse(x + 30, y - 12, x + 105, y + 22);
        }
    }
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
    // 主题粒子只装饰远景：林中萤火、实验室能量点、要塞余烬与山谷光尘。
    for (int i = 0; i < 28; ++i) {
        const int x = (i * 137 + 73 - drift + 960) % 960;
        const int y = 190 + (i * 79) % 380 + static_cast<int>(std::sin(w.elapsed + i) * 6);
        setfillcolor(theme == 4 ? RGB(226, 111, 65) : (theme == 2 ? RGB(150, 193, 115) : RGB(118, 151, 171)));
        solidcircle(x, y, i % 4 == 0 ? 2 : 1);
    }
}
void drawTerrain(const World& w) {
    // 第一步：每种主题提供独立顶面和底材，不改变真实地形外框。
    const COLORREF base[]{RGB(71, 78, 65), RGB(114, 76, 64), RGB(44, 74, 65), RGB(46, 49, 79), RGB(72, 43, 49), RGB(46, 56, 89)};
    const COLORREF top[]{RGB(130, 185, 109), RGB(224, 167, 110), RGB(106, 191, 143), RGB(107, 211, 230), RGB(231, 137, 82), RGB(148, 159, 239)};
    for (std::size_t i = 0; i < w.level.platforms.size(); ++i) {
        if (!w.platformSolid(i)) continue;
        const auto& p = w.level.platforms[i];
        const int x = static_cast<int>(p.x - w.cameraX()), y = static_cast<int>(p.y);
        const int right = static_cast<int>(p.right() - w.cameraX()), bottom = static_cast<int>(p.bottom());
        if (right < 0 || x > ScreenWidth) continue;
        bool fragile = false, touched = false;
        for (const auto& f : w.level.fragile) if (f.index == i) { fragile = true; touched = f.touched; }
        fill(x, y, right - 1, bottom - 1, fragile ? RGB(135, 94, 69) : base[w.level.theme]);
        fill(x, y, right - 1, y + 4, touched ? RGB(255, 116, 75) : top[w.level.theme]);
        // 第二步：石块接缝、桥面裂纹与草沿区分可站立表面和危险机关。
        setlinecolor(RGB(38, 42, 49));
        for (int tx = x + 20; tx < right; tx += 30) {
            if (fragile) { line(tx, y + 5, tx - 7, y + 12); line(tx - 7, y + 12, tx - 2, bottom - 1); }
            else { line(tx, y + 7, tx, bottom - 1); line(tx + 2, y + 13, std::min(tx + 15, right - 1), y + 13); }
        }
        if (w.level.theme == 0 || w.level.theme == 2) {
            setlinecolor(top[w.level.theme]);
            for (int tx = x + 8; tx < right - 4; tx += 17) { line(tx, y, tx - 2, y - 4); line(tx, y, tx + 3, y - 3); }
        }
    }
}
void drawAdventureScene(const World& w) {
    // 金币沿主线和跳跃弧线分布；已收集后隐藏，金币中心与拾取碰撞盒一致。
    for (const auto& coin : w.level.coins) if (!coin.collected) {
        const int x = static_cast<int>(coin.body.x + 8 - w.cameraX()), y = static_cast<int>(coin.body.y + 8);
        if (x < -12 || x > ScreenWidth + 12) continue;
        setfillcolor(RGB(154, 100, 29)); solidcircle(x, y, 9);
        setfillcolor(RGB(255, 207, 65)); solidcircle(x, y, 7);
        setlinecolor(RGB(255, 245, 169)); line(x, y - 4, x, y + 4);
        setfillcolor(RGB(255, 250, 193)); solidcircle(x - 3, y - 3, 2);
    }
    // 第一步：高路用蓝色单向台面，与普通实体平台区分；移动轨迹在背后以虚线绘制。
    const auto rect = [&](const Rect& r, COLORREF c) { fill(static_cast<int>(r.x - w.cameraX()), static_cast<int>(r.y),
        static_cast<int>(r.right() - w.cameraX()), static_cast<int>(r.bottom()), c); };
    for (const auto& p : w.level.skyPlatforms) {
        const int x = static_cast<int>(p.body.x - w.cameraX()), y = static_cast<int>(p.body.y);
        if (p.travelX != 0 || p.travelY != 0) {
            setlinestyle(PS_DOT, 1); setlinecolor(RGB(103, 143, 160));
            line(static_cast<int>(p.homeX + p.body.w / 2 - w.cameraX()), static_cast<int>(p.homeY + 8),
                static_cast<int>(p.homeX + p.travelX + p.body.w / 2 - w.cameraX()), static_cast<int>(p.homeY + p.travelY + 8));
            setlinestyle(PS_SOLID, 1);
        }
        if (skyActive(p, w.elapsed)) {
            rect(p.body, RGB(49, 87, 105)); rect({p.body.x, p.body.y, p.body.w, 4}, p.timed ? RGB(204, 159, 245) : RGB(120, 225, 224));
            for (int tx = x + 8; tx < x + p.body.w - 4; tx += 12) fill(tx, y + 8, tx + 4, y + 10, RGB(100, 143, 162));
        } else {
            setlinecolor(RGB(151, 124, 190)); setlinestyle(PS_DOT, 1);
            rectangle(x, y, static_cast<int>(x + p.body.w), static_cast<int>(y + p.body.h)); setlinestyle(PS_SOLID, 1);
        }
        // 定时台面上方的进度条让玩家看得见即将消失的时机。
        if (p.timed) {
            const float phase = std::fmod(w.elapsed, p.period) / p.period;
            rect({p.body.x, p.body.y - 7, p.body.w * (1 - phase), 2}, RGB(214, 172, 255));
        }
    }
    // 第二步：弹簧橙色顶盖，落刺预警显示感叹号和下落方向线。
    for (const auto& s : w.level.springs) {
        rect(s.body, s.flash > 0 ? RGB(255, 244, 173) : RGB(255, 170, 69));
        rect({s.body.x + 4, s.body.y - 5, s.body.w - 8, 4}, RGB(250, 217, 123));
    }
    for (const auto& f : w.level.fallingSpikes) {
        const int x = static_cast<int>(f.body.x - w.cameraX()), y = static_cast<int>(f.body.y);
        POINT points[]{{x, y}, {x + 24, y}, {x + 12, y + 32}};
        setfillcolor(f.phase == FallingPhase::Warning ? RGB(255, 209, 98) : RGB(234, 112, 148)); solidpolygon(points, 3);
        if (f.phase == FallingPhase::Warning) {
            text(x + 5, y - 24, L"!", RGB(255, 228, 120));
            setlinestyle(PS_DOT, 1); setlinecolor(RGB(241, 186, 97)); line(x + 12, y + 35, x + 12, 300); setlinestyle(PS_SOLID, 1);
        }
    }
    // 第三步：米饭悬浮发光，碰撞区域固定，收集后不再绘制。
    for (const auto& f : w.level.feathers) if (!f.collected) {
        const int x = static_cast<int>(f.body.x + 9 - w.cameraX()), y = static_cast<int>(f.body.y + 13 + std::sin(w.elapsed * 3) * 3);
        drawRiceBowl(static_cast<float>(x), static_cast<float>(y));
    }
    if (!w.level.skyPlatforms.empty()) text(static_cast<int>(45 - w.cameraX()), 338, L"↑ 米饭高路", RGB(213, 234, 203));
}
void drawCombatScene(const World& w) {
    // 第一步：将敌人、锯轮与子弹的世界坐标转换到屏幕，使用与碰撞相同的外框。
    const float camera = w.cameraX();
    const auto box = [camera](const Rect& r, COLORREF c) {
        fill(static_cast<int>(r.x - camera), static_cast<int>(r.y), static_cast<int>(r.right() - camera), static_cast<int>(r.bottom()), c);
    };
    for (const auto& enemy : w.level.enemies) if (enemy.alive) {
        // AI 形象与碰撞盒分离，名字标识怪物种类。
        drawWhaleGirl(enemy.body.x + 13 - w.cameraX(), enemy.body.y + 14, 0.75f, enemy.direction, 3 + std::max(0, enemy.avatar), w.elapsed, 0, false, 0, 0);
        const wchar_t* names[]{L"Claude", L"GPT", L"Grok"};
        settextstyle(12, 0, L"Microsoft YaHei"); settextcolor(RGB(255,220,170));
        outtextxy(static_cast<int>(enemy.body.x - w.cameraX()), static_cast<int>(enemy.body.y - 18), names[std::max(0, enemy.avatar)]);
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
    // 进入竞技场后才显示血条，避免遮住前半段高路的落刺预警。
    if (w.level.boss && w.bossHealth > 0 && w.player.body.x > 1730) {
        box({2115, 288, 56, 72}, RGB(180, 50, 70));
        box({2123, 304, 12, 10}, RGB(255, 215, 100)); box({2150, 304, 12, 10}, RGB(255, 215, 100));
        fill(330, 120, 630, 134, RGB(68, 27, 36));
        fill(330, 120, 330 + w.bossHealth * 25, 134, RGB(233, 75, 91));
        text(345, 140, L"BOSS  " + std::to_wstring(w.bossHealth) + L" / 12   J 射击", RGB(255, 214, 183));
    }
    for (const auto& shot : w.projectiles) box(shot.body, shot.hostile ? RGB(255, 98, 99) : RGB(255, 231, 102));
}
}

