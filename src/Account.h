// 玩家内存档案接口：实现统一放在 PlayerSystem.cpp。
#pragma once
#include <filesystem>
#include <array>
#include <string>
#include <vector>
namespace game {
class Account {
public:
    int coins = 0, owned = 0, unlocked = 1;
    std::array<int, 15> bestStars{}, bestScores{};
    std::wstring nickname;
    bool ready = false;
    std::filesystem::path path;
    // 实际选中的存档目录；与默认目录不同说明自动改用了备用可写位置。
    std::filesystem::path directory;
    bool fallbackDirectory = false;
    // 最近一次落盘是否失败；界面据此提示，避免通关进度被静默丢弃。
    bool lastSaveFailed = false;
    // 同昵称打开同一份本地档案；root 为空时自动挑选第一个可写目录。
    bool login(const std::wstring& name, std::filesystem::path root = {});
    // 按候选目录顺序登录：读取第一份存在的存档，写入第一个可写目录；测试与回退共用这一入口。
    bool loginInDirectories(const std::wstring& name, const std::vector<std::filesystem::path>& directories);
    bool save(int balance, int items);
    bool earn(int amount);
    bool buy(int item);
    bool resetCampaign();
    bool complete(int chapter, int stars, int score);
};
inline Account account;
}
