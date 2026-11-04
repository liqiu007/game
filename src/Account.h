// 玩家内存档案接口：实现统一放在 PlayerSystem.cpp。
#pragma once
#include <filesystem>
#include <array>
#include <string>
namespace game {
class Account {
public:
    int coins = 0, owned = 0, unlocked = 1;
    std::array<int, 15> bestStars{}, bestScores{};
    std::wstring nickname;
    bool ready = false;
    std::filesystem::path path;
    // 同昵称打开同一份本地档案；可传入隔离目录供测试使用。
    bool login(const std::wstring& name, std::filesystem::path root = {});
    bool save(int balance, int items);
    bool earn(int amount);
    bool buy(int item);
    bool resetCampaign();
    bool complete(int chapter, int stars, int score);
};
inline Account account;
}
