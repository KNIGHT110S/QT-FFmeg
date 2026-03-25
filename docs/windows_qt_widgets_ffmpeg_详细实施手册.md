# Windows 下 Qt Widgets + MSVC + 手动集成 FFmpeg 详细实施手册

## 1. 目标与范围

本文档针对下面这套固定方案：

- 操作系统：`Windows 10/11 x64`
- Qt：`Qt 6`
- UI：`Qt Widgets`
- 编译器：`MSVC 2022 x64`
- 构建工具：`CMake`
- FFmpeg 接入方式：`手动集成`
- 项目范围：`本地文件播放器`

这份文档不是讲“所有可能方案”，而是讲“你现在最适合先跑通的一套方案”。

最终阶段目标按顺序是：

1. 播放无音频视频
2. 播放有音频视频
3. 加倍速
4. 加音量调节

## 2. 为什么选这套组合

你已经装了 Qt 的两个套件，但现在最重要的不是“都能不能用”，而是先收敛。

建议优先使用：

- Qt 套件：`MSVC 2022 64-bit`
- Qt 界面技术：`Widgets`
- FFmpeg：手动集成到项目目录

原因：

- `Widgets` 比 `QML` 更适合先把底层解码播放链路跑通
- `MSVC` 和 Windows 桌面环境配合更稳，调试器体验也更成熟
- 手动集成 FFmpeg 对初学阶段最直观，便于理解 `include / lib / bin`

请注意：

- 既然决定用 `MSVC`，就不要再切到 `MinGW` 套件
- FFmpeg 的头文件、`.lib`、`.dll` 必须和 `x64 + MSVC` 匹配
- `MinGW` 版 Qt 不能直接链接 `MSVC` 版 FFmpeg 库

## 3. 你现在需要准备的东西

## 3.1 Qt

你已经装好了 Qt，但需要实际确认你会使用的是哪一套。

你后面在 Qt Creator 新建或打开项目时，构建套件请选择：

- `Desktop Qt 6.x MSVC2022 64bit`

不要选：

- `Desktop Qt 6.x MinGW`

## 3.2 Visual Studio / MSVC

如果你没装完整的 C++ 编译工具，需要补装：

- `Visual Studio 2022`
或
- `Build Tools for Visual Studio 2022`

安装时至少勾选：

- `Desktop development with C++`
- `MSVC v143`
- `Windows 10/11 SDK`
- `C++ CMake tools for Windows`

安装后你最好能在 “x64 Native Tools Command Prompt for VS 2022” 中执行：

```bat
cl
cmake --version
```

如果 `cl` 能输出版本信息，说明 MSVC 基本就绪。

## 3.3 FFmpeg

这里必须说明一个关键点：

- FFmpeg 官方下载页主要提供的是源码
- Windows 下你通常需要“已经编译好的二进制包”

也就是说，虽然你在用 FFmpeg 官方 API，但 Windows 下拿到的现成 `include / lib / dll` 往往来自第三方构建分发。这一点是从 FFmpeg 官方下载页的说明推断出来的。

你需要准备一套适用于：

- `Windows x64`
- `MSVC`

的 FFmpeg 二进制包。

你最终需要拿到三类东西：

### 1. 头文件

放到：

```text
third_party/ffmpeg/include
```

里面应有类似目录：

```text
libavcodec
libavformat
libavutil
libswscale
libswresample
```

### 2. 链接库

放到：

```text
third_party/ffmpeg/lib
```

里面至少应有：

- `avcodec.lib`
- `avformat.lib`
- `avutil.lib`
- `swscale.lib`

后面做音频还要：

- `swresample.lib`

### 3. 运行时动态库

放到：

```text
third_party/ffmpeg/bin
```

里面至少应有：

- `avcodec-xxx.dll`
- `avformat-xxx.dll`
- `avutil-xxx.dll`
- `swscale-xxx.dll`

后面做音频还要：

- `swresample-xxx.dll`

## 4. 推荐的项目目录结构

先把目录建好，后面你写代码会顺很多。

建议使用：

```text
QT-FFmeg-player/
  CMakeLists.txt
  docs/
  samples/
  src/
    main.cpp
    mainwindow.h
    mainwindow.cpp
    mainwindow.ui
    player/
      playercontroller.h
      playercontroller.cpp
      videorenderwidget.h
      videorenderwidget.cpp
      ffmpeghelper.h
      ffmpeghelper.cpp
      videoframe.h
      packetqueue.h
      packetqueue.cpp
      framequeue.h
      framequeue.cpp
      demuxthread.h
      demuxthread.cpp
      videodecoderthread.h
      videodecoderthread.cpp
    audio/
      audiodecoderthread.h
      audiodecoderthread.cpp
      audiooutputdevice.h
      audiooutputdevice.cpp
      avsyncclock.h
      avsyncclock.cpp
  third_party/
    ffmpeg/
      include/
      lib/
      bin/
```

注意：

- `audio/` 目录可以先建，但第一阶段可以先不实现
- `samples/` 放测试视频，不要和源码混在一起

## 5. 第一步先做什么

先不要写播放器逻辑，先做“空工程 + 链接 FFmpeg + 能运行”。

顺序一定要这样：

1. 创建一个最小 Qt Widgets 工程
2. 用 CMake 跑通
3. 把 FFmpeg 接入工程
4. 写一个最小测试，验证 FFmpeg 头文件和库可以使用
5. 再开始写播放器结构

如果你跳过第 4 步，后面出错时你很难判断是：

- Qt 工程问题
- CMake 配置问题
- FFmpeg 路径问题
- DLL 部署问题

## 6. Qt Widgets 最小工程建议

第一版建议创建一个 `QMainWindow` 项目，界面上只保留最小控件：

- 一个视频显示区
- 一个“打开文件”按钮
- 一个“播放/暂停”按钮
- 一个“停止”按钮
- 一个状态栏

第一阶段不要一开始就做这些：

- 进度条拖动
- 播放列表
- 全屏
- 截图
- 字幕
- 网络流

## 7. 第一版界面草图

建议界面结构如下：

```text
+--------------------------------------------------------+
| Menu / Toolbar                                         |
+--------------------------------------------------------+
|                                                        |
|                  VideoRenderWidget                     |
|                                                        |
|                                                        |
+--------------------------------------------------------+
| [Open] [Play/Pause] [Stop]                             |
+--------------------------------------------------------+
| Status: idle                                           |
+--------------------------------------------------------+
```

这样做的好处是：

- UI 简单
- 播放链路一旦打通，马上能看到结果
- 后面加倍速、音量时也容易扩展

## 8. CMake 的推荐写法

下面给你的是“适合当前阶段”的 CMake 模板思路。

顶层 `CMakeLists.txt` 建议长这样：

```cmake
cmake_minimum_required(VERSION 3.16)

project(QtFfmpegPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

set(FFMPEG_ROOT ${CMAKE_SOURCE_DIR}/third_party/ffmpeg)
set(FFMPEG_INCLUDE_DIR ${FFMPEG_ROOT}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_ROOT}/lib)

qt_add_executable(QtFfmpegPlayer
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/mainwindow.ui
)

target_include_directories(QtFfmpegPlayer PRIVATE
    ${FFMPEG_INCLUDE_DIR}
)

target_link_directories(QtFfmpegPlayer PRIVATE
    ${FFMPEG_LIB_DIR}
)

target_link_libraries(QtFfmpegPlayer PRIVATE
    Qt6::Widgets
    avformat
    avcodec
    avutil
    swscale
)

set_target_properties(QtFfmpegPlayer PROPERTIES
    WIN32_EXECUTABLE ON
)
```

第二阶段做音频时，再加：

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)
```

以及：

```cmake
target_link_libraries(QtFfmpegPlayer PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    avformat
    avcodec
    avutil
    swscale
    swresample
)
```

## 9. 第一阶段先做一个 FFmpeg 连通性测试

在真正写播放器之前，建议先写一个非常小的测试函数，验证：

- 头文件能找到
- `.lib` 能链接
- 程序运行时 `.dll` 能加载

测试逻辑可以很简单：

- 调用一次 FFmpeg 的版本函数
- 把版本号打印到调试输出或状态栏

例如测试方向：

- `av_version_info()`

如果这一步失败，不要继续写播放器，先把环境修好。

## 10. 如何判断 FFmpeg 环境是否接好了

### 编译阶段常见错误

#### 错误 1：头文件找不到

例如：

```text
fatal error: libavformat/avformat.h: No such file or directory
```

说明：

- `include` 路径没配置好

#### 错误 2：链接失败

例如：

```text
unresolved external symbol ...
```

说明：

- `.lib` 没加对
- 使用了错误架构
- 使用了错误编译器版本的库

### 运行阶段常见错误

#### 错误 3：程序启动时报缺少 DLL

说明：

- `third_party/ffmpeg/bin` 下的 DLL 没有复制到 exe 同目录
- 或者系统 `PATH` 没有包含这些 DLL 目录

### 最稳妥的做法

每次构建出 exe 后，把下面两部分都部署到 exe 目录：

1. Qt 依赖
2. FFmpeg 的 DLL

## 11. Qt 运行库部署

Windows 下，Qt 程序通常不能只拷贝一个 exe 就运行。

你需要用 `windeployqt`。

典型思路：

1. 先编译出 `QtFfmpegPlayer.exe`
2. 打开 Qt 对应套件的命令行环境
3. 执行：

```bat
windeployqt path\to\QtFfmpegPlayer.exe
```

然后把 FFmpeg 的 DLL 也复制进去。

注意：

- `windeployqt` 只负责 Qt 相关依赖
- 不会自动帮你把所有第三方 DLL 都处理完整
- FFmpeg DLL 你仍然需要自己确认

## 12. 从零开始的具体操作步骤

下面是一条你可以直接照着做的路径。

### 步骤 1：创建目录

在项目根目录建立：

```text
docs/
samples/
src/
third_party/ffmpeg/include
third_party/ffmpeg/lib
third_party/ffmpeg/bin
```

### 步骤 2：把 FFmpeg 文件放进去

把你准备好的 FFmpeg 文件拷进去：

- 头文件到 `include`
- `.lib` 到 `lib`
- `.dll` 到 `bin`

### 步骤 3：创建 Qt Widgets + CMake 工程

推荐在 Qt Creator 中：

1. 新建项目
2. 选择 `Qt Widgets Application`
3. 构建系统选择 `CMake`
4. Kit 选择 `MSVC 2022 64-bit`

### 步骤 4：确认能空工程编译运行

这一步只看一个结果：

- 不写 FFmpeg 逻辑，Qt 空窗口能正常运行

### 步骤 5：配置 CMake 接入 FFmpeg

把前面第 8 节的 CMake 思路接进去。

### 步骤 6：写一个 FFmpeg 版本测试

只验证：

- 工程能编译
- 程序能运行
- 能调到 FFmpeg API

### 步骤 7：部署 DLL

把 Qt 和 FFmpeg 的运行库都放到 exe 同目录，再运行。

### 步骤 8：开始写播放器骨架

只有走到这一步，才开始写：

- `PlayerController`
- `VideoRenderWidget`
- `DemuxThread`
- `VideoDecoderThread`

## 13. 第一阶段的项目骨架设计

你第一版只需要 4 个核心对象。

## 13.1 `MainWindow`

职责：

- 负责 UI
- 响应按钮点击
- 把用户操作转发给 `PlayerController`

建议提供：

- 打开文件按钮槽函数
- 播放按钮槽函数
- 停止按钮槽函数
- 状态栏文本更新

## 13.2 `VideoRenderWidget`

职责：

- 保存最近一帧图像
- 在 `paintEvent()` 中绘制

第一版建议接口：

```cpp
void setFrame(const QImage& image);
```

内部逻辑：

- 收到新图像
- 保存一份副本
- 调用 `update()`
- 在 `paintEvent()` 中按窗口比例绘制

注意：

- 最初版本先不考虑复杂缩放策略
- 先保证显示正确即可

## 13.3 `PlayerController`

职责：

- 对外提供统一控制接口
- 管理播放器状态
- 管理解封装和解码线程

第一版建议接口：

```cpp
bool openFile(const QString& filePath);
void play();
void pause();
void stop();
bool isPlaying() const;
```

第一版状态建议：

- `Idle`
- `Opened`
- `Playing`
- `Paused`
- `Stopped`

## 13.4 `DemuxThread`

职责：

- 打开媒体文件
- 找视频流
- 循环读取 `AVPacket`
- 把视频包放进视频包队列

第一版只处理视频流，不处理音频流。

## 13.5 `VideoDecoderThread`

职责：

- 从视频包队列取包
- 调用 FFmpeg 解码
- 把 `AVFrame` 转成 `QImage`
- 发信号给主线程刷新显示

第一版建议信号：

```cpp
void frameReady(const QImage& image, double ptsSeconds);
```

## 14. 第一阶段的数据流

第一版数据流建议严格保持单向：

```text
MainWindow
  -> PlayerController
      -> DemuxThread
          -> PacketQueue
              -> VideoDecoderThread
                  -> QImage
                      -> VideoRenderWidget
```

这样做的价值在于：

- 模块职责清楚
- 出问题时容易定位
- 后面加音频链路时也容易扩展

## 15. 第一阶段必须先简化哪些问题

为了让项目先跑起来，你必须主动“降复杂度”。

第一阶段只保留下面这些：

- 单文件播放
- 单视频流
- 只做软件解码
- 只做 RGB 转换后显示
- 不支持 seek
- 不支持拖动进度条
- 不支持倍速
- 不支持音量

如果你第一阶段就做 seek、暂停、音视频同步、倍速，复杂度会急剧上升。

## 16. 第一阶段的视频显示实现建议

建议路线：

1. FFmpeg 解码拿到 `AVFrame`
2. 读取其像素格式、宽高
3. 使用 `sws_scale` 转成 `AV_PIX_FMT_BGRA` 或与 `QImage` 匹配的格式
4. 构造 `QImage`
5. 在主线程显示

这里的重点不是“最高性能”，而是“先保证颜色正确、生命周期正确”。

### 特别注意

不要长期让 `QImage` 直接引用 FFmpeg 临时缓冲区。

更稳的做法是：

- 创建自己的图像缓冲
- 或者复制成 Qt 自己持有的图像数据

否则很容易出现：

- 花屏
- 偶发崩溃
- 已释放内存被继续访问

## 17. 第一阶段的时间控制

只要你开始播放视频，就一定会碰到“显示节奏”问题。

如果你没有时间控制，程序会：

- 以 CPU 最大速度解码
- 画面瞬间播放完成

第一阶段建议的简化方案：

1. 从视频帧拿 `pts`
2. 用 `time_base` 转成秒
3. 计算当前帧相对起始帧应显示的时间
4. 解码线程或调度层在适当时刻发出帧

核心公式：

```text
pts_seconds = pts * av_q2d(stream->time_base)
```

如果某些文件时间戳不稳定，你可以暂时退化到按帧率估算间隔，但这只是兜底方案。

## 18. 第一阶段推荐的测试视频

你至少准备下面几类：

### 1. 无音频 MP4

用途：

- 验证第一阶段核心播放链路

### 2. 带音频 MP4

用途：

- 第二阶段使用

### 3. 低分辨率视频

例如：

- `640x360`

用途：

- 先排除性能问题

### 4. 高分辨率视频

例如：

- `1920x1080`

用途：

- 初步观察渲染性能和 CPU 占用

## 19. 你每完成一步应该怎么验收

不要用“感觉差不多”验收，要用明确结果验收。

### 验收 1：Qt 空工程

成功标准：

- 可以编译
- 可以启动
- 有主窗口

### 验收 2：FFmpeg 接入

成功标准：

- 头文件能引用
- 能调用版本函数
- 程序运行时不报缺少 FFmpeg DLL

### 验收 3：媒体文件打开

成功标准：

- 可以选择本地文件
- 能打开文件
- 能找到视频流
- 能打印视频基本信息

### 验收 4：视频解码

成功标准：

- 能成功收到视频帧
- 解码循环不崩溃

### 验收 5：视频显示

成功标准：

- 能在界面看到连续画面
- 颜色基本正常

### 验收 6：播放节奏

成功标准：

- 画面不会一瞬间播完
- 速度看起来基本正常

## 20. 第二阶段实现音频时的新增内容

当你第一阶段稳定后，再做下面这些。

新增依赖：

- `Qt6::Multimedia`
- `swresample`

新增模块：

- `AudioDecoderThread`
- `AudioOutputDevice`
- `AVSyncClock`

新增职责：

- 找音频流
- 解码音频
- 重采样成统一 PCM 格式
- 交给 `QAudioSink`
- 以音频为主时钟做同步

## 21. 倍速为什么要放在第三阶段

倍速看起来像个 UI 功能，实际上不是。

它会影响：

- 视频帧显示间隔
- 音频播放速度
- 音视频同步逻辑

所以如果你在音频链路没稳定之前就做倍速，后面很可能要重写。

建议路线：

### 无音频倍速

先只改视频帧等待时间。

### 有音频倍速

先做“简单版”：

- 允许音调变化
- 先把速度变化做出来

不要一上来就做保音调高质量时伸。

## 22. 音量为什么最后做

音量控制功能本身不复杂，但它依赖一个已经稳定的音频输出链路。

所以正确顺序是：

1. 先能正常播出声音
2. 再加音量接口
3. 再加静音

否则你会把“没声音”与“音量实现问题”混在一起排查。

## 23. 常见错误与解决思路

## 23.1 Qt 程序能编译但启动不了

优先排查：

- 是否缺少 Qt DLL
- 是否执行了 `windeployqt`
- 是否选错了 Debug/Release 对应运行库

## 23.2 FFmpeg 头文件能找到但链接失败

优先排查：

- `.lib` 是否确实存在
- `.lib` 是否是 x64
- `.lib` 是否适用于 MSVC
- 库名是否和 CMake 中写的一致

## 23.3 程序能启动但一打开视频就崩溃

优先排查：

- `AVFormatContext` 是否初始化成功
- `AVCodecContext` 是否为空
- `AVFrame` / `AVPacket` 是否重复释放
- 解码线程是否跨线程访问 UI

## 23.4 有画面但颜色不对

优先排查：

- `sws_scale` 输出像素格式
- `QImage::Format` 是否匹配

## 23.5 有帧但播放过快

优先排查：

- 是否按 `pts` 控制显示时间
- 是否只是单纯循环解码然后立即刷新

## 24. 当前阶段你最应该避免的事

以下做法会极大增加难度，建议暂时避免：

- 一开始就用 `QOpenGLWidget`
- 一开始就做硬件解码
- 一开始就做进度条拖动
- 一开始就做网络流
- 一开始就支持多种复杂容器和字幕
- 在 `DemuxThread` 里直接刷新 UI
- 在没有状态机的情况下堆业务逻辑

## 25. 一条最稳的推进路径

如果你想尽量降低失败率，按这条路线走：

1. 选定 `MSVC 2022 x64`
2. 准备好 `third_party/ffmpeg/include/lib/bin`
3. 建立最小 Qt Widgets + CMake 工程
4. 跑通空窗口
5. 跑通 FFmpeg 版本测试
6. 写 `VideoRenderWidget`
7. 写 `PlayerController`
8. 打通打开文件和读取视频流
9. 打通视频解码
10. 打通视频显示
11. 加入视频播放节奏控制
12. 稳定后再做音频

## 26. 你下一步应该立刻做什么

你现在最应该做的，不是“继续想功能”，而是把环境落地。

建议你按这个清单执行：

### 必做清单

1. 确认 Qt Creator 中默认使用的是 `MSVC 2022 64-bit` Kit
2. 准备一套 `x64 + MSVC` 的 FFmpeg 二进制
3. 按本文档建立 `third_party/ffmpeg/include/lib/bin`
4. 创建 Qt Widgets + CMake 空工程
5. 跑通空工程
6. 再做 FFmpeg 连通性测试

### 完成后你就可以找我要的下一份内容

等你做到上面第 6 步，我下一步最适合继续给你的内容是：

- 一份完整可用的 `CMakeLists.txt`
- 一份第一版 `MainWindow + VideoRenderWidget + PlayerController` 的代码骨架
- 一份“无音频视频播放”阶段的详细类设计

## 27. 官方文档参考

下面这些是本文档设计时参考的官方资料：

- Qt Windows 平台支持说明
- Qt CMake 入门
- Qt Windows 部署工具 `windeployqt`
- Qt `QAudioSink`
- FFmpeg 下载页
- FFmpeg `demuxing_decoding` 官方示例

如果你下一步要我继续，我建议直接进入：

- “给我生成第一版工程骨架”

这样我就可以直接开始给你写项目文件和代码模板。
