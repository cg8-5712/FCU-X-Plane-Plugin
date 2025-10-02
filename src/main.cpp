#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include <string>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdlib>

// ToLiss FCU DataRef 定义
XPLMDataRef gSPD = nullptr;
XPLMDataRef gHDG = nullptr;
XPLMDataRef gALT = nullptr;
XPLMDataRef gVS  = nullptr;
XPLMDataRef gAP1 = nullptr;
XPLMDataRef gAP2 = nullptr;
XPLMDataRef gFPA = nullptr;

// FCU 按压状态
XPLMDataRef gSPDPush = nullptr;
XPLMDataRef gHDGPush = nullptr;
XPLMDataRef gALTPush = nullptr;
XPLMDataRef gVSPush  = nullptr;

// FCU 模式切换
XPLMDataRef gHDGTRKMode = nullptr;  // 0=HDG/VS, 1=TRK/FPA
XPLMDataRef gMachMode   = nullptr;  // 0=SPD, 1=MACH

XPLMWindowID gWindow = nullptr;

// 按压状态跟踪结构
struct PushState {
    float prevValue;
    bool wasPositive;
    bool isPushed;

    PushState() : prevValue(0.0f), wasPositive(false), isPushed(false) {}

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

// 各个旋钮的按压状态
PushState gSPDPushState;
PushState gHDGPushState;
PushState gALTPushState;
PushState gVSPushState;

// 绘制函数
void DrawWindowCallback(XPLMWindowID inWindowID, void* inRefcon)
{
    int l, t, r, b;
    XPLMGetWindowGeometry(inWindowID, &l, &t, &r, &b);

    // 绘制背景
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    XPLMDrawTranslucentDarkBox(l, t, r, b);

    // 动态查找未找到的 DataRef（飞机加载后才注册）
    if (!gSPDPush) gSPDPush = XPLMFindDataRef("ckpt/fcu/airspeedPush/anim");
    if (!gHDGPush) gHDGPush = XPLMFindDataRef("ckpt/fcu/headingPush/anim");
    if (!gALTPush) gALTPush = XPLMFindDataRef("ckpt/fcu/altitudePush/anim");
    if (!gVSPush)  gVSPush  = XPLMFindDataRef("ckpt/fcu/vviPush/anim");
    if (!gHDGTRKMode) gHDGTRKMode = XPLMFindDataRef("AirbusFBW/HDGTRKmode");
    if (!gAP1) gAP1 = XPLMFindDataRef("AirbusFBW/AP1Engage");
    if (!gAP2) gAP2 = XPLMFindDataRef("AirbusFBW/AP2Engage");
    if (!gFPA) gFPA = XPLMFindDataRef("AirbusFBW/FMA1b");

    // 读取 DataRef 值
    float spd = gSPD ? XPLMGetDataf(gSPD) : 0.0f;
    float hdg = gHDG ? XPLMGetDataf(gHDG) : 0.0f;
    float alt = gALT ? XPLMGetDataf(gALT) : 0.0f;
    float vs  = gVS  ? XPLMGetDataf(gVS)  : 0.0f;

    // 读取按压动画值并更新状态
    float spdPush = gSPDPush ? XPLMGetDataf(gSPDPush) : 0.0f;
    float hdgPush = gHDGPush ? XPLMGetDataf(gHDGPush) : 0.0f;
    float altPush = gALTPush ? XPLMGetDataf(gALTPush) : 0.0f;
    float vsPush  = gVSPush  ? XPLMGetDataf(gVSPush)  : 0.0f;

    gSPDPushState.update(spdPush);
    gHDGPushState.update(hdgPush);
    gALTPushState.update(altPush);
    gVSPushState.update(vsPush);

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

    // 拼接显示字符串
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    oss << "========== ToLiss FCU ==========\n";

    // 速度显示
    if (machMode) {
        oss << (gSPDPushState.isPushed ? "·" : " ")
            << "MACH: " << std::setprecision(3) << spd << "\n";
    } else {
        oss << (gSPDPushState.isPushed ? "·" : " ")
            << "SPD:  " << std::setw(3) << static_cast<int>(spd) << " kts\n";
    }

    // 航向显示
    if (hdgTrkMode) {
        oss << (gHDGPushState.isPushed ? "·" : " ")
            << "TRK:  " << std::setw(3) << static_cast<int>(hdg) << " deg\n";
    } else {
        oss << (gHDGPushState.isPushed ? "·" : " ")
            << "HDG:  " << std::setw(3) << static_cast<int>(hdg) << " deg\n";
    }

    // 高度显示
    oss << (gALTPushState.isPushed ? "·" : " ")
        << "ALT:  " << std::setw(5) << static_cast<int>(alt) << " ft\n";

    // 垂直速度/FPA 显示
    if (hdgTrkMode) {
        oss << (gVSPushState.isPushed ? "·" : " ")
            << "FPA:  " << std::setprecision(1) << std::setw(5) << fpa << " deg\n";
    } else {
        oss << (gVSPushState.isPushed ? "·" : " ")
            << "V/S:  " << std::setw(5) << static_cast<int>(vs) << " fpm\n";
    }

    oss << "--------------------------------\n";
    oss << "Mode: " << (hdgTrkMode ? "TRK/FPA" : "HDG/VS ") << " | ";
    oss << (machMode ? "MACH" : "SPD ") << "\n";
    oss << "AP1: " << (ap1 ? "ON " : "OFF") << "  |  ";
    oss << "AP2: " << (ap2 ? "ON " : "OFF") << "\n";
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

    // 获取按压状态 DataRefs
    gSPDPush = XPLMFindDataRef("ckpt/fcu/airspeedPush/anim");
    gHDGPush = XPLMFindDataRef("ckpt/fcu/headingPush/anim");
    gALTPush = XPLMFindDataRef("ckpt/fcu/altitudePush/anim");
    gVSPush  = XPLMFindDataRef("ckpt/fcu/vviPush/anim");

    // 获取模式切换 DataRefs
    gHDGTRKMode = XPLMFindDataRef("AirbusFBW/HDGTRKmode");
    gMachMode   = XPLMFindDataRef("sim/cockpit/autopilot/airspeed_is_mach");

    // 获取 AP 和 FPA DataRefs
    gAP1 = XPLMFindDataRef("AirbusFBW/AP1Engage");
    gAP2 = XPLMFindDataRef("AirbusFBW/AP2Engage");
    gFPA = XPLMFindDataRef("AirbusFBW/FMA1b");

    // 创建窗口
    XPLMCreateWindow_t params;
    memset(&params, 0, sizeof(params));
    params.structSize = sizeof(params);
    params.visible = 1;
    params.drawWindowFunc = DrawWindowCallback;
    params.handleMouseClickFunc = DummyMouse;
    params.handleKeyFunc = DummyKey;
    params.handleCursorFunc = DummyCursor;
    params.handleMouseWheelFunc = nullptr;
    params.refcon = nullptr;
    params.left = 50;
    params.top = 600;
    params.right = 380;
    params.bottom = 420;
    params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;

    gWindow = XPLMCreateWindowEx(&params);
    XPLMSetWindowPositioningMode(gWindow, xplm_WindowPositionFree, -1);
    XPLMSetWindowTitle(gWindow, "ToLiss FCU Monitor");

    return 1;
}

PLUGIN_API void XPluginStop(void)
{
    if (gWindow) {
        XPLMDestroyWindow(gWindow);
        gWindow = nullptr;
    }
}

PLUGIN_API int XPluginEnable(void)  { return 1; }
PLUGIN_API void XPluginDisable(void) { }
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int, void*) { }
