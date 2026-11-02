#include "Game.h"
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string_view(argv[1]) == "--self-test") return game::runSelfTests();
    game::Game application;
    game::Preview preview = game::Preview::None;
    if (argc > 1) {
        const std::string_view option(argv[1]);
        if (option == "--smoke-test") preview = game::Preview::Scene;
        if (option == "--death-smoke-test") preview = game::Preview::Death;
        if (option == "--spike-smoke-test") preview = game::Preview::Spikes;
        if (option == "--map-smoke-test") preview = game::Preview::Map;
    }
    return application.run(preview);
}
