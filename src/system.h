#pragma once
#include <cstddef>
#include <vector>

namespace game {
struct Player;
struct Level;
class World;
enum class DeathAction { None, Respawn, Restart };

class ScoreSystem {
public:
    int total() const { return score; }
    int cleared() const { return static_cast<int>(awarded.size()); }
    int deathScore() const { return finalScore; }
    void update(const Player& before, const Player& after, const Level& level);
    void settleDeath();
    void reset();
private:
    int score = 0;
    int finalScore = 0;
    std::vector<std::size_t> awarded;
    void award(std::size_t id, int points);
};

// Both the death screen and its clickable button hitboxes live in system.cpp.
void drawDeathScreen(const World& world);
DeathAction deathScreenClick(int x, int y);
}
