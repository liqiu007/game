#include "World.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace game {
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void step(World& w, int count, Input input = {}) { for (int i = 0; i < count; ++i) w.update(input, FixedStep); }
void baitFirstTrap(World& w) {
    step(w, 34, {false, true, false});
    step(w, 180);
}
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
        w.player.body = {w.level.goal.x, w.level.goal.bottom() - 28, 24, 28};
        w.update({}, FixedStep);
        check(w.won, "Goal detection failed");
        const float elapsed = w.elapsed;
        step(w, 10, {true, false, false});
        check(w.elapsed == elapsed && w.player.body.x == w.level.goal.x, "Win must freeze simulation");
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
        baitFirstTrap(route);
        jumpTo(route, 365);
        check(route.player.grounded && route.player.body.x >= 340, "First gap is not traversable");
        check(route.scoring.total() == 350 && route.scoring.cleared() == 3, "First two spikes and first landing should score 350");
        jumpTo(route, 555);
        check(route.checkpointActive, "Tutorial route cannot reach checkpoint");
        step(route, 180); // Bait the checkpoint trap and wait for it to retract.
        jumpTo(route, 780);
        check(!route.won, "Old endpoint must no longer finish the expanded map");
        jumpTo(route, 970);
        check(!route.dead && route.player.body.x > ScreenWidth, "Player must move beyond the old screen edge");
        jumpTo(route, 1180);
        step(route, 180);
        jumpTo(route, 1410);
        jumpTo(route, 1640);
        step(route, 180);
        jumpTo(route, 1870);
        jumpTo(route, 2080);
        step(route, 50, {false, true, false});
        check(route.won && route.deaths == 0, "Tutorial must be completable without dying");
        check(route.scoring.total() == 3100 && route.scoring.cleared() == 17, "Expanded route must score all eight spikes and nine platforms");
        check(route.cameraX() == route.level.width - ScreenWidth, "Camera must stop at the map's right edge");
        route.respawn();
        check(route.cameraX() < 200, "Checkpoint respawn must reset camera position");
        route.restart();
        check(route.cameraX() == 0, "Restart must reset camera to map start");
        route.level.width = 500;
        check(route.cameraX() == 0, "A narrow level must not produce a negative camera offset");
        World scored;
        baitFirstTrap(scored);
        jumpTo(scored, 365);
        scored.die(L"Test death");
        check(scored.scoring.deathScore() == 350, "Death screen should snapshot score");
        step(scored, 20, {false, true, true});
        check(scored.scoring.total() == 350, "Cannot score while dead");
        scored.respawn();
        baitFirstTrap(scored);
        jumpTo(scored, 365);
        check(scored.scoring.total() == 350 && scored.scoring.cleared() == 3, "Respawn and repeated obstacles must not farm points");
        scored.restart();
        check(scored.scoring.total() == 0 && scored.scoring.cleared() == 0 && scored.scoring.deathScore() == 0 && !scored.dead,
            "Restart must clear score, settlement and awarded obstacles");
        baitFirstTrap(scored);
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
        World trapTest;
        step(trapTest, 20);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Idle, "Distant players must not trigger traps");
        trapTest.spikeActivity.update({210, 560, 24, 28}, trapTest.level.spikes, FixedStep);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Idle, "Players below platform must not trigger traps");
        step(trapTest, 22, {false, true, false});
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Extending, "Approaching must trigger growth");
        step(trapTest, 12);
        const auto extended = trapTest.spikeActivity.bounds(1, trapTest.level.spikes[1]);
        check(std::abs(extended.h - 148) < 0.01f && extended.bottom() == 550, "Trap must extend upward with fixed base");
        check(trapTest.spikeActivity.bounds(0, trapTest.level.spikes[0]).h == extended.h, "Adjacent spikes must extend together");
        check(trapTest.spikeActivity.bounds(2, trapTest.level.spikes[2]).h == 24, "Distant groups must not activate together");
        step(trapTest, 100);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Retracting, "Trap should retract after hold");
        check(trapTest.spikeActivity.bounds(0, trapTest.level.spikes[0]).h ==
            trapTest.spikeActivity.bounds(1, trapTest.level.spikes[1]).h, "Grouped spikes must retract together");
        step(trapTest, 100);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Spent &&
            trapTest.spikeActivity.bounds(1, trapTest.level.spikes[1]).h == 24, "Trap should return to normal size");
        step(trapTest, 180);
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Spent, "Trap must not retrigger continuously");
        trapTest.respawn();
        check(trapTest.spikeActivity.traps[0].phase == SpikePhase::Idle, "Respawn should rearm traps");
        // A player above the original spike is caught by its growing portion.
        trapTest.player.body = {210, 440, 24, 28};
        trapTest.player.grounded = false;
        step(trapTest, 12);
        check(trapTest.dead && trapTest.deathReason == L"你被突然伸长的尖刺刺中了", "Extended spike collision must be lethal");
        check(trapTest.scoring.total() == 0, "Trap death must not grant points");
        const float frozenExtension = trapTest.spikeActivity.traps[0].extension;
        step(trapTest, 60);
        check(trapTest.spikeActivity.traps[0].extension == frozenExtension, "Trap animation must freeze on death");
        trapTest.restart();
        check(!trapTest.dead && trapTest.spikeActivity.traps[0].extension == 0 &&
            trapTest.spikeActivity.traps[0].phase == SpikePhase::Idle, "Restart must reset trap state");
        trapTest.checkpointActive = true;
        trapTest.respawn();
        step(trapTest, 20);
        check(!trapTest.dead && trapTest.spikeActivity.traps[1].phase == SpikePhase::Holding,
            "Checkpoint spawn must remain safe while nearby trap activates");
        const std::vector<Rect> unorderedSpikes{{148, 526, 24, 24}, {100, 526, 24, 24}, {124, 526, 24, 24},
            {124, 406, 24, 24}, {300, 526, 24, 24}};
        auto groups = SpikeActivity::tutorial(unorderedSpikes);
        check(groups.traps.size() == 3 && groups.traps[0].indices.size() == 3,
            "Grouping must join an unsorted adjacent chain, but separate floors and distant spikes");
        groups.update({190, 522, 24, 28}, unorderedSpikes, FixedStep);
        check(groups.traps[0].phase == SpikePhase::Extending && groups.traps[1].phase == SpikePhase::Idle,
            "Approaching the rightmost group member must trigger the entire group only");
        for (int i = 0; i < 150; ++i) {
            groups.update({190, 522, 24, 28}, unorderedSpikes, FixedStep);
            check(groups.bounds(0, unorderedSpikes[0]).h == groups.bounds(1, unorderedSpikes[1]).h &&
                groups.bounds(1, unorderedSpikes[1]).h == groups.bounds(2, unorderedSpikes[2]).h,
                "Group members must remain synchronized throughout the whole cycle");
        }
        std::cout << "PASS: physics, expanded full route, 3100 score, camera, settlement, grouped traps, collision, reset\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
}
