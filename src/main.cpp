// 程序入口：选择正常游戏或自动验证模式，再把控制权交给游戏主循环。
#include "Game.h"
#include <string_view>

int main(int argc, char* argv[]) {
    // 第一步：自测只运行规则测试，不创建图形窗口，退出码供命令行检查。
    if (argc > 1 && std::string_view(argv[1]) == "--self-test") return game::runSelfTests();
    game::Game application;
    // 第六关交互调试直接启动，不修改玩家正式存档。
    if (argc > 1 && std::string_view(argv[1]) == "--debug-level-6") return application.run(game::Preview::None, true);
    if (argc > 1 && std::string_view(argv[1]) == "--debug-level-6-smoke-test") return application.run(game::Preview::Scene, true);
    // 第二步：解析可选截图参数；没有参数时进入正常可操作的游戏。
    game::Preview preview = game::Preview::None;
    if (argc > 1) {
        const std::string_view option(argv[1]);
        if (option == "--weapons-smoke-test") preview = game::Preview::Weapons;
        if (option == "--smoke-test") preview = game::Preview::Scene;
        if (option == "--death-smoke-test") preview = game::Preview::Death;
        if (option == "--spike-smoke-test") preview = game::Preview::Spikes;
        if (option == "--map-smoke-test") preview = game::Preview::Map;
        if (option == "--menu-smoke-test") preview = game::Preview::Home;
        if (option == "--levels-smoke-test") preview = game::Preview::Levels;
        if (option == "--settings-smoke-test") preview = game::Preview::Settings;
        if (option == "--traps-smoke-test") preview = game::Preview::Traps;
        if (option == "--modes-smoke-test") preview = game::Preview::Modes;
        if (option == "--window-smoke-test") preview = game::Preview::Window;
        if (option == "--bridge-smoke-test") preview = game::Preview::Bridge;
        if (option == "--enemies-smoke-test") preview = game::Preview::Enemies;
        if (option == "--lasers-smoke-test") preview = game::Preview::Lasers;
        if (option == "--boss-smoke-test") preview = game::Preview::Boss;
        if (option == "--final-smoke-test") preview = game::Preview::Final;
        if (option == "--endless-smoke-test") preview = game::Preview::Endless;
        if (option == "--adventure-smoke-test") preview = game::Preview::Adventure;
        if (option == "--victory-smoke-test") preview = game::Preview::Victory;
        if (option == "--victory-full-smoke-test") preview = game::Preview::VictoryFull;
        if (option == "--exhausted-smoke-test") preview = game::Preview::Exhausted;
    }
    // 第三步：启动窗口和游戏循环，并把运行结果返回操作系统。
    return application.run(preview);
}
