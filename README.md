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

触碰尖刺或掉出画面后，游戏冻结并显示 Minecraft 风格的死亡结算界面：暗红色场景、死亡原因、黄色分数和灰色立体按钮。可以用鼠标点击“重生”或“重新开始”，也可以使用对应快捷键。绿色旗帜激活存档点，地图最右侧金色门是终点。失去窗口焦点时自动停止模拟。存档仅在本次运行中有效。

## 扩展地图

地图宽度由 960 扩大到 2240 像素，共 10 个平台（原为 5 个），包含上升、下降和连续跨越路线。窗口仍为 960×640，深色网格背景保持原样；镜头水平跟随角色，平台、尖刺、旗帜和终点一起滚动，顶部信息与死亡界面固定在屏幕上。镜头在地图两端停止，重生后立即回到出生点或存档点的视野。

`Level::tutorial()` 配置平台、尖刺、分值、终点和地图宽度；`World::cameraX()` 计算镜头偏移。人物边界按地图宽度计算，不再被第一屏右侧挡住。

## 积分规则

- 八个尖刺各 100 分：角色完整向右越过尖刺，并位于其上方才计分。分组控制活动，每枚尖刺仍独立计分。
- 九段平台跨越分别为 150、200、250、200、250、300、300、300、350 分：安全落到对应平台时计分。
- 一局最多 3100 分，同一障碍每局只计一次，往返或死亡重生不会刷分。
- 死亡界面显示死亡时的本局累计积分；重生保留积分、已通过障碍记录和存档点。
- “重新开始”或 Shift+R 清空积分、障碍记录、死亡次数和存档点，可以重新获得分数。
- 计分在碰撞与死亡判断之后执行，死亡当帧不会额外加分。

计分与死亡界面实现集中在 `src/system.cpp`，声明位于 `src/system.h`。障碍分值在 `Level::tutorial()` 的 `spikeScores` 和 `platformScores` 中配置，分别按尖刺和平台索引对应；尖刺未配置时默认 100 分，平台未配置时默认不计分。当前关卡几何在一局中保持固定；替换关卡时应重置积分系统。

## 活动尖刺

`src/SpikeActivity.cpp` 专门处理尖刺活动，`SpikeActivity.h` 定义配置和状态。地图现在有 4 组双尖刺；相邻尖刺自动组成一组，共用伸缩进度、阶段及计时器。

- 底座高度相差不超过 1 像素、水平间隙不超过 4 像素的尖刺自动连为一组，支持多枚连续排列及无序存储；不同高度或相隔较远的尖刺分组独立。
- 角色边缘靠近组内任意尖刺至 52 像素内、且处于尖刺所在区域时，整组同步触发；从平台下方或远处上层经过不会触发。
- 整组在 0.08 秒内从 24 像素向上伸长至 148 像素，底座位置不动。
- 伸长状态保持 0.75 秒，再用 0.35 秒缩回；每条命只触发一次，重生或重开后复位。
- 碰撞使用与绘制相同的动态尺寸。伸长部分会致命，死亡时尖刺动画随游戏一起冻结。
- 可以在安全距离引诱尖刺触发，等待缩回后再跳过；存档点重生位置不会被伸长尖刺覆盖。

`SpikeActivity::tutorial(level.spikes)` 根据当前关卡自动分组，每个 `SpikeTrap::indices` 保存组内尖刺编号。在 `SpikeActivity.h` 中可以调整默认触发距离、伸长高度、伸长时间、停留时间及缩回时间。需要更换关卡尖刺布局时，重新调用分组函数；只重生时使用 `reset()` 复位整组状态。积分按障碍编号计算，不会因为伸缩重复加分。

## 代码结构

```text
game.sln / game.vcxproj  Visual Studio 解决方案和项目配置
src/main.cpp            程序入口和验证参数
src/Game.h / Game.cpp    EasyX 窗口、键盘输入、固定步长循环、绘制
src/World.h / World.cpp  角色、关卡数据、重力、碰撞、重生与通关规则
src/system.h / system.cpp 积分、防重复计分、死亡结算界面及按钮命中判断
src/SpikeActivity.h / SpikeActivity.cpp 靠近触发的尖刺伸长、停留、缩回及复位
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
./bin/x64/Release/game.exe --spike-smoke-test
./bin/x64/Release/game.exe --map-smoke-test
```

`--self-test` 不创建窗口，检查扩展地图的完整无死亡通关路线，同时覆盖 3100 分总分、重复计分拦截、死亡冻结、结算快照、重生保分、重开清零、自定义障碍分值、从尖刺下方经过不计分，以及结算按钮命中区域。还验证镜头左右边界、重生镜头复位、组内同步、不同组独立，以及无序尖刺链的自动分组。失败返回非零退出码。

`--smoke-test` 创建实际 EasyX 窗口，绘制三帧，保存当前工作目录下的 `smoke-test.png`，随后退出。生成物和 VS 本地状态已通过 `.gitignore` 排除。

`--death-smoke-test` 自动经过第一组障碍获得 350 分，再触发死亡结算，生成 `death-smoke-test.png` 后退出，用于检查实际结算画面。

`--spike-smoke-test` 自动靠近第一组双尖刺并等待整组伸长，生成 `spike-smoke-test.png` 后退出。自动测试也覆盖触发范围、从下方经过不触发、底座固定、远处组不变、伸长碰撞、死亡冻结、缩回、重生复位及存档点安全性；完整通关路线会先引诱机关并等待缩回。

`--map-smoke-test` 在扩展地图后半段设置一个预览位置，触发附近尖刺组，生成 `map-smoke-test.png` 后退出，用于检查滚动后的平台、尖刺和终点绘制。
