# game

C++17 + EasyX 的 I Wanna 风格桌面平台跳跃基础框架，使用 Visual Studio 开发。当前提供一个用几何图形绘制的可玩测试关卡，不需要图片或音频资源。

## 在 Visual Studio 中运行

1. 安装 Visual Studio 2022 或 2026，勾选“使用 C++ 的桌面开发”及 Windows SDK。
2. 按 [EasyX 官方安装说明](https://easyx.cn/setup)，将 EasyX 安装到对应的 Visual Studio。
3. 打开根目录的 `game.sln`，选择 **Debug / x64**。
4. 按 **F5** 调试，或 **Ctrl+F5** 直接运行。

项目在 VS 2026 使用 v145 工具集，在 VS 2022 使用 v143。支持 Debug/Release 和 x64/Win32。当前机器已安装 VS 2026 和 EasyX 26.10.25；VS 2022 尚未实机验证。

默认从 `$(VCInstallDir)Auxiliary\VS` 查找 EasyX。若使用自定义安装目录，可用 MSBuild 参数 `/p:EasyXRoot="你的目录"` 指定；目录下应有 `include/graphics.h` 和 `lib/x64/EasyXw.lib`（Win32 对应 `lib/x86`）。EasyX 库文件不包含在仓库中。

输出程序：`bin/x64/Debug/game.exe` 或 `bin/x64/Release/game.exe`。Release 使用静态 C++ 运行时，便于在 Windows 上单文件试玩。

## 操作

| 按键 | 功能 |
| --- | --- |
| A / D 或左右方向键 | 左右移动 |
| 空格 / Z | 跳跃；在空中松开再按可二段跳 |
| R | 游戏中主动结算死亡；死亡界面重生；通关后重新开始 |
| Enter | 在死亡界面重生 |
| Shift + R | 清除本局进度并重新开始 |
| P | 暂停 / 继续 |
| Esc | 退出 |

触碰尖刺或掉出画面后，游戏冻结并显示 Minecraft 风格的死亡结算界面：暗红色场景、死亡原因、黄色分数和灰色立体按钮。可以用鼠标点击“重生”或“重新开始”，也可以使用对应快捷键。绿色旗帜激活存档点，右上方金色门是终点。失去窗口焦点时自动停止模拟。存档仅在本次运行中有效。

## 积分规则

- 三个尖刺各 100 分：角色完整向右越过尖刺，并位于其上方才计分。
- 三段平台跨越分别为 150、200、250 分：安全落到对应平台时计分。
- 一局最多 900 分，同一障碍每局只计一次，往返或死亡重生不会刷分。
- 死亡界面显示死亡时的本局累计积分；重生保留积分、已通过障碍记录和存档点。
- “重新开始”或 Shift+R 清空积分、障碍记录、死亡次数和存档点，可以重新获得分数。
- 计分在碰撞与死亡判断之后执行，死亡当帧不会额外加分。

计分与死亡界面实现集中在 `src/system.cpp`，声明位于 `src/system.h`。障碍分值在 `Level::tutorial()` 的 `spikeScores` 和 `platformScores` 中配置，分别按尖刺和平台索引对应；尖刺未配置时默认 100 分，平台未配置时默认不计分。当前关卡几何在一局中保持固定；替换关卡时应重置积分系统。

## 代码结构

```text
game.sln / game.vcxproj  Visual Studio 解决方案和项目配置
src/main.cpp            程序入口和验证参数
src/Game.h / Game.cpp    EasyX 窗口、键盘输入、固定步长循环、绘制
src/World.h / World.cpp  角色、关卡数据、重力、碰撞、重生与通关规则
src/system.h / system.cpp 积分、防重复计分、死亡结算界面及按钮命中判断
tests/WorldTests.cpp     无窗口的物理与完整通关路线测试
```

模拟以 120 Hz 固定步长更新，绘制采用 EasyX 批量绘图。移动和碰撞分 X/Y 轴处理。角色和平台使用矩形碰撞，尖刺使用略小于视觉轮廓的矩形判定。

修改 `Level::tutorial()` 可添加平台、尖刺，调整出生点、存档点和终点。坐标原点位于左上角，X 向右、Y 向下；速度、重力和跳跃参数在 `World::update()` 中。建议先在此框架上扩展关卡加载、角色动画、音效和移动陷阱。

## 验证

在 Visual Studio Developer PowerShell 中，从仓库目录执行：

```powershell
msbuild game.sln /m /p:Configuration=Debug /p:Platform=x64
./bin/x64/Debug/game.exe --self-test
msbuild game.sln /m /p:Configuration=Release /p:Platform=x64
./bin/x64/Release/game.exe --self-test
./bin/x64/Release/game.exe --smoke-test
./bin/x64/Release/game.exe --death-smoke-test
```

`--self-test` 不创建窗口，检查物理和完整无死亡通关路线，同时覆盖 900 分总分、重复计分拦截、死亡冻结、结算快照、重生保分、重开清零、自定义障碍分值、从尖刺下方经过不计分，以及结算按钮命中区域。失败返回非零退出码。

`--smoke-test` 创建实际 EasyX 窗口，绘制三帧，保存当前工作目录下的 `smoke-test.png`，随后退出。生成物和 VS 本地状态已通过 `.gitignore` 排除。

`--death-smoke-test` 自动经过第一组障碍获得 350 分，再触发死亡结算，生成 `death-smoke-test.png` 后退出，用于检查实际结算画面。
