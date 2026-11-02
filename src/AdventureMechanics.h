// 冒险扩展：参考 JubilWanna 的跳跃容错、移动平台、弹簧、落刺和星羽设计。
// 上游来源与 MIT 许可见 THIRD_PARTY_NOTICES.md，规则层保持不依赖 EasyX。
#pragma once
#include "World.h"
namespace game {
// 加载时建立支线；重生只复位机关，不清除已经收集的星羽。
void buildAdventureRoute(Level& level);
void resetAdventure(World& world);
// 先移动平台并带动乘客，再应用输入；物理后处理单向落地、弹簧和危险。
void advanceAdventure(World& world, float dt);
void updateJumpFeel(Player& player, const Input& input, float dt);
void landOnSky(World& world, float previousBottom);
bool finishAdventure(World& world, float dt);
bool skyActive(const SkyPlatform& platform, float elapsed);
}
