// 主页面状态与设置：负责导航和选项，游戏规则仍由 World 负责。
#pragma once
#include <string>

namespace game {
enum class Page { Home, Modes, Levels, Settings, Playing };
enum class MenuAction { None, Start, Continue, Exit };
struct GameSettings {
    // 设置在本次运行中生效：提示开关、三种角色配色、绘制帧率上限。
    bool showHints = true;
    int playerPalette = 0;
    int frameLimit = 120;
    int windowSize = 0;
};
class Menu {
public:
    Page page = Page::Home;
    bool hasSession = false;
    int selected = 0;
    int chosenLevel = 0;
    bool endlessMode = false;
    GameSettings settings;
    // 按当前页面列出按钮，禁用没有游戏进度时的“继续游戏”。
    int itemCount() const;
    bool enabled(int index) const;
    std::wstring itemText(int index) const;
    // 鼠标与键盘共用同一导航和执行入口，保证两种操作结果一致。
    void move(int direction);
    void hover(int x, int y);
    MenuAction click(int x, int y);
    MenuAction activate();
    void home();
    void back();
    // 绘制主页面、模式页、五关选择页和设置页，不推进世界计时。
    void render() const;
private:
    int hit(int x, int y) const;
};
}
