// 模块说明：账户与存档。一个昵称对应一个文本存档（文件名是昵称的 UTF-16 十六进制）。
// 关键流程：loginInDirectories() 探测可写目录 → 读最新一份旧档 → 迁移写回 → complete() 记录通关。
// 存档目录候选顺序：%LOCALAPPDATA% → 程序目录 saves → 当前目录 saves → 临时目录。
// 写盘使用临时文件 + MoveFileExW 原子替换；失败会置 lastSaveFailed，由 Game.cpp 的 HUD 红字提示。
// 玩家系统：昵称映射到独立文件，内存保留当前玩家，原子落盘保存进度和账户。
// 存档目录在登录时按候选顺序探测可写位置：默认在 %LOCALAPPDATA%，受限进程（例如被沙箱或代理启动）
// 会自动改用程序目录、当前目录或临时目录，避免通关后写入失败却没有任何界面提示。
#include "Account.h"
#include <windows.h>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <climits>
#include <vector>
namespace game {
namespace {
// 每份档案最多记录 15 关的最高星级和最高分，与关卡总数保持一致。
constexpr int SlotCount = 15;

// 读取环境变量；未设置或为空时返回空路径，由调用方决定是否跳过该候选目录。
std::filesystem::path environmentPath(const wchar_t* name) {
    wchar_t* value = nullptr; std::size_t length = 0;
    _wdupenv_s(&value, &length, name);
    std::filesystem::path result;
    if (value && *value) result = value;
    std::free(value);
    return result;
}

// 程序所在目录：默认目录不可写时，优先在安装位置旁边建立 saves/players。
std::filesystem::path executableDirectory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};
    return std::filesystem::path(buffer).parent_path();
}

// 探测目录是否真的可写：建立目录、写入探测文件再删除；任何一步失败都视为不可写。
bool writableDirectory(const std::filesystem::path& directory) {
    if (directory.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    const auto probe = directory / L".save-probe";
    {
        std::ofstream file(probe, std::ios::trunc);
        file << '1'; file.flush();
        if (!file) { std::filesystem::remove(probe, error); return false; }
    }
    std::filesystem::remove(probe, error);
    return true;
}

// 候选存档目录顺序：系统用户目录 → 程序目录 → 当前目录 → 临时目录；重复路径只保留一次。
std::vector<std::filesystem::path> candidateDirectories() {
    std::vector<std::filesystem::path> candidates;
    // 只追加非空且尚未出现过的路径，避免同一位置被探测两次。
    const auto push = [&candidates](const std::filesystem::path& value) {
        if (value.empty()) return;
        if (std::find(candidates.begin(), candidates.end(), value) == candidates.end()) candidates.push_back(value);
    };
    push(environmentPath(L"LOCALAPPDATA") / L"IWannaGame" / L"players");
    push(executableDirectory() / L"saves" / L"players");
    std::error_code error;
    const auto working = std::filesystem::current_path(error);
    if (!error) push(working / L"saves" / L"players");
    push(environmentPath(L"TEMP") / L"IWannaGame" / L"players");
    return candidates;
}

// 昵称转文件名：去掉首尾空格，最多 20 字，用 UTF-16 十六进制编码，任何昵称都不能形成路径。
bool encodeNickname(const std::wstring& name, std::wstring& clean, std::wstring& key) {
    const auto first = name.find_first_not_of(L" ");
    if (first == std::wstring::npos) return false;
    clean = name.substr(first, name.find_last_not_of(L" ") - first + 1);
    if (clean.size() > 20) return false;
    key.clear();
    for (wchar_t c : clean) {
        if (c < 32) return false;
        for (int shift : {12, 8, 4, 0}) key += L"0123456789abcdef"[(c >> shift) & 15];
    }
    return true;
}
}

bool Account::login(const std::wstring& name, std::filesystem::path root) {
    // 显式传入目录时只使用该目录（测试与调试）；正常启动使用完整候选链。
    if (!root.empty()) return loginInDirectories(name, {root});
    return loginInDirectories(name, candidateDirectories());
}

bool Account::loginInDirectories(const std::wstring& name, const std::vector<std::filesystem::path>& directories) {
    std::wstring clean, key;
    if (!encodeNickname(name, clean, key)) return false;
    if (directories.empty()) return false;
    // 第一步：在候选目录里挑出最后写入的一份存档来读取。同一台机器上普通启动和沙箱启动
    // 可能把进度写到不同目录，取最新一份才不会让新解锁被旧档案覆盖回去。
    Account candidate;
    candidate.nickname = clean;
    std::filesystem::path newestFile;
    std::filesystem::file_time_type newestTime = std::filesystem::file_time_type::min();
    for (const auto& folder : directories) {
        if (folder.empty()) continue;
        const auto file = folder / (key + L".txt");
        std::error_code error;
        if (!std::filesystem::exists(file, error) || error) continue;
        const auto stamp = std::filesystem::last_write_time(file, error);
        if (error) continue;
        if (newestFile.empty() || stamp > newestTime) { newestFile = file; newestTime = stamp; }
    }
    if (!newestFile.empty()) {
        std::ifstream input(newestFile);
        int version = 0;
        if (!(input >> version >> candidate.coins >> candidate.owned >> candidate.unlocked) ||
            (version != 2 && version != 3) || candidate.coins < 0 || candidate.owned < 0 || candidate.owned > 3 ||
            candidate.unlocked < 1 || candidate.unlocked > (version == 2 ? 5 : SlotCount)) return false;
        for (int i = 0; i < (version == 2 ? 5 : SlotCount); ++i)
            if (!(input >> candidate.bestStars[i] >> candidate.bestScores[i]) ||
                candidate.bestStars[i] < 0 || candidate.bestStars[i] > 3 || candidate.bestScores[i] < 0) return false;
    }
    // 第二步：选择第一个可写目录作为写入位置；全部不可写时登录失败，由主循环弹窗提示玩家。
    std::filesystem::path chosen;
    for (const auto& folder : directories) {
        if (writableDirectory(folder)) { chosen = folder; break; }
    }
    if (chosen.empty()) return false;
    candidate.directory = chosen;
    candidate.path = chosen / (key + L".txt");
    candidate.fallbackDirectory = directories.size() > 1 && chosen != directories.front();
    candidate.ready = true;
    // 第三步：新档立即建立文件；旧档写入选中目录即完成迁移，并把版本 2 统一升级为版本 3。
    if (!candidate.save(candidate.coins, candidate.owned)) return false;
    *this = candidate;
    return true;
}

bool Account::save(int balance, int items) {
    // 完整档案写入临时文件后替换，失败不修改余额，并记录失败供界面提示。
    if (!ready || balance < 0 || items < 0 || items > 3) return false;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) { lastSaveFailed = true; return false; }
    auto temporary = path; temporary += L".tmp";
    {
        std::ofstream file(temporary, std::ios::trunc);
        file << 3 << ' ' << balance << ' ' << items << ' ' << unlocked;
        for (int i = 0; i < SlotCount; ++i) file << ' ' << bestStars[i] << ' ' << bestScores[i];
        file.flush();
        if (!file) { lastSaveFailed = true; return false; }
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        lastSaveFailed = true; return false;
    }
    coins = balance; owned = items; lastSaveFailed = false; return true;
}

bool Account::earn(int amount) { return amount > 0 && amount <= INT_MAX - coins && save(coins + amount, owned); }

bool Account::buy(int item) {
    // 已拥有物品只装备，不重复支付。
    if (item < 0 || item > 1 || !ready) return false;
    if (owned & (1 << item)) return true;
    const int price = item == 0 ? 30 : 50;
    return coins >= price && save(coins - price, owned | (1 << item));
}

bool Account::resetCampaign() {
    // 仅清除闯关解锁和纪录，金币与外观保留；写入失败不吞掉重试请求。
    unlocked = 1; bestStars.fill(0); bestScores.fill(0);
    return save(coins, owned);
}

bool Account::complete(int chapter, int stars, int score) {
    // 只接受已经开放的关卡；任何星数通关都解锁下一关，最高纪录只增不减。
    if (!ready || chapter < 0 || chapter >= unlocked || chapter > 14 || stars < 0 || stars > 3 || score < 0) return false;
    const Account previous = *this;
    unlocked = std::max(unlocked, std::min(15, chapter + 2));
    bestStars[chapter] = std::max(bestStars[chapter], stars);
    bestScores[chapter] = std::max(bestScores[chapter], score);
    if (save(coins, owned)) return true;
    *this = previous; return false;
}
}
