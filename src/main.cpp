// 程序入口：选择正常游戏或自动验证模式，再把控制权交给游戏主循环。
#include "Game.h"
#include <string_view>

int main(int argc, char* argv[]) {
    // 第一步：自测只运行规则测试，不创建图形窗口，退出码供命令行检查。
    if (argc > 1 && std::string_view(argv[1]) == "--self-test") return game::runSelfTests();
    game::Game application;
    // 第二步：解析可选截图参数；没有参数时进入正常可操作的游戏。
    game::Preview preview = game::Preview::None;
    if (argc > 1) {
        const std::string_view option(argv[1]);
        if (option == "--smoke-test") preview = game::Preview::Scene;
        if (option == "--death-smoke-test") preview = game::Preview::Death;
        if (option == "--spike-smoke-test") preview = game::Preview::Spikes;
        if (option == "--map-smoke-test") preview = game::Preview::Map;
    }
    // 第三步：启动窗口和游戏循环，并把运行结果返回操作系统。
    return application.run(preview);
}
