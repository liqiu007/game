#pragma once
#include "World.h"

namespace game {
class Game {
public:
    int run(bool smokeTest = false, bool deathPreview = false);
private:
    World world;
    bool paused = false;
    void render() const;
};
}
