# ToLiss FCU Monitor

一个用于 X-Plane 11/12 的 ToLiss 飞机 FCU（Flight Control Unit）监控插件。

## 功能特性

- **实时显示 FCU 参数**
  - 速度 (SPD/MACH)
  - 航向 (HDG/TRK)
  - 高度 (ALT)
  - 垂直速度/飞行路径角 (V/S/FPA)

- **模式识别**
  - 自动识别 HDG/VS 和 TRK/FPA 模式
  - 自动识别 SPD 和 MACH 模式

- **按钮状态检测**
  - 实时检测 SPD、HDG、ALT、V/S 旋钮的按压/拔出状态
  - 按压状态使用 "·" 标记显示

- **自动驾驶状态**
  - 显示 AP1 和 AP2 的开关状态

## 系统要求

### 运行环境
- X-Plane 11 或 X-Plane 12
- ToLiss 飞机插件（A319/A321 等）
- Windows 64-bit（当前版本）

### 开发环境
- CMake 3.15 或更高版本
- Visual Studio 2017 或更高版本（含 C++ 工具集）
- X-Plane SDK 3.0.1 或更高版本

## 构建步骤

### 1. 配置 SDK 路径

编辑 `CMakeLists.txt`，确保 SDK 路径正确：

```cmake
set(XPLM_SDK_PATH "//f/bian/cppproject/X-Plane-plugins/X-plane-SDK")
```

### 2. 生成项目文件

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

### 3. 编译插件

```bash
cmake --build build --config Release
```

编译成功后，插件文件位于：
- `build/Release/win.xpl`
- `dist/Release/win.xpl`（自动复制）

## 安装说明

### 自动安装

如果在 CMakeLists.txt 中配置了目标路径，构建时会自动复制：

```cmake
set(DEPLOY_PATH "E:/software/X-Plane 12/Resources/plugins/ToLissFCUMonitor")
```

### 手动安装

1. 在 X-Plane 插件目录创建文件夹：
   ```
   X-Plane 12/Resources/plugins/ToLissFCUMonitor/64/
   ```

2. 复制 `win.xpl` 到以下位置：
   ```
   X-Plane 12/Resources/plugins/ToLissFCUMonitor/64/win.xpl
   ```

3. 重启 X-Plane

## 使用说明

1. 启动 X-Plane
2. 加载 ToLiss 飞机（如 A319、A321）
3. 插件窗口会自动显示在屏幕上
4. 窗口显示内容示例：

```
========== ToLiss FCU ==========
 SPD:  250 kts
 HDG:  090 deg
 ALT:  10000 ft
 V/S:   500 fpm
--------------------------------
Mode: HDG/VS  | SPD
AP1: ON   |  AP2: OFF
================================
```

### 按压状态指示

当旋钮处于按压状态时，对应行前面会显示 "·" 标记：

```
· SPD:  250 kts    <- SPD 旋钮被按下
  HDG:  090 deg
· ALT:  10000 ft   <- ALT 旋钮被按下
  V/S:   500 fpm
```

## 技术细节

### DataRef 映射

| 功能 | DataRef |
|-----|---------|
| 速度设定值 | `sim/cockpit/autopilot/airspeed` |
| 航向设定值 | `sim/cockpit/autopilot/heading_mag` |
| 高度设定值 | `sim/cockpit2/autopilot/altitude_dial_ft` |
| 垂直速度设定值 | `sim/cockpit/autopilot/vertical_velocity` |
| SPD 按压动画 | `ckpt/fcu/airspeedPush/anim` |
| HDG 按压动画 | `ckpt/fcu/headingPush/anim` |
| ALT 按压动画 | `ckpt/fcu/altitudePush/anim` |
| V/S 按压动画 | `ckpt/fcu/vviPush/anim` |
| HDG/TRK 模式 | `AirbusFBW/HDGTRKmode` |
| SPD/MACH 模式 | `sim/cockpit/autopilot/airspeed_is_mach` |
| AP1 状态 | `AirbusFBW/AP1Engage` |
| AP2 状态 | `AirbusFBW/AP2Engage` |
| FPA 值 | `AirbusFBW/FMA1b` |

### 按压检测逻辑

插件使用状态机检测旋钮的按压和拔出：

- **按压检测**：动画值从 0 → 正值 → 0
- **拔出检测**：按压状态 + 检测到正值

```cpp
struct PushState {
    float prevValue;
    bool wasPositive;
    bool isPushed;

    void update(float currentValue) {
        bool isPositive = currentValue > 0.1f;
        bool isZero = currentValue < 0.05f;

        // 检测按下：0 -> 正 -> 0
        if (prevValue < 0.05f && isPositive) {
            wasPositive = true;
        } else if (wasPositive && isZero) {
            isPushed = true;
            wasPositive = false;
        }

        // 检测拔出：按下状态 + 检测到正值
        if (isPushed && isPositive) {
            isPushed = false;
        }

        prevValue = currentValue;
    }
};
```

### 动态 DataRef 查找

ToLiss 飞机的某些 DataRef 在飞机加载后才注册，因此插件在每帧绘制时动态查找：

```cpp
if (!gSPDPush) gSPDPush = XPLMFindDataRef("ckpt/fcu/airspeedPush/anim");
if (!gHDGPush) gHDGPush = XPLMFindDataRef("ckpt/fcu/headingPush/anim");
// ...
```

## 项目结构

```
FCU/
├── CMakeLists.txt          # CMake 构建配置
├── src/
│   └── main.cpp            # 插件主代码
├── build/                  # CMake 构建目录
│   └── Release/
│       └── win.xpl         # 编译输出
└── dist/                   # 分发目录
    └── Release/
        └── win.xpl         # 最终插件文件
```

## 已知问题

1. 如果 X-Plane 正在运行，构建时的自动复制可能失败（文件被锁定）
2. 当前仅支持 Windows 平台，Mac 和 Linux 版本待开发

## 开发计划

- [ ] 添加 Mac 平台支持
- [ ] 添加 Linux 平台支持
- [ ] 添加窗口位置保存功能
- [ ] 添加自定义主题/颜色配置
- [ ] 添加更多 FCU 参数显示

## 许可证

本项目仅供学习和个人使用。

## 作者

dzc

## 致谢

- X-Plane SDK
- ToLiss Simulations
