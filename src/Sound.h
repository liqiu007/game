// Windows 音效播放：预合成到内存，异步播放不会阻塞物理或绘图。
#pragma once
#include "Feedback.h"
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

namespace game {
class Sound {
    // 内存必须保留至异步播放结束；析构时先停止，再释放缓冲。
    const std::vector<unsigned char> jump = synthesizeSound(false);
    const std::vector<unsigned char> death = synthesizeSound(true);
public:
    ~Sound() { PlaySoundW(nullptr, nullptr, 0); }
    // 死亡替换当前跳跃声；设备不可用时保持静音，不影响游戏运行。
    void play(bool died) const {
        const auto& wav = died ? death : jump;
        PlaySoundW(reinterpret_cast<LPCWSTR>(wav.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }
};
}
