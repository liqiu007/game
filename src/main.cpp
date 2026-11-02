#include "Game.h"
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string_view(argv[1]) == "--self-test") return game::runSelfTests();
    game::Game application;
    const bool deathPreview = argc > 1 && std::string_view(argv[1]) == "--death-smoke-test";
    return application.run(deathPreview || (argc > 1 && std::string_view(argv[1]) == "--smoke-test"), deathPreview);
}
