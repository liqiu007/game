// 主页面实现：独立维护菜单状态，使用 EasyX 绘制可交互的游戏入口。
#include "Menu.h"
#include "Account.h"
#include "CharacterRenderer.h"
#pragma warning(push)
#pragma warning(disable: 4201)
#include <graphics.h>
#pragma warning(pop)

namespace game {
namespace {
// 统一按钮位置，命中检测和绘制共用，避免显示与点击区域错位。
constexpr int ButtonX = 64, ButtonY = 242, ButtonWidth = 350, ButtonHeight = 46, ButtonStep = 58;
void text(int x, int y, const std::wstring& value, int size, COLORREF color) {
    settextstyle(size, 0, L"Microsoft YaHei");
    settextcolor(color);
    outtextxy(x, y, value.c_str());
}
void fill(int x, int y, int right, int bottom, COLORREF color) {
    setfillcolor(color);
    solidrectangle(x, y, right, bottom);
}
void spike(int x, int y, bool downward) {
    // 用相同底座宽度绘制正向或倒挂尖刺，作为菜单中的关卡示意。
    POINT points[3]{{x, y}, {x + 24, y}, {x + 12, y + (downward ? 48 : -48)}};
    setfillcolor(RGB(255, 105, 128));
    solidpolygon(points, 3);
}
}
int Menu::itemCount() const {
    // 闯关模式内有十五关和返回按钮；无尽模式直接启动持续生成的地图。
    return page == Page::Shop ? 3 : page == Page::Home ? 6 : (page == Page::Levels ? 16 : (page == Page::Settings ? 5 : (page == Page::Modes ? 3 : 0)));
}
bool Menu::enabled(int index) const {
    return index >= 0 && index < itemCount() && !(page == Page::Levels && index < 15 && index >= account.unlocked) && !(page == Page::Home && index == 1 && !hasSession);
}
std::wstring Menu::itemText(int index) const {
    // 先检查索引，避免无效选项访问页面名称数组。
    if (index < 0 || index >= itemCount()) return L"";
    // 为每种页面生成实际状态文字；设置按钮直接显示当前值。
    if (page == Page::Home) {
        const wchar_t* items[]{L"开始游戏", L"继续游戏", L"游戏模式", L"设置", L"退出游戏", L"游戏商店"};
        return index >= 0 && index < 6 ? items[index] : L"";
    }
    if (page == Page::Shop) {
        if (index == 2) return L"返回主页面";
        const wchar_t* names[]{L"暖阳金配色", L"玫瑰红配色"};
        return std::wstring(names[index]) + ((account.owned & (1 << index)) ? L"  已拥有 / 装备" : (index == 0 ? L"  30 金币" : L"  50 金币"));
    }
    if (page == Page::Modes) {
        const wchar_t* items[]{L"闯关模式", L"无尽模式", L"返回主页面"};
        return items[index];
    }
    if (page == Page::Levels) {
        // 十五张卡片动态生成短标题，返回按钮独立处理。
        return index == 15 ? L"返回游戏模式" : std::to_wstring(index + 1) + L" 关";
    }
    if (page == Page::Settings) {
        if (index == 0) return std::wstring(L"操作提示   ") + (settings.showHints ? L"开启" : L"关闭");
        if (index == 1) {
            const wchar_t* colors[]{L"深海蓝", L"海盐青", L"暮光紫", L"Claude", L"GPT", L"Grok"};
            return std::wstring(L"选择角色   ") + (settings.playerPalette == 6 ? L"暖阳金" : (settings.playerPalette == 7 ? L"玫瑰红" : colors[settings.playerPalette]));
        }
        if (index == 2) return L"帧率上限   " + std::to_wstring(settings.frameLimit) + L" FPS";
        if (index == 3) {
            const wchar_t* sizes[]{L"960 × 640", L"1200 × 800", L"1440 × 960", L"全屏（无边框）"};
            return std::wstring(L"显示模式   ") + sizes[settings.windowSize];
        }
        return L"返回主页面";
    }
    return L"";
}
void Menu::move(int direction) {
    // 循环选择并跳过禁用项，让无进度时的键盘操作跳过继续按钮。
    const int count = itemCount();
    if (count == 0) return;
    do { selected = (selected + direction + count) % count; } while (!enabled(selected));
}
int Menu::hit(int x, int y) const {
    // 间隔区域不算点击，按钮最右和最下边缘也不越界到下一项。
    // 正方形卡片的鼠标命中与显示位置一致。
    if (page == Page::Levels) {
        if (x >= 64 && x < 250 && y >= 550 && y < 590) return 15;
        if (x < 64 || y < 210) return -1;
        int col = (x - 64) / 170, row = (y - 210) / 108;
        return col < 5 && row < 3 && (x - 64) % 170 < 96 && (y - 210) % 108 < 96 ? row * 5 + col : -1;
    }
    if (x < ButtonX || x >= ButtonX + ButtonWidth || y < ButtonY) return -1;
    const int index = (y - ButtonY) / ButtonStep;
    return index < itemCount() && (y - ButtonY) % ButtonStep < ButtonHeight ? index : -1;
}
void Menu::hover(int x, int y) {
    const int index = hit(x, y);
    if (enabled(index)) selected = index;
}
MenuAction Menu::click(int x, int y) {
    // 鼠标点击先检查命中与可用性，再复用键盘确认流程。
    const int index = hit(x, y);
    if (!enabled(index)) return MenuAction::None;
    selected = index;
    return activate();
}
void Menu::home() { page = Page::Home; selected = hasSession ? 1 : 0; }
void Menu::back() {
    // 选关返回模式页，其余菜单返回首页，保留正在进行的游戏。
    if (page == Page::Levels) { page = Page::Modes; selected = 0; } else home();
}
MenuAction Menu::activate() {
    // 第一步：禁止禁用项执行，主页面决定打开子页面还是启动游戏。
    if (!enabled(selected)) return MenuAction::None;
    if (page == Page::Home) {
        if (selected == 1) { page = Page::Playing; return MenuAction::Continue; }
        if (selected == 4) return MenuAction::Exit;
        page = selected == 5 ? Page::Shop : (selected == 3 ? Page::Settings : Page::Modes);
        selected = 0;
    } else if (page == Page::Modes) {
        // 模式选择是唯一入口，闯关才进入选关，无尽直接生成新地图。
        if (selected == 0) { page = Page::Levels; selected = 0; }
        else if (selected == 1) { endlessMode = true; hasSession = true; page = Page::Playing; return MenuAction::Start; }
        else home();
    } else if (page == Page::Levels) {
        // 十五关按进度解锁；通关后还可在游戏内按 Enter 顺序进入下一关。
        if (selected < 15) { chosenLevel = selected; endlessMode = false; hasSession = true; page = Page::Playing; return MenuAction::Start; }
        back();
    } else if (page == Page::Shop) {
        // 原子保存成功才扣款，已拥有的配色再次点击只装备。
        if (selected == 2) home();
        else if (account.buy(selected)) { settings.playerPalette = 6 + selected; shopMessage = L"已装备！"; }
        else shopMessage = L"余额不足或存档不可写，未扣款。";
    } else if (page == Page::Settings) {
        // 第三步：修改可立即生效的设置，设置页面自身不改变世界状态。
        if (selected == 0) settings.showHints = !settings.showHints;
        else if (selected == 1) settings.playerPalette = (settings.playerPalette + 1) % 6;
        else if (selected == 2) settings.frameLimit = settings.frameLimit == 120 ? 60 : 120;
        else if (selected == 3) settings.windowSize = (settings.windowSize + 1) % 4;
        else home();
    }
    return MenuAction::None;
}
void Menu::render() const {
    // 绘制步骤一：沿用游戏的深色网格配色，菜单不会修改关卡背景。
    setbkcolor(RGB(15, 22, 38));
    cleardevice();
    setbkmode(TRANSPARENT);
    setlinecolor(RGB(25, 36, 54));
    for (int x = 0; x < 960; x += 40) line(x, 0, x, 640);
    for (int y = 0; y < 640; y += 40) line(0, y, 960, y);
    text(64, 42, L"GAME / " + (account.nickname.empty() ? L"I WANNA" : account.nickname) + L"    金币 " + std::to_wstring(account.coins), 17, RGB(95, 213, 255));
    const wchar_t* title = page == Page::Shop ? L"游戏商店" : page == Page::Home ? L"极限试炼" : (page == Page::Levels ? L"闯关 / 选择关卡" : (page == Page::Modes ? L"游戏模式" : L"游戏设置"));
    text(60, 86, title, 52, RGB(239, 245, 255));
    text(64, 160, page == Page::Home ? L"每一次重来，都离终点更近。" :
        (page == Page::Levels ? L"十五关挑战，逐关解锁。选择后从该关起点开始。" :
        (page == Page::Modes ? L"完成十五关挑战，或不断突破自己的极限。" : L"设置即时生效；大窗口自动适配屏幕。")), 18, RGB(153, 178, 202));

    // 十五关以五列三行卡片排列，锁定关卡保持灰色。
    if (page == Page::Levels) {
        for (int i = 0; i < 15; ++i) {
            int x = 64 + i % 5 * 170, y = 210 + i / 5 * 108;
            fill(x,y,x+96,y+96,i == selected ? RGB(36,100,125) : RGB(24,36,53));
            text(x+12,y+18,itemText(i),24,enabled(i) ? RGB(239,245,255) : RGB(89,104,122));
            text(x+12,y+60,enabled(i) ? L"挑战" : L"未解锁",15,RGB(153,178,202));
        }
        text(64,550,L"返回游戏模式",20,selected == 15 ? RGB(95,213,255) : RGB(153,178,202));
        text(300,560,L"W / S 选择 · Enter 确认 · Esc 返回",16,RGB(153,178,202));
        return;
    }
    // 绘制步骤二：按钮根据键盘选中或鼠标悬停显示高亮，无进度时继续按钮置灰。
    for (int i = 0; i < itemCount(); ++i) {
        const int y = ButtonY + i * ButtonStep;
        const bool active = enabled(i), focused = i == selected && active;
        fill(ButtonX, y, ButtonX + ButtonWidth, y + ButtonHeight, focused ? RGB(36, 68, 88) : RGB(24, 36, 53));
        setlinecolor(focused ? RGB(95, 213, 255) : RGB(48, 65, 82));
        rectangle(ButtonX, y, ButtonX + ButtonWidth, y + ButtonHeight);
        if (focused) fill(ButtonX, y, ButtonX + 4, y + ButtonHeight, RGB(95, 213, 255));
        text(ButtonX + 22, y + 10, itemText(i), 23, active ? RGB(239, 245, 255) : RGB(89, 104, 122));
        if (page == Page::Levels && i < 15 && i >= account.unlocked) {
            // 用几何线条画锁，不依赖字体是否包含锁字符。
            setlinecolor(RGB(150, 160, 175)); setfillcolor(RGB(100, 110, 125));
            arc(ButtonX + 307, y + 8, ButtonX + 327, y + 29, 0, 3.14159265);
            solidrectangle(ButtonX + 303, y + 21, ButtonX + 331, y + 38);
        }
        if (page == Page::Home && i == 1 && !hasSession) text(ButtonX + 205, y + 15, L"暂无本次进度", 14, RGB(89, 104, 122));
    }

    if (page == Page::Shop) {
        text(64, 195, L"账户金币：" + std::to_wstring(account.coins), 22, RGB(255, 220, 120));
        text(64, 440, shopMessage, 19, RGB(255, 220, 120));
        drawWhaleGirl(650, 360, 3, 1, settings.playerPalette, 0, 0, false, 0, 0);
        return;
    }
    // 绘制步骤三：右侧说明当前关卡主题或玩法，不把选关入口放回主页面。
    fill(492, 188, 900, 544, RGB(21, 32, 49));
    const wchar_t* names[]{L"小试牛刀", L"断桥危机", L"敌人来袭", L"小心激光", L"终章：BOSS战"};
    const wchar_t* themes[]{L"青翠山谷 / 尖刺入门", L"暮色峡谷 / 断桥与锯轮", L"幽暗森林 / 巡逻敌人", L"霓虹实验室 / 定时激光", L"熔岩要塞 / BOSS 弹幕"};
    const int chapter = page == Page::Levels && selected < 15 ? selected : chosenLevel;
    text(520, 210, page == Page::Modes && selected == 1 ? L"无尽模式" : names[chapter % 5], 25, RGB(239, 245, 255));
    text(520, 250, page == Page::Modes && selected == 1 ? L"持续生成 / 距离与积分挑战" : themes[chapter % 5], 17, RGB(153, 178, 202));
    fill(536, 432, 860, 452, RGB(52, 71, 94));
    fill(650, 304, 850, 314, RGB(52, 71, 94));
    for (int i = 0; i < 4; ++i) { spike(674 + i * 26, 432, false); spike(674 + i * 26, 314, true); }
    drawWhaleGirl(595, 385, 2.1f, 1, settings.playerPalette, 0, 0, false, 0, 0);
    text(520, 475, L"成组伸缩 / 倒挂尖刺 / 上下夹击", 17, RGB(255, 157, 170));
    text(520, 508, L"观察机关，找准时机，再跳一次。", 16, RGB(153, 178, 202));
    // 绘制步骤四：给出导航规则和继续游戏的实际存续范围。
    text(64, 591, L"W / S 选择 · D / Enter 确认 · A / Esc 返回", 16, RGB(153, 178, 202));
    text(585, 591, L"Esc 回主页面 · F11 切换全屏", 15, RGB(153, 178, 202));
}
}

