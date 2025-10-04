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

#if IBM
#include <windows.h>
#include <GL/gl.h>
#elif LIN
#include <GL/gl.h>
#else
#include <OpenGL/gl.h>
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
XPLMMenuID gPortMenuID = nullptr;
int gPortMenuItemIdx = -1;

// 串口相关
#ifdef _WIN32
HANDLE gSerialHandle = INVALID_HANDLE_VALUE;
#endif
std::string gSerialPortName = "";
std::string gSerialStatus = "Disconnected";
std::vector<std::string> gAvailablePorts;
std::vector<std::string> gPortMenuRefs; // 保存菜单项引用字符串

// UI 控件状态
int gSelectedPortIndex = 0;  // 当前选择的串口索引
bool gShowDropdown = false;  // 是否显示下拉列表

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

// 前向声明
void BuildPortMenu();

// 定时刷新COM端口列表
float RefreshPortsCallback(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop,
                          int inCounter, void* inRefcon)
{
    // 刷新可用端口列表
    gAvailablePorts = EnumerateSerialPorts();

    // 更新菜单
    BuildPortMenu();

    // 根据是否有设备连接返回不同的刷新间隔
    if (gSerialHandle != INVALID_HANDLE_VALUE) {
        return 30.0f;  // 已连接：30秒刷新一次
    } else {
        return 10.0f;  // 未连接：10秒刷新一次
    }
}
#endif

// 构建串口菜单
void BuildPortMenu()
{
#ifdef _WIN32
    if (!gPortMenuID) return;

    // 清除所有菜单项
    XPLMClearAllMenuItems(gPortMenuID);

    // 清空并重建引用字符串
    gPortMenuRefs.clear();

    // 添加所有可用串口
    if (gAvailablePorts.empty()) {
        XPLMAppendMenuItem(gPortMenuID, "No ports available", (void*)"no_port", 0);
    } else {
        for (const auto& port : gAvailablePorts) {
            std::string portRef = "port:" + port;
            gPortMenuRefs.push_back(portRef);

            std::string menuLabel = port;
            if (port == gSerialPortName) {
                menuLabel += " (Connected)";
            }

            XPLMAppendMenuItem(gPortMenuID, menuLabel.c_str(),
                             (void*)gPortMenuRefs.back().c_str(), 0);
        }
    }
#endif
}

// 菜单回调函数
void MenuHandlerCallback(void* inMenuRef, void* inItemRef)
{
    if (!inItemRef) return;

    const char* itemRef = (const char*)inItemRef;

    if (strcmp(itemRef, "toggle_ui") == 0) {
        // 切换窗口显示/隐藏
        if (gWindow) {
            int isVisible = XPLMGetWindowIsVisible(gWindow);
            XPLMSetWindowIsVisible(gWindow, !isVisible);
        }
    }
    else if (strcmp(itemRef, "refresh_ports") == 0) {
        // 刷新串口列表
#ifdef _WIN32
        gAvailablePorts = EnumerateSerialPorts();
        BuildPortMenu();
        if (gAvailablePorts.empty()) {
            gSerialStatus = "No serial ports found";
        } else {
            gSerialStatus = std::to_string(gAvailablePorts.size()) + " port(s) found";
        }
#endif
    }
    else if (strncmp(itemRef, "port:", 5) == 0) {
        // 选择串口
#ifdef _WIN32
        std::string portName = itemRef + 5; // 跳过 "port:" 前缀
        if (OpenSerialPort(portName)) {
            // 连接成功
        }
#endif
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
    float yellow[3] = {1.0f, 1.0f, 0.0f};
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

    // 绘制串口选择UI
    int uiY = b + 30;  // UI控件的Y位置（往下移动避免重合）

    // 绘制下拉框背景
    int dropdownX = l + 10;
    int dropdownY = uiY;
    int dropdownWidth = 150;
    int dropdownHeight = 20;

    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2i(dropdownX, dropdownY);
    glVertex2i(dropdownX + dropdownWidth, dropdownY);
    glVertex2i(dropdownX + dropdownWidth, dropdownY + dropdownHeight);
    glVertex2i(dropdownX, dropdownY + dropdownHeight);
    glEnd();

    // 绘制下拉框边框
    glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(dropdownX, dropdownY);
    glVertex2i(dropdownX + dropdownWidth, dropdownY);
    glVertex2i(dropdownX + dropdownWidth, dropdownY + dropdownHeight);
    glVertex2i(dropdownX, dropdownY + dropdownHeight);
    glEnd();

    // 绘制下拉框文本
#ifdef _WIN32
    std::string dropdownText = "None";
    if (!gAvailablePorts.empty() && gSelectedPortIndex >= 0 &&
        gSelectedPortIndex < static_cast<int>(gAvailablePorts.size())) {
        dropdownText = gAvailablePorts[gSelectedPortIndex];
    } else if (!gAvailablePorts.empty()) {
        gSelectedPortIndex = 0;
        dropdownText = gAvailablePorts[0];
    }
#else
    std::string dropdownText = "N/A (Windows only)";
#endif

    XPLMDrawString(white, dropdownX + 5, dropdownY + 5,
                   const_cast<char*>(dropdownText.c_str()), nullptr, xplmFont_Basic);

    // 绘制下拉箭头
    XPLMDrawString(white, dropdownX + dropdownWidth - 15, dropdownY + 5,
                   const_cast<char*>("v"), nullptr, xplmFont_Basic);

    // 如果下拉列表展开，绘制列表项
#ifdef _WIN32
    if (gShowDropdown && !gAvailablePorts.empty()) {
        int itemHeight = 18;
        for (size_t i = 0; i < gAvailablePorts.size(); i++) {
            int itemY = dropdownY - (i + 1) * itemHeight;

            // 绘制项背景
            if (i == static_cast<size_t>(gSelectedPortIndex)) {
                glColor4f(0.3f, 0.3f, 0.5f, 1.0f);  // 高亮选中项
            } else {
                glColor4f(0.2f, 0.2f, 0.2f, 1.0f);
            }
            glBegin(GL_QUADS);
            glVertex2i(dropdownX, itemY);
            glVertex2i(dropdownX + dropdownWidth, itemY);
            glVertex2i(dropdownX + dropdownWidth, itemY + itemHeight);
            glVertex2i(dropdownX, itemY + itemHeight);
            glEnd();

            // 绘制项边框
            glColor4f(0.5f, 0.5f, 0.5f, 1.0f);
            glBegin(GL_LINE_LOOP);
            glVertex2i(dropdownX, itemY);
            glVertex2i(dropdownX + dropdownWidth, itemY);
            glVertex2i(dropdownX + dropdownWidth, itemY + itemHeight);
            glVertex2i(dropdownX, itemY + itemHeight);
            glEnd();

            // 绘制项文本
            XPLMDrawString(white, dropdownX + 5, itemY + 3,
                         const_cast<char*>(gAvailablePorts[i].c_str()),
                         nullptr, xplmFont_Basic);
        }
    }
#endif

    // 绘制连接/断开按钮
    int buttonX = dropdownX + dropdownWidth + 10;
    int buttonY = uiY;
    int buttonWidth = 80;
    int buttonHeight = 20;

    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);

    // 按钮背景 - 根据连接状态改变颜色
    if (gSerialHandle != INVALID_HANDLE_VALUE) {
        glColor4f(0.5f, 0.2f, 0.2f, 1.0f);  // 已连接 - 红色系
    } else {
        glColor4f(0.2f, 0.5f, 0.2f, 1.0f);  // 未连接 - 绿色系
    }
    glBegin(GL_QUADS);
    glVertex2i(buttonX, buttonY);
    glVertex2i(buttonX + buttonWidth, buttonY);
    glVertex2i(buttonX + buttonWidth, buttonY + buttonHeight);
    glVertex2i(buttonX, buttonY + buttonHeight);
    glEnd();

    // 按钮边框
    glColor4f(0.7f, 0.7f, 0.7f, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(buttonX, buttonY);
    glVertex2i(buttonX + buttonWidth, buttonY);
    glVertex2i(buttonX + buttonWidth, buttonY + buttonHeight);
    glVertex2i(buttonX, buttonY + buttonHeight);
    glEnd();

    // 按钮文本
#ifdef _WIN32
    const char* buttonText = (gSerialHandle != INVALID_HANDLE_VALUE) ? "Disconnect" : "Connect";
#else
    const char* buttonText = "N/A";
#endif
    XPLMDrawString(yellow, buttonX + 10, buttonY + 5,
                   const_cast<char*>(buttonText), nullptr, xplmFont_Basic);
}

// 鼠标回调
int DummyMouse(XPLMWindowID inWindowID, int x, int y, int isDown, void* inRefcon)
{
    if (!isDown) return 0;  // 只处理按下事件

    int l, t, r, b;
    XPLMGetWindowGeometry(inWindowID, &l, &t, &r, &b);

    // UI控件位置（与绘制时保持一致）
    int uiY = b + 30;
    int dropdownX = l + 10;
    int dropdownY = uiY;
    int dropdownWidth = 150;
    int dropdownHeight = 20;
    int buttonX = dropdownX + dropdownWidth + 10;
    int buttonY = uiY;
    int buttonWidth = 80;
    int buttonHeight = 20;

#ifdef _WIN32
    // 检查是否点击了下拉框
    if (x >= dropdownX && x <= dropdownX + dropdownWidth &&
        y >= dropdownY && y <= dropdownY + dropdownHeight) {
        // 刷新可用串口列表
        gAvailablePorts = EnumerateSerialPorts();
        // 切换下拉列表显示状态
        gShowDropdown = !gShowDropdown;
        return 1;
    }

    // 检查是否点击了下拉列表项
    if (gShowDropdown && !gAvailablePorts.empty()) {
        int itemHeight = 18;
        for (size_t i = 0; i < gAvailablePorts.size(); i++) {
            int itemY = dropdownY - (i + 1) * itemHeight;
            if (x >= dropdownX && x <= dropdownX + dropdownWidth &&
                y >= itemY && y <= itemY + itemHeight) {
                // 选择串口
                gSelectedPortIndex = static_cast<int>(i);
                gShowDropdown = false;
                return 1;
            }
        }
    }

    // 检查是否点击了连接/断开按钮
    if (x >= buttonX && x <= buttonX + buttonWidth &&
        y >= buttonY && y <= buttonY + buttonHeight) {
        if (gSerialHandle != INVALID_HANDLE_VALUE) {
            // 当前已连接，执行断开
            CloseSerialPort();
        } else {
            // 当前未连接，执行连接
            if (!gAvailablePorts.empty() && gSelectedPortIndex >= 0 &&
                gSelectedPortIndex < static_cast<int>(gAvailablePorts.size())) {
                OpenSerialPort(gAvailablePorts[gSelectedPortIndex]);
            } else {
                gSerialStatus = "No port selected";
            }
        }
        gShowDropdown = false;  // 关闭下拉列表
        return 1;
    }

    // 点击其他区域，关闭下拉列表
    if (gShowDropdown) {
        gShowDropdown = false;
        return 1;
    }
#endif

    return 0;
}

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

    // 添加菜单项
    XPLMAppendMenuItem(gMenuID, "Show/Hide UI", (void*)"toggle_ui", 0);
    XPLMAppendMenuSeparator(gMenuID);

    // 添加串口相关菜单
#ifdef _WIN32
    XPLMAppendMenuItem(gMenuID, "Refresh Ports", (void*)"refresh_ports", 0);

    // 创建串口选择子菜单
    gPortMenuItemIdx = XPLMAppendMenuItem(gMenuID, "Select Port", nullptr, 0);
    gPortMenuID = XPLMCreateMenu("Select Port", gMenuID, gPortMenuItemIdx, MenuHandlerCallback, nullptr);

    // 初始化串口菜单
    BuildPortMenu();
#endif

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
    params.bottom = 320;  // 调整高度以容纳串口信息和UI控件
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;

    gWindow = XPLMCreateWindowEx(&params);
    XPLMSetWindowPositioningMode(gWindow, xplm_WindowPositionFree, -1);
    XPLMSetWindowTitle(gWindow, "ToLiss FCU Monitor");

    // 注册定时刷新回调（仅Windows）
#ifdef _WIN32
    XPLMRegisterFlightLoopCallback(RefreshPortsCallback, 10.0f, nullptr);
#endif

    return 1;
}

PLUGIN_API void XPluginStop(void)
{
    // 注销定时刷新回调
#ifdef _WIN32
    XPLMUnregisterFlightLoopCallback(RefreshPortsCallback, nullptr);
#endif

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
#ifdef _WIN32
    if (gPortMenuID) {
        XPLMDestroyMenu(gPortMenuID);
        gPortMenuID = nullptr;
    }
#endif
    if (gMenuID) {
        XPLMDestroyMenu(gMenuID);
        gMenuID = nullptr;
    }
}

PLUGIN_API int XPluginEnable(void)  { return 1; }
PLUGIN_API void XPluginDisable(void) { }
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int, void*) { }
