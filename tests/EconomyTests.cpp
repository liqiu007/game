// 金币、重生费用与伪随机回归：验证单次领取、原子扣费、耗尽限制和随机参数稳定性。
#include "World.h"
#include "Account.h"
#include <windows.h>
#include <fstream>
#include "Menu.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace game {
namespace {
void expect(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void ticks(World& w, int count, Input input = {}) { for (int i = 0; i < count; ++i) w.update(input, FixedStep); }
bool sameParameters(const SpikeActivity& a, const SpikeActivity& b) {
    if (a.traps.size() != b.traps.size()) return false;
    for (std::size_t i = 0; i < a.traps.size(); ++i)
        if (a.traps[i].triggerDistance != b.traps[i].triggerDistance || a.traps[i].extensionSpeed != b.traps[i].extensionSpeed) return false;
    return true;
}
}
void runEconomyTests() {
        // 玩家昵称隔离、解锁持久化与空昵称验证，测试不接触真实玩家目录。
    const auto folder = std::filesystem::temp_directory_path() / ("game-profiles-" + std::to_string(GetCurrentProcessId()));
    Account firstPlayer, secondPlayer, restored;
    expect(!firstPlayer.login(L"   ", folder), "Blank nickname accepted");
    expect(firstPlayer.login(L"测试甲", folder) && firstPlayer.unlocked == 1, "New player must start with one level");
    expect(!firstPlayer.complete(1, 3, 100), "Locked level awarded progress");
    expect(firstPlayer.earn(25) && firstPlayer.complete(0, 2, 600), "Progress write failed");
    expect(secondPlayer.login(L"测试乙", folder) && secondPlayer.coins == 0 && secondPlayer.unlocked == 1, "Profiles mixed");
    expect(restored.login(L"测试甲", folder) && restored.coins == 25 && restored.unlocked == 2 &&
        restored.bestStars[0] == 2 && restored.bestScores[0] == 600, "Profile reload failed");
    std::filesystem::remove(firstPlayer.path); std::filesystem::remove(secondPlayer.path); std::filesystem::remove(folder);
    // 临时账户与真实用户存档隔离，验证存盘、余额不足、去重购买及菜单返回。
    Account wallet; wallet.ready = true;
    wallet.path = std::filesystem::temp_directory_path() / ("game-wallet-test-" + std::to_string(GetCurrentProcessId()) + ".txt");
    expect(wallet.earn(40) && wallet.coins == 40, "Wallet credit failed");
    expect(!wallet.buy(1) && wallet.coins == 40, "Insufficient purchase charged coins");
    expect(wallet.buy(0) && wallet.coins == 10 && wallet.owned == 1, "Purchase failed");
    expect(wallet.buy(0) && wallet.coins == 10, "Owned item charged twice");
    { std::ifstream file(wallet.path); int version, balance, owned; file >> version >> balance >> owned;
      expect(version == 3 && balance == 10 && owned == 1, "Account persistence failed"); }
    std::filesystem::remove(wallet.path);
    Menu shop; shop.selected = 5; shop.activate();
    expect(shop.page == Page::Shop && shop.itemCount() == 3, "Shop entry missing");
    shop.selected = 2; shop.activate(); expect(shop.page == Page::Home, "Shop return failed");
    // 无尽重生恢复金币并允许再次计数；闯关重生保留已吃状态。
    World refill; refill.load(0, true);
    refill.level.coins[0].collected = true; refill.scoring.collectCoin(0);
    refill.die(L"刷新金币"); refill.respawn();
    expect(!refill.level.coins[0].collected && refill.scoring.coinsCollected() == 0, "Endless coins not reset");
    refill.scoring.collectCoin(0);
    expect(refill.scoring.coinsCollected() == 1 && refill.scoring.total() == 0, "Reset coin cannot be collected again");
    // 无尽以最远距离计分，金币和战斗不加分，重生不扣费也不限制次数。
    World distanceWorld; distanceWorld.load(0, true);
    distanceWorld.scoring.advanceDistance(100);
    distanceWorld.scoring.collectCoin(0); distanceWorld.scoring.defeatEnemy(0);
    expect(distanceWorld.scoring.total() == 200 && distanceWorld.scoring.coinsCollected() == 1, "Endless distance-only scoring failed");
    distanceWorld.scoring.advanceDistance(50);
    for (int i = 0; i < 15; ++i) { distanceWorld.die(L"无限重生"); expect(distanceWorld.respawn(), "Endless respawn limited"); }
    expect(distanceWorld.scoring.total() == 200 && distanceWorld.stamina == 10, "Endless respawn charged a fee");
    distanceWorld.restart(); expect(distanceWorld.scoring.total() == 0 && distanceWorld.scoring.distanceMeters == 0, "Endless restart retained distance");
    // 混合类型回归：靠近固定组不伸长，活动组仍响应，重生保持类型。
    World mixed; mixed.load(0);
    expect(mixed.spikeActivity.traps[0].retractable && !mixed.spikeActivity.traps[1].retractable, "Mixed spike types missing");
    const auto fixedIndex = mixed.spikeActivity.traps[1].indices.front();
    const auto base = mixed.level.spikes[fixedIndex];
    for (int i = 0; i < 240; ++i) mixed.spikeActivity.update(base, mixed.level.spikes, FixedStep);
    expect(mixed.spikeActivity.bounds(fixedIndex, base).h == base.h &&
        mixed.spikeActivity.traps[1].extension == 0, "Fixed spikes must never extend");
    const auto movingIndex = mixed.spikeActivity.traps[0].indices.front();
    mixed.spikeActivity.update(mixed.level.spikes[movingIndex], mixed.level.spikes, FixedStep);
    expect(mixed.spikeActivity.traps[0].extension > 0, "Moving spikes must still extend");
    mixed.die(L"类型复位测试"); mixed.respawn();
    expect(!mixed.spikeActivity.traps[1].retractable, "Respawn changed fixed spike type");
    // 第一组：开局不扣体力、不自动送金币；真实移动收集两枚，每枚 50 分且只领一次。
    World w; w.load(0);
    expect(w.stamina == 10 && w.scoring.total() == 0 && w.level.coins.size() == 47, "New challenge must start at ten stamina with road coins");
    ticks(w, 3); expect(w.scoring.total() == 0, "Spawn must not overlap a coin");
    expect(!w.respawn() && w.stamina == 10, "Alive player cannot spend a respawn");
    ticks(w, 30, {false, true}); ticks(w, 5);
    expect(w.scoring.total() == 100 && w.scoring.coinsCollected() == 2, "Two collected road coins must award exactly 100");
    w.scoring.collectCoin(0); expect(w.scoring.total() == 100, "Repeated coin ID must not award twice");
    w.scoring.defeatBoss(); const int beforeDeath = w.scoring.total();
    const auto beforeRandom = w.spikeActivity;
    w.die(L"扣费测试"); w.die(L"重复死亡");
    expect(w.stamina == 10 && w.deaths == 1 && w.scoring.deathScore() == beforeDeath, "Death itself must not spend a respawn");
    expect(w.respawn() && w.stamina == 9 && w.scoring.total() == beforeDeath - 100, "Successful respawn must spend exactly one stamina and 100 points");
    expect(!w.respawn() && w.stamina == 9, "Double-clicking respawn must not double charge");
    expect(!sameParameters(beforeRandom, w.spikeActivity), "A new life must draw fresh group parameters");
    ticks(w, 30, {false, true});
    expect(w.scoring.coinsCollected() == 2 && w.scoring.total() == beforeDeath - 100, "Collected coins must stay absent after respawn");
    // 第二组：最后一点可支付最后一次重生，之后死亡只能重开，所有入口共享限制。
    for (int i = 9; i > 0; --i) { w.die(L"消耗体力"); expect(w.respawn() && w.stamina == i - 1, "Each life must consume one remaining respawn"); }
    w.die(L"体力已耗尽");
    const int exhaustedScore = w.scoring.total();
    expect(!w.canRespawn() && !w.respawn() && w.dead && w.stamina == 0 && w.scoring.total() == exhaustedScore, "Exhausted run must reject respawn without extra charges");
    expect(deathScreenClick(400, 360, false) == DeathAction::None && deathScreenClick(400, 440, false) == DeathAction::Restart,
        "Exhausted death screen must disable revive but allow restart");
    w.restart();
    expect(w.stamina == 10 && !w.dead && w.scoring.total() == 0 && w.scoring.coinsCollected() == 0 &&
        std::none_of(w.level.coins.begin(), w.level.coins.end(), [](const auto& c) { return c.collected; }), "Restart must reset stamina, score and coins");
    w.die(L"零分重生"); w.respawn(); expect(w.scoring.total() == 0, "Respawn score cannot become negative");
    w.load(1); expect(w.stamina == 10 && w.scoring.total() == 0, "Changing chapter must start a fresh challenge");
    // 第三组：金币与危险同时触碰时先结算死亡，不让死亡帧获得金币。
    World lethal; lethal.load(0); lethal.level.coins.push_back({{185, 522, 16, 16}});
    lethal.player.body.x = 185; ticks(lethal, 1);
    expect(lethal.dead && !lethal.level.coins.back().collected && lethal.scoring.total() == 0, "Death frame must not collect a coin");
    // 第四组：相同种子和操作可重现，新种子覆盖 5–15 的端点，更新过程不重抽参数。
    World a(42), b(42); a.load(0); b.load(0);
    expect(sameParameters(a.spikeActivity, b.spikeActivity), "Identical seeds must reproduce spike parameters");
    a.die(L"test"); b.die(L"test"); a.respawn(); b.respawn();
    expect(sameParameters(a.spikeActivity, b.spikeActivity), "Identical respawn sequences must reproduce parameters");
    std::set<int> distances, speeds;
    for (std::uint32_t seed = 1; seed <= 128; ++seed) {
        World sample(seed);
        for (const auto& trap : sample.spikeActivity.traps) {
            expect(trap.triggerDistance >= 5 && trap.triggerDistance <= 15 && trap.extensionSpeed >= 5 && trap.extensionSpeed <= 15,
                "Spike distance and speed must stay within inclusive 5–15");
            distances.insert(static_cast<int>(trap.triggerDistance)); speeds.insert(static_cast<int>(trap.extensionSpeed));
        }
    }
    expect(distances.size() == 11 && speeds.size() == 11, "Seed sample must cover both endpoints and varied values");
    const auto original = a.spikeActivity;
    ticks(a, 60); expect(sameParameters(original, a.spikeActivity), "Ordinary frames must not reroll spike parameters");
    // 第五组：边界外不触发、边界内触发；不同时间步的总伸长相同，整组同步。
    const auto bases = a.level.spikes;
    auto sixty = a.spikeActivity, oneTwenty = a.spikeActivity;
    const float distance = sixty.traps[0].triggerDistance;
    sixty.update({bases[0].x - distance - 24 - 0.1f, 522, 24, 28}, bases, FixedStep);
    expect(sixty.traps[0].phase == SpikePhase::Idle, "Outside the sampled trigger radius must not activate");
    const Rect trigger{bases[0].x - distance - 24, 522, 24, 28};
    for (int i = 0; i < 6; ++i) sixty.update(trigger, bases, 1.0f / 60);
    for (int i = 0; i < 12; ++i) oneTwenty.update(trigger, bases, FixedStep);
    const float expectedHeight = 24 + sixty.traps[0].extensionSpeed * 60 * 0.1f;
    expect(std::abs(sixty.bounds(0, bases[0]).h - expectedHeight) < 0.01f &&
        std::abs(sixty.bounds(0, bases[0]).h - oneTwenty.bounds(0, bases[0]).h) < 0.01f, "Extension speed must use elapsed time, not render frames");
    expect(sixty.bounds(0, bases[0]).h == sixty.bounds(1, bases[1]).h, "Neighbors must share randomized extension progress");
    // 第六组：无尽生成追加金币和新组，旧金币、积分和正在伸长的组全部保留。
    World endless(7); endless.load(0, true);
    endless.scoring.collectCoin(0); endless.level.coins[0].collected = true;
    endless.spikeActivity.traps[0].phase = SpikePhase::Holding; endless.spikeActivity.traps[0].extension = 1;
    const auto old = endless.spikeActivity.traps[0]; const auto count = endless.level.coins.size();
    endless.player.body.x = endless.level.width - 200; endless.extendEndless();
    const auto& kept = endless.spikeActivity.traps[0];
    expect(endless.level.coins.size() > count && endless.level.coins[0].collected && endless.scoring.total() == 0,
        "Endless extension must append coins without duplicating old rewards");
    expect(kept.triggerDistance == old.triggerDistance && kept.extensionSpeed == old.extensionSpeed && kept.extension == 1 && kept.phase == old.phase,
        "Endless extension must preserve old spike parameters and state");
}
}

