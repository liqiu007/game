// 独立星级模块：前三关检查时间，后两关检查战斗；白饭与金币各占一星。
#include "StarSystem.h"
#include "World.h"
#pragma warning(push)
#pragma warning(disable: 4201)
#include <graphics.h>
#pragma warning(pop)
#include <algorithm>
#include <cmath>
#include <string>

namespace game {
StarRules starRules(int chapter) {
    // 试玩暂定值集中在这里；后两关的时间仅作参考，不参与战斗星判定。
    constexpr StarRules rules[]{{90, 12}, {120, 14}, {150, 16}, {180, 18}, {240, 20}};
    return rules[std::clamp(chapter, 0, 4)];
}
StarResult evaluateStars(const World& world) {
    // 只在闯关成功后授星，无尽、死亡和未通关状态都不提前结算。
    StarResult result;
    if (!world.won || world.dead || world.endless) return result;
    const auto rules = starRules(world.chapter);
    result.achieved[0] = world.chapter < 3 ? world.elapsed <= rules.timeLimit :
        (world.chapter == 3 ? world.scoring.hasDefeatedEnemy() : world.scoring.hasDefeatedBoss());
    result.achieved[1] = std::any_of(world.level.feathers.begin(), world.level.feathers.end(),
        [](const auto& rice) { return rice.collected; });
    result.achieved[2] = world.scoring.coinsCollected() >= rules.coinTarget;
    for (bool achieved : result.achieved) if (achieved) ++result.count;
    return result;
}
namespace {
// 按钮与命中共用逻辑坐标，全屏与窗口缩放沿用主循环的坐标转换。
constexpr Rect buttons[]{{160, 506, 200, 54}, {380, 506, 200, 54}, {600, 506, 200, 54}};
void textAt(int x, int y, const std::wstring& text, int size, COLORREF color) {
    settextstyle(size, 0, L"Microsoft YaHei"); settextcolor(color);
    outtextxy(x, y, text.c_str());
}
void centered(int y, const std::wstring& text, int size, COLORREF color) {
    settextstyle(size, 0, L"Microsoft YaHei");
    textAt((ScreenWidth - textwidth(text.c_str())) / 2, y, text, size, color);
}
void star(int x, int y, bool filled) {
    // 外半径与内半径交替构成十个顶点；未获得时仅画边，不填充内部。
    POINT points[10];
    for (int i = 0; i < 10; ++i) {
        const double angle = -1.57079632679 + i * 3.14159265359 / 5;
        const double radius = i % 2 == 0 ? 43 : 19;
        points[i] = {static_cast<LONG>(x + std::cos(angle) * radius), static_cast<LONG>(y + std::sin(angle) * radius)};
    }
    setlinecolor(filled ? RGB(255, 215, 96) : RGB(115, 137, 162));
    setlinestyle(PS_SOLID, 3);
    if (filled) { setfillcolor(RGB(255, 202, 70)); fillpolygon(points, 10); }
    else polygon(points, 10);
    setlinestyle(PS_SOLID, 1);
}
}
VictoryAction victoryScreenClick(int x, int y) {
    // 点击按钮外不执行操作，末关的“下一步”由主循环改为返回首页。
    for (int i = 0; i < 3; ++i) {
        const auto& r = buttons[i];
        if (x >= r.x && x < r.right() && y >= r.y && y < r.bottom())
            return i == 0 ? VictoryAction::Next : (i == 1 ? VictoryAction::Restart : VictoryAction::Home);
    }
    return VictoryAction::None;
}
void drawVictoryScreen(const World& world) {
    // 第一步：覆盖游戏场景，展示关卡标题、总星数、最终分数与实际用时。
    const auto result = evaluateStars(world);
    const auto rules = starRules(world.chapter);
    setfillcolor(RGB(13, 22, 37)); solidrectangle(90, 42, 870, 600);
    setlinecolor(RGB(68, 94, 124)); rectangle(90, 42, 870, 600);
    centered(62, L"挑战完成 / " + world.level.name, 28, RGB(237, 245, 255));
    for (int i = 0; i < 3; ++i) star(360 + i * 120, 164, i < result.count);
    centered(220, std::to_wstring(result.count) + L" / 3 星     得分 " + std::to_wstring(world.scoring.total()), 28, RGB(255, 220, 117));
    centered(261, L"用时 " + std::to_wstring(static_cast<int>(std::ceil(world.elapsed))) + L" 秒    死亡 " +
        std::to_wstring(world.deaths) + L" 次", 20, RGB(171, 190, 211));
    // 第二步：逐项显示获星原因，独立达成任意一项都加一星，无需按顺序领取。
    const std::wstring first = world.chapter < 3 ? L"限时通关：不超过 " + std::to_wstring(rules.timeLimit) + L" 秒" :
        (world.chapter == 3 ? L"战斗目标：击败至少一名巡逻敌人" : L"战斗目标：击败最终 BOSS");
    const std::wstring objectives[]{first, L"探索目标：吃到本关白饭", L"金币目标：" +
        std::to_wstring(world.scoring.coinsCollected()) + L" / " + std::to_wstring(rules.coinTarget) + L" 枚"};
    for (int i = 0; i < 3; ++i)
        textAt(194, 310 + i * 48, (result.achieved[i] ? L"已达成   " : L"未达成   ") + objectives[i], 23,
            result.achieved[i] ? RGB(125, 228, 175) : RGB(154, 170, 192));
    // 第三步：保留快捷键，同时提供可点击的下一关、重玩与首页按钮。
    const wchar_t* labels[]{world.chapter < 4 ? L"下一关  Enter" : L"返回首页  Enter", L"重玩本关  R", L"主页面  Esc"};
    for (int i = 0; i < 3; ++i) {
        const auto& r = buttons[i];
        setfillcolor(i == 0 ? RGB(49, 104, 108) : RGB(44, 60, 82));
        solidrectangle(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.right()), static_cast<int>(r.bottom()));
        settextstyle(21, 0, L"Microsoft YaHei");
        textAt(static_cast<int>(r.x) + (200 - textwidth(labels[i])) / 2, 521, labels[i], 21, RGB(239, 245, 255));
    }
}
}
