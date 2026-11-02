#pragma once
#include "World.h"

namespace game {
enum class Preview { None, Scene, Death, Spikes, Map };
class Game {
public:
    int run(Preview preview = Preview::None);
private:
    World world;
    bool paused = false;
    void render() const;
};
}
