// 玩家血量系统：初始五点，受伤后短暂无敌，耗尽时统一结算死亡。
#include "World.h"
#include <algorithm>
namespace game {
void World::resetBlood() { health = MaxHealth; hurtCooldown = 0; }
void World::updateBlood(float dt) { hurtCooldown = std::max(0.0f, hurtCooldown - dt); }
void World::hurt(const wchar_t* reason) {
    // 同一次接触和同时命中的机关在无敌期内不会连续扣血。
    if (dead || won || hurtCooldown > 0) return;
    health = std::max(0, health - 1);
    hurtCooldown = 1.0f;
    if (health == 0) die(reason);
}
}
