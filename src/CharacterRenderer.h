// 像素鲸鱼娘及碗装米饭接口：待机、跑步、起跳、下落共用内嵌图集。
#pragma once
#include "World.h"
namespace game {
// 角色碰撞盒仍为 24×28；头发、鲸鱼尾与头饰仅为装饰。
void drawWhaleGirl(float centerX, float centerY, float scale, int facing, int palette,
    float time, float runPhase, bool airborne, float squash, float stretch,
    bool running = false, bool falling = false);
void drawRiceBowl(float centerX, float centerY, float scale = 0.65f);
void drawPlayer(const World& world, int palette);
}
