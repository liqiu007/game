// 小鸟角色绘制：从 JubilWanna 的原创啾比造型适配为 EasyX，许可见第三方说明。
#pragma once
#include "World.h"
namespace game {
// 角色碰撞盒保持 24×28；翅膀、披风和呆毛仅作为装饰，不扩大受伤范围。
void drawBird(float centerX, float centerY, float scale, int facing, int palette,
    float time, float runPhase, bool airborne, float squash, float stretch);
void drawPlayer(const World& world, int palette);
}
