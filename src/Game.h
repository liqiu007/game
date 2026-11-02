// 游戏表现层声明：管理窗口、输入、暂停、绘制和自动截图模式。
#pragma once
#include "World.h"
#include "Menu.h"

namespace game {
// 验证模式只改变启动场景；正常游戏使用 None。
enum class Preview { None, Scene, Death, Spikes, Map, Home, Levels, Settings, Traps, Modes, Window, Bridge, Enemies, Lasers, Boss, Endless, Adventure };
class Game {
public:
    // 初始化窗口，持续处理输入、更新规则和绘图，退出时释放图形资源。
    int run(Preview preview = Preview::None);
private:
    // 世界保存游戏规则状态；暂停只控制主循环是否推进世界。
    World world;
    bool paused = false;
    Menu menu;
    // 先画世界与固定信息，再根据死亡或通关状态覆盖相应界面。
    void render() const;
};
}
