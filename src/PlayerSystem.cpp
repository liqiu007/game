// 玩家系统：昵称映射到独立文件，内存保留当前玩家，原子落盘保存进度和账户。
#include "Account.h"
#include <windows.h>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <climits>
namespace game {
bool Account::login(const std::wstring& name, std::filesystem::path root) {
    // 去掉首尾空格；使用 UTF-16 十六进制编码作为文件名，不允许昵称形成路径。
    const auto first = name.find_first_not_of(L" ");
    if (first == std::wstring::npos) return false;
    const auto clean = name.substr(first, name.find_last_not_of(L" ") - first + 1);
    if (clean.size() > 20) return false;
    std::wstring key;
    for (wchar_t c : clean) {
        if (c < 32) return false;
        for (int shift : {12, 8, 4, 0}) key += L"0123456789abcdef"[(c >> shift) & 15];
    }
    if (root.empty()) {
        wchar_t* value = nullptr; std::size_t length = 0;
        _wdupenv_s(&value, &length, L"LOCALAPPDATA");
        if (!value) return false;
        root = std::filesystem::path(value) / L"IWannaGame" / L"players"; std::free(value);
    }
    // 先读临时对象，坏存档不会覆盖当前内存或旧文件。
    Account candidate; candidate.nickname = clean; candidate.path = root / (key + L".txt");
    std::error_code error;
    const bool exists = std::filesystem::exists(candidate.path, error);
    if (error) return false;
    if (exists) {
        std::ifstream file(candidate.path); int version = 0;
        if (!(file >> version >> candidate.coins >> candidate.owned >> candidate.unlocked) ||
            version != 2 || candidate.coins < 0 || candidate.owned < 0 || candidate.owned > 3 ||
            candidate.unlocked < 1 || candidate.unlocked > 5) return false;
        for (int i = 0; i < 5; ++i)
            if (!(file >> candidate.bestStars[i] >> candidate.bestScores[i]) ||
                candidate.bestStars[i] < 0 || candidate.bestStars[i] > 3 || candidate.bestScores[i] < 0) return false;
    }
    candidate.ready = true;
    if (!exists && !candidate.save(0, 0)) return false;
    *this = candidate; return true;
}
bool Account::save(int balance, int items) {
    // 完整档案写入临时文件后替换，失败不修改余额。
    if (!ready || balance < 0 || items < 0 || items > 3) return false;
    std::error_code error; std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    auto temporary = path; temporary += L".tmp";
    {
        std::ofstream file(temporary, std::ios::trunc);
        file << 2 << ' ' << balance << ' ' << items << ' ' << unlocked;
        for (int i = 0; i < 5; ++i) file << ' ' << bestStars[i] << ' ' << bestScores[i];
        file.flush(); if (!file) return false;
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
    coins = balance; owned = items; return true;
}
bool Account::earn(int amount) { return amount > 0 && amount <= INT_MAX - coins && save(coins + amount, owned); }
bool Account::buy(int item) {
    // 已拥有物品只装备，不重复支付。
    if (item < 0 || item > 1 || !ready) return false;
    if (owned & (1 << item)) return true;
    const int price = item == 0 ? 30 : 50;
    return coins >= price && save(coins - price, owned | (1 << item));
}
bool Account::complete(int chapter, int stars, int score) {
    // 只接受已经开放的关卡；任何星数通关都解锁下一关，最高纪录只增不减。
    if (!ready || chapter < 0 || chapter >= unlocked || chapter > 4 || stars < 0 || stars > 3 || score < 0) return false;
    const Account previous = *this;
    unlocked = std::max(unlocked, std::min(5, chapter + 2));
    bestStars[chapter] = std::max(bestStars[chapter], stars);
    bestScores[chapter] = std::max(bestScores[chapter], score);
    if (save(coins, owned)) return true;
    *this = previous; return false;
}
}
