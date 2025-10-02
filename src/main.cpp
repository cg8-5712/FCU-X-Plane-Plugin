#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include <string>
#include <sstream>

// 📌 ToLiss FCU DataRef 定义
XPLMDataRef gSPD = nullptr;
XPLMDataRef gHDG = nullptr;
XPLMDataRef gALT = nullptr;
XPLMDataRef gVS  = nullptr;
XPLMDataRef gAP1 = nullptr;
XPLMDataRef gAP2 = nullptr;

XPLMWindowID gWindow = nullptr;

// 绘制函数
void DrawWindowCallback(XPLMWindowID inWindowID, void* inRefcon)
{
    int l, t, r, b;
    XPLMGetWindowGeometry(inWindowID, &l, &t, &r, &b);

    // 读取 DataRef
    float spd = XPLMGetDataf(gSPD);
    float hdg = XPLMGetDataf(gHDG);
    float alt = XPLMGetDataf(gALT);
    float vs  = XPLMGetDataf(gVS);
    int ap1   = XPLMGetDatai(gAP1);
    int ap2   = XPLMGetDatai(gAP2);

    // 拼接显示字符串
    std::ostringstream oss;
    oss << "ToLiss FCU Data\n"
        << "SPD: " << spd << "\n"
        << "HDG: " << hdg << "\n"
        << "ALT: " << alt << "\n"
        << "V/S: " << vs  << "\n"
        << "AP1: " << ap1 << "  AP2: " << ap2;

    std::string text = oss.str();

    // 绘制文本
    float white[3] = {1.0f, 1.0f, 1.0f};
    int lineHeight = 15;
    int y = t - 20;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        XPLMDrawString(white, l + 10, y, (char*)line.c_str(), nullptr, xplmFont_Basic);
        y -= lineHeight;
    }
}

// 鼠标、键盘回调空实现
int DummyMouse(XPLMWindowID, int, int, int, void*) { return 0; }
int DummyKey(XPLMWindowID, char, XPLMKeyFlags, char, void*, int) { return 0; }
XPLMCursorStatus DummyCursor(XPLMWindowID, int, int, void*) { return xplm_CursorDefault; }

// 🧩 插件入口函数
PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc)
{
    strcpy(outName, "ToLissFCUMonitor");
    strcpy(outSig, "dzc.toliss.fcu.monitor");
    strcpy(outDesc, "Display ToLiss FCU Data in X-Plane.");

    // 获取 ToLiss DataRefs
    gSPD = XPLMFindDataRef("AirbusFBW/FCU_SPD_dial_kts");
    gHDG = XPLMFindDataRef("AirbusFBW/FCU_HDG_dial_deg");
    gALT = XPLMFindDataRef("AirbusFBW/FCU_ALT_dial_ft");
    gVS  = XPLMFindDataRef("AirbusFBW/FCU_VS_dial_fpm");
    gAP1 = XPLMFindDataRef("AirbusFBW/AP1Engage");
    gAP2 = XPLMFindDataRef("AirbusFBW/AP2Engage");

    // 创建窗口
    XPLMCreateWindow_t params = { 0 };
    params.structSize = sizeof(params);
    params.visible = 1;
    params.drawWindowFunc = DrawWindowCallback;
    params.handleMouseClickFunc = DummyMouse;
    params.handleKeyFunc = DummyKey;
    params.handleCursorFunc = DummyCursor;
    params.handleMouseWheelFunc = nullptr;
    params.refcon = nullptr;
    params.left = 100;
    params.top = 600;
    params.right = 350;
    params.bottom = 400;
    params.decorations = xplm_WindowDecorationRoundRectangle;

    gWindow = XPLMCreateWindowEx(&params);
    XPLMSetWindowPositioningMode(gWindow, xplm_WindowPositionFree, -1);
    XPLMSetWindowTitle(gWindow, "ToLiss FCU Data");

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
