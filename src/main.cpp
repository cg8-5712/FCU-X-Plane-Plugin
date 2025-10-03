#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"
#include "XPLMMenus.h"

#include <string>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// ToLiss FCU DataRef 定义
XPLMDataRef gSPD = nullptr;
XPLMDataRef gHDG = nullptr;
XPLMDataRef gALT = nullptr;
XPLMDataRef gVS  = nullptr;
XPLMDataRef gAP1 = nullptr;
XPLMDataRef gAP2 = nullptr;
XPLMDataRef gFPA = nullptr;

// FCU 模式切换
XPLMDataRef gHDGTRKMode = nullptr;  // 0=HDG/VS, 1=TRK/FPA
XPLMDataRef gMachMode   = nullptr;  // 0=SPD, 1=MACH

// Airbus FBW 自动管理模式
XPLMDataRef gSPDmanaged = nullptr;  // 0=手动, 1=自动
XPLMDataRef gHDGmanaged = nullptr;  // 0=手动, 1=自动
XPLMDataRef gAPVerticalMode = nullptr;  // 1=CLB, 101=OP CLB, 107=VS

XPLMWindowID gWindow = nullptr;
XPLMMenuID gMenuID = nullptr;
int gMenuItemIdx = -1;

// 串口相关
#ifdef _WIN32
HANDLE gSerialHandle = INVALID_HANDLE_VALUE;
#endif
std::string gSerialPortName = "";
std::string gSerialStatus = "Disconnected";
std::vector<std::string> gAvailablePorts;

// 串口函数
#ifdef _WIN32
std::vector<std::string> EnumerateSerialPorts()
{
    std::vector<std::string> ports;
    for (int i = 1; i <= 256; i++) {
        std::string portName = "COM" + std::to_string(i);
        HANDLE hSerial = CreateFileA(
            portName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (hSerial != INVALID_HANDLE_VALUE) {
            ports.push_back(portName);
            CloseHandle(hSerial);
        }
    }
    return ports;
}

bool OpenSerialPort(const std::string& portName)
{
    if (gSerialHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(gSerialHandle);
        gSerialHandle = INVALID_HANDLE_VALUE;
    }

    gSerialHandle = CreateFileA(
        portName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (gSerialHandle == INVALID_HANDLE_VALUE) {
        gSerialStatus = "Failed to open " + portName;
        gSerialPortName = "";
        return false;
    }

    // 配置串口参数
    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(gSerialHandle, &dcbSerialParams)) {
        CloseHandle(gSerialHandle);
        gSerialHandle = INVALID_HANDLE_VALUE;
        gSerialStatus = "Failed to get comm state";
        return false;
    }

    dcbSerialParams.BaudRate = CBR_115200; // 115200 波特率
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(gSerialHandle, &dcbSerialParams)) {
        CloseHandle(gSerialHandle);
        gSerialHandle = INVALID_HANDLE_VALUE;
        gSerialStatus = "Failed to set comm state";
        return false;
    }

    // 设置超时参数
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    
    if (!SetCommTimeouts(gSerialHandle, &timeouts)) {
        CloseHandle(gSerialHandle);
        gSerialHandle = INVALID_HANDLE_VALUE;
        gSerialStatus = "Failed to set timeouts";
        return false;
    }

    gSerialPortName = portName;
    gSerialStatus = "Connected";
    return true;
}

void CloseSerialPort()
{
    if (gSerialHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(gSerialHandle);
        gSerialHandle = INVALID_HANDLE_VALUE;
    }
    gSerialStatus = "Disconnected";
    gSerialPortName = "";
}
#endif

// 菜单回调函数
void MenuHandlerCallback(void* inMenuRef, void* inItemRef)
{
    if (gWindow) {
        int isVisible = XPLMGetWindowIsVisible(gWindow);
        XPLMSetWindowIsVisible(gWindow, !isVisible);
    }
}

// 绘制函数
void DrawWindowCallback(XPLMWindowID inWindowID, void* inRefcon)
{
    int l, t, r, b;
    XPLMGetWindowGeometry(inWindowID, &l, &t, &r, &b);

    // 绘制背景
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    XPLMDrawTranslucentDarkBox(l, t, r, b);

    // 动态查找未找到的 DataRef（飞机加载后才注册）
    if (!gHDGTRKMode) gHDGTRKMode = XPLMFindDataRef("AirbusFBW/HDGTRKmode");
    if (!gAP1) gAP1 = XPLMFindDataRef("AirbusFBW/AP1Engage");
    if (!gAP2) gAP2 = XPLMFindDataRef("AirbusFBW/AP2Engage");
    if (!gFPA) gFPA = XPLMFindDataRef("AirbusFBW/FMA1b");
    if (!gSPDmanaged) gSPDmanaged = XPLMFindDataRef("AirbusFBW/SPDmanaged");
    if (!gHDGmanaged) gHDGmanaged = XPLMFindDataRef("AirbusFBW/HDGmanaged");
    if (!gAPVerticalMode) gAPVerticalMode = XPLMFindDataRef("AirbusFBW/APVerticalMode");

    // 读取 DataRef 值
    float spd = gSPD ? XPLMGetDataf(gSPD) : 0.0f;
    float hdg = gHDG ? XPLMGetDataf(gHDG) : 0.0f;
    float alt = gALT ? XPLMGetDataf(gALT) : 0.0f;
    float vs  = gVS  ? XPLMGetDataf(gVS)  : 0.0f;

    // 读取模式
    int hdgTrkMode = gHDGTRKMode ? XPLMGetDatai(gHDGTRKMode) : 0;
    int machMode   = gMachMode   ? XPLMGetDatai(gMachMode)   : 0;

    // 读取 AP 状态和 FPA
    int ap1 = gAP1 ? XPLMGetDatai(gAP1) : 0;
    int ap2 = gAP2 ? XPLMGetDatai(gAP2) : 0;

    // 读取 FPA 字符串
    float fpa = 0.0f;
    if (gFPA) {
        char fpaStr[64] = {0};
        int len = XPLMGetDatab(gFPA, fpaStr, 0, sizeof(fpaStr) - 1);
        if (len > 0) {
            fpaStr[len] = '\0';
            fpa = static_cast<float>(atof(fpaStr));
        }
    }

    // 读取自动管理模式
    int spdManaged = gSPDmanaged ? XPLMGetDatai(gSPDmanaged) : 0;
    int hdgManaged = gHDGmanaged ? XPLMGetDatai(gHDGmanaged) : 0;
    int apVerticalMode = gAPVerticalMode ? XPLMGetDatai(gAPVerticalMode) : 0;

    // 拼接显示字符串
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    oss << "========== ToLiss FCU ==========\n";

    // 速度显示
    if (spdManaged) {
        // 自动模式：显示 --- 和 ·
        oss << "·MACH: ---\n";
    } else if (machMode) {
        oss << " MACH: " << std::setprecision(3) << spd << "\n";
    } else {
        oss << " SPD:  " << std::setw(3) << static_cast<int>(spd) << " kts\n";
    }

    // 航向显示
    if (hdgManaged) {
        // 自动模式：显示 --- 和 ·
        oss << "·HDG:  --- deg\n";
    } else if (hdgTrkMode) {
        oss << " TRK:  " << std::setfill('0') << std::setw(3) << static_cast<int>(hdg)
            << std::setfill(' ') << " deg\n";
    } else {
        oss << " HDG:  " << std::setfill('0') << std::setw(3) << static_cast<int>(hdg)
            << std::setfill(' ') << " deg\n";
    }

    // 高度显示
    if (apVerticalMode == 1) {
        // CLB 模式：显示高度数值，带 ·
        oss << "·ALT:  " << std::setw(5) << static_cast<int>(alt) << " ft\n";
    } else if (apVerticalMode == 101) {
        // OP CLB 模式：不带 ·，显示 -----
        oss << " ALT:  ----- ft\n";
    } else {
        // 其他模式：正常显示
        oss << " ALT:  " << std::setw(5) << static_cast<int>(alt) << " ft\n";
    }

    // 垂直速度/FPA 显示
    if (apVerticalMode == 1 || apVerticalMode == 101) {
        // CLB 或 OP CLB 模式：显示 -----
        if (hdgTrkMode) {
            oss << " FPA:  ----- deg\n";
        } else {
            oss << " V/S:  ----- fpm\n";
        }
    } else if (apVerticalMode == 107) {
        // VS 模式：不带 ·，显示带符号的四位数，精确到百位
        int vsValue = static_cast<int>(vs);
        // 四舍五入到百位
        if (vsValue >= 0) {
            vsValue = (vsValue + 50) / 100 * 100;
        } else {
            vsValue = (vsValue - 50) / 100 * 100;
        }
        oss << " V/S:  " << (vsValue >= 0 ? "+" : "")
            << std::setfill('0') << std::setw(4) << abs(vsValue) << std::setfill(' ') << " fpm\n";
    } else {
        // 其他模式：正常显示
        if (hdgTrkMode) {
            // FPA显示，带符号
            oss << " FPA:  " << (fpa >= 0 ? "+" : "")
                << std::setprecision(1) << std::setw(4) << fpa << " deg\n";
        } else {
            // V/S精确到百位，显示为带符号的4位数
            int vsValue = static_cast<int>(vs);
            if (vsValue >= 0) {
                vsValue = (vsValue + 50) / 100 * 100;
            } else {
                vsValue = (vsValue - 50) / 100 * 100;
            }
            oss << " V/S:  " << (vsValue >= 0 ? "+" : "")
                << std::setfill('0') << std::setw(4) << abs(vsValue) << std::setfill(' ') << " fpm\n";
        }
    }

    oss << "--------------------------------\n";
    oss << "Mode: " << (hdgTrkMode ? "TRK/FPA" : "HDG/VS ") << " | ";
    oss << (machMode ? "MACH" : "SPD ") << "\n";
    oss << "AP1: " << (ap1 ? "ON " : "OFF") << "  |  ";
    oss << "AP2: " << (ap2 ? "ON " : "OFF") << "\n";
    oss << "================================";

    // 添加串口状态信息
    oss << "\n\n======== Serial Port ==========\n";
    if (gSerialPortName.empty()) {
        oss << "Port: None\n";
    } else {
        oss << "Port: " << gSerialPortName << "\n";
    }
    oss << "Status: " << gSerialStatus << "\n";
    oss << "================================";

    std::string text = oss.str();

    // 绘制文本
    float white[3] = {1.0f, 1.0f, 1.0f};
    float green[3] = {0.0f, 1.0f, 0.0f};
    int lineHeight = 15;
    int y = t - 18;
    std::istringstream lines(text);
    std::string line;
    int lineNum = 0;

    while (std::getline(lines, line)) {
        float* color = white;

        // 标题和分隔线用绿色
        if (lineNum == 0 || line.find("===") != std::string::npos ||
            line.find("---") != std::string::npos) {
            color = green;
        }

        XPLMDrawString(color, l + 10, y, const_cast<char*>(line.c_str()), nullptr, xplmFont_Basic);
        y -= lineHeight;
        lineNum++;
    }
}

// 鼠标回调
int DummyMouse(XPLMWindowID, int, int, int, void*) { return 0; }

// 键盘回调
void DummyKey(XPLMWindowID, char, XPLMKeyFlags, char, void*, int) { }

// 鼠标光标回调
XPLMCursorStatus DummyCursor(XPLMWindowID, int, int, void*) { return xplm_CursorDefault; }

// 插件入口函数
PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc)
{
    strcpy(outName, "ToLissFCUMonitor");
    strcpy(outSig, "dzc.toliss.fcu.monitor");
    strcpy(outDesc, "Display ToLiss FCU Data in X-Plane 11/12.");

    // 获取 FCU 值 DataRefs
    gSPD = XPLMFindDataRef("sim/cockpit/autopilot/airspeed");
    gHDG = XPLMFindDataRef("sim/cockpit/autopilot/heading_mag");
    gALT = XPLMFindDataRef("sim/cockpit2/autopilot/altitude_dial_ft");
    gVS  = XPLMFindDataRef("sim/cockpit/autopilot/vertical_velocity");

    // 获取模式切换 DataRefs
    gHDGTRKMode = XPLMFindDataRef("AirbusFBW/HDGTRKmode");
    gMachMode   = XPLMFindDataRef("sim/cockpit/autopilot/airspeed_is_mach");

    // 获取 AP 和 FPA DataRefs
    gAP1 = XPLMFindDataRef("AirbusFBW/AP1Engage");
    gAP2 = XPLMFindDataRef("AirbusFBW/AP2Engage");
    gFPA = XPLMFindDataRef("AirbusFBW/FMA1b");

    // 获取 Airbus FBW 自动管理模式 DataRefs
    gSPDmanaged = XPLMFindDataRef("AirbusFBW/SPDmanaged");
    gHDGmanaged = XPLMFindDataRef("AirbusFBW/HDGmanaged");
    gAPVerticalMode = XPLMFindDataRef("AirbusFBW/APVerticalMode");

    // 初始化串口
#ifdef _WIN32
    gAvailablePorts = EnumerateSerialPorts();
    if (!gAvailablePorts.empty()) {
        if (gAvailablePorts.size() == 1) {
            // 只有一个串口，自动连接
            if (OpenSerialPort(gAvailablePorts[0])) {
                // 连接成功
            }
        } else {
            // 多个串口，默认尝试第一个
            OpenSerialPort(gAvailablePorts[0]);
            // TODO: 将来可以添加用户选择功能
        }
    } else {
        gSerialStatus = "No serial ports found";
    }
#endif

    // 创建插件菜单
    gMenuItemIdx = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "FCU Display", nullptr, 0);
    gMenuID = XPLMCreateMenu("FCU Display", XPLMFindPluginsMenu(), gMenuItemIdx, MenuHandlerCallback, nullptr);
    XPLMAppendMenuItem(gMenuID, "Show/Hide UI", nullptr, 0);

    // 创建窗口
    XPLMCreateWindow_t params;
    memset(&params, 0, sizeof(params));
    params.structSize = sizeof(params);
    params.visible = 1;  // 初始可见
    params.drawWindowFunc = DrawWindowCallback;
    params.handleMouseClickFunc = DummyMouse;
    params.handleKeyFunc = DummyKey;
    params.handleCursorFunc = DummyCursor;
    params.handleMouseWheelFunc = nullptr;
    params.refcon = nullptr;
    params.left = 50;
    params.top = 600;
    params.right = 380;
    params.bottom = 360;  // 调整高度以容纳串口信息
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;

    gWindow = XPLMCreateWindowEx(&params);
    XPLMSetWindowPositioningMode(gWindow, xplm_WindowPositionFree, -1);
    XPLMSetWindowTitle(gWindow, "ToLiss FCU Monitor");

    return 1;
}

PLUGIN_API void XPluginStop(void)
{
    // 关闭串口
#ifdef _WIN32
    CloseSerialPort();
#endif

    // 销毁窗口
    if (gWindow) {
        XPLMDestroyWindow(gWindow);
        gWindow = nullptr;
    }

    // 销毁菜单
    if (gMenuID) {
        XPLMDestroyMenu(gMenuID);
        gMenuID = nullptr;
    }
}

PLUGIN_API int XPluginEnable(void)  { return 1; }
PLUGIN_API void XPluginDisable(void) { }
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int, void*) { }
