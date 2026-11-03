# JubilWanna 来源与许可

本项目参考并适配 [Firefly26710773/JubilWanna](https://github.com/Firefly26710773/JubilWanna)，参考提交：`e2b0ff1948243e9f9c420fd73bd395d8537921b0`。

- `src/CharacterRenderer.cpp`：由上游 `src/Player.cpp` 的 `drawJubi` 小鸟造型与动画设计改写为 EasyX，沿用蓝披风、圆鸟、呆毛和星形胸饰，并适配本项目的角色尺寸与配色。
- `src/AdventureMechanics.cpp`：参考 `Player.cpp` 的离边容错、跳跃缓冲、松键截断，及 `Level.cpp` 的平滑往返平台、单向碰撞、弹簧、预警落刺和星羽探索设计，按本项目的 World/积分/复活接口重新实现。
- `src/SceneRenderer.cpp`：参考程序化主题材质、云层和微粒的视觉层次，保留本项目的五关主题。
- 新高路是针对本项目 960×640 逻辑画布与横向地图重新设计，没有直接复制上游六间 1280×720 房间，也没有替换本项目的 VS/EasyX 框架。

上游代码及原创角色按以下 MIT 许可使用；保留原作者版权声明。本次未引入上游音频、存档或第三方资源。

```text
MIT License

Copyright (c) 2026 JubiIWanna contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```



## 当前角色外观

当前版本的鲸鱼娘和碗装米饭像素图集根据用户提供的角色参考图及确认预览制作，不来自 JubilWanna。上游 MIT 声明继续适用于保留的适配代码，不代表其覆盖用户参考图中的角色设计。

## AI 娘新版角色

Claude、GPT、Grok 的新版像素图集依据用户确认的预览制作，角色设定参考 ZipZipPipe 的 AI 娘系列，非厂商官方形象。参考来源：[Bilibili 相关作品及作者署名](https://www.bilibili.com/video/BV1Hfet6KEug/)、[角色图鉴](https://wulusai.net/AI/AI%E5%A8%98%E6%A1%A3%E6%A1%88)。角色设定权利归原作者；本项目代码的许可不代表授予这些角色设定的独立许可。
