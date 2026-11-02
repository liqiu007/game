#include "World.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace game {
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void step(World& w, int count, Input input = {}) { for (int i = 0; i < count; ++i) w.update(input, FixedStep); }
void jumpTo(World& w, float target) {
    for (int i = 0; i < 180; ++i) {
        w.update({false, w.player.body.x < target, i == 0 || i == 36}, FixedStep);
        if (i > 36 && w.player.grounded) break;
    }
}
}
int runSelfTests() {
    try {
        World w;
        step(w, 20);
        check(w.player.grounded && std::abs(w.player.body.y - 522) < 0.01f, "Ground landing failed");
        w.update({false, false, true}, FixedStep);
        check(w.player.vy < 0 && w.player.jumpsLeft == 1, "First jump failed");
        step(w, 5);
        w.update({false, false, true}, FixedStep);
        check(w.player.jumpsLeft == 0, "Double jump failed");
        const float before = w.player.vy;
        w.update({false, false, true}, FixedStep);
        check(w.player.vy > before, "Third jump must be rejected");
        step(w, 150);
        check(w.player.grounded && w.player.jumpsLeft == 2, "Landing must replenish jumps");
        w.player.body.x = 185;
        w.update({}, FixedStep);
        check(w.dead && w.deaths == 1 && w.player.body.x == 185, "Spike must show death screen before respawn");
        const float deathTime = w.elapsed;
        step(w, 30, {false, true, true});
        w.die(L"Repeated death");
        check(w.deaths == 1 && w.elapsed == deathTime && w.player.body.x == 185, "Death state must freeze simulation");
        w.respawn();
        check(!w.dead && w.deaths == 1 && w.player.body.x == w.level.spawnX, "Respawn must not count death twice");
        w.player.body = {555, 402, 24, 28};
        w.update({}, FixedStep);
        check(w.checkpointActive, "Checkpoint activation failed");
        w.player.body.y = 800;
        w.update({}, FixedStep);
        check(w.dead && w.deaths == 2, "Falling must settle death");
        w.respawn();
        check(!w.dead && w.player.body.x == 555, "Checkpoint respawn failed");
        w.player.body = {850, 337, 24, 28};
        w.update({}, FixedStep);
        check(w.won, "Goal detection failed");
        const float elapsed = w.elapsed;
        step(w, 10, {true, false, false});
        check(w.elapsed == elapsed && w.player.body.x == 850, "Win must freeze simulation");
        w.restart();
        check(!w.won && !w.checkpointActive && w.deaths == 0 && w.elapsed == 0, "Restart failed");
        w.level.platforms = {{100, 100, 100, 20}};
        w.level.spikes.clear();
        w.player.body = {73, 100, 24, 28};
        step(w, 4, {false, true, false});
        check(w.player.body.right() <= 100.01f, "Side collision failed");
        w.player.body = {120, 121, 24, 28};
        w.player.vy = -400;
        w.update({}, FixedStep);
        check(w.player.body.y >= 120 && w.player.vy == 0, "Ceiling collision failed");
        World route;
        step(route, 20, {false, true, false});
        jumpTo(route, 365);
        check(route.player.grounded && route.player.body.x >= 340, "First gap is not traversable");
        check(route.scoring.total() == 350 && route.scoring.cleared() == 3, "First two spikes and first landing should score 350");
        jumpTo(route, 555);
        check(route.checkpointActive, "Tutorial route cannot reach checkpoint");
        jumpTo(route, 780);
        step(route, 35, {false, true, false});
        check(route.won && route.deaths == 0, "Tutorial must be completable without dying");
        check(route.scoring.total() == 900 && route.scoring.cleared() == 6, "Full route should score 900 across six obstacles");
        World scored;
        step(scored, 20, {false, true, false});
        jumpTo(scored, 365);
        scored.die(L"Test death");
        check(scored.scoring.deathScore() == 350, "Death screen should snapshot score");
        step(scored, 20, {false, true, true});
        check(scored.scoring.total() == 350, "Cannot score while dead");
        scored.respawn();
        step(scored, 20, {false, true, false});
        jumpTo(scored, 365);
        check(scored.scoring.total() == 350 && scored.scoring.cleared() == 3, "Respawn and repeated obstacles must not farm points");
        scored.restart();
        check(scored.scoring.total() == 0 && scored.scoring.cleared() == 0 && scored.scoring.deathScore() == 0 && !scored.dead,
            "Restart must clear score, settlement and awarded obstacles");
        step(scored, 20, {false, true, false});
        jumpTo(scored, 365);
        check(scored.scoring.total() == 350, "New game should allow earning points again");
        World custom;
        custom.level.spikeScores[0] = 75;
        Player beforePlayer, afterPlayer;
        beforePlayer.body = {203, 550, 24, 28};
        afterPlayer.body = {205, 550, 24, 28};
        custom.scoring.update(beforePlayer, afterPlayer, custom.level);
        check(custom.scoring.total() == 0, "Passing underneath a spike should not score");
        beforePlayer.body.y = afterPlayer.body.y = 480;
        custom.scoring.update(beforePlayer, afterPlayer, custom.level);
        check(custom.scoring.total() == 75, "Obstacle-specific point values should be honored");
        check(deathScreenClick(400, 360) == DeathAction::Respawn && deathScreenClick(400, 440) == DeathAction::Restart &&
            deathScreenClick(10, 10) == DeathAction::None, "Death screen button hitboxes failed");
        std::cout << "PASS: physics, full route, scoring, duplicate prevention, death freeze, settlement, respawn, restart, buttons\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
}
