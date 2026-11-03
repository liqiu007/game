// 本地账户：仅正常游戏接入，不让模拟路线和截图向真实账户发币。
#pragma once
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <windows.h>
namespace game {
class Account {
public:
    int coins = 0, owned = 0;
    bool ready = false;
    std::filesystem::path path;
    // 使用用户目录，临时文件写完后原子替换；损坏存档禁止覆盖。
    void load() {
        wchar_t* root = nullptr; std::size_t length = 0; _wdupenv_s(&root, &length, L"LOCALAPPDATA");
        if (!root) return;
        path = std::filesystem::path(root) / L"IWannaGame" / L"account.txt";
        std::free(root);
        if (!std::filesystem::exists(path)) { ready = true; return; }
        std::ifstream file(path); int version;
        if (file >> version >> coins >> owned && version == 1 && coins >= 0 && owned >= 0 && owned <= 3) ready = true;
    }
    bool save(int balance, int items) {
        if (!ready) return false;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
        auto temporary = path; temporary += L".tmp";
        { std::ofstream file(temporary, std::ios::trunc); file << 1 << ' ' << balance << ' ' << items; file.flush(); if (!file) return false; }
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
        coins = balance; owned = items; return true;
    }
    bool earn(int amount) { return amount > 0 && amount <= INT_MAX - coins && save(coins + amount, owned); }
    bool buy(int item) {
        if (item < 0 || item > 1) return false;
        if (owned & (1 << item)) return true;
        const int price = item == 0 ? 30 : 50;
        return coins >= price && save(coins - price, owned | (1 << item));
    }
};
inline Account account;
}
