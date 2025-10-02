@echo off
chcp 65001 >nul
echo ========================================
echo ToLiss FCU Monitor - Build Script
echo ========================================
echo.

echo [1/2] 配置 CMake 项目...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [错误] CMake 配置失败！
    pause
    exit /b 1
)

echo.
echo [2/2] 编译项目...
cmake --build build --config Release
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [错误] 编译失败！
    pause
    exit /b 1
)

echo.
echo ========================================
echo 构建成功！
echo ========================================
echo.
echo 插件文件位置：
echo   - build\Release\win.xpl
echo   - dist\Release\win.xpl
echo.

if exist "dist\Release\win.xpl" (
    echo 文件大小：
    dir "dist\Release\win.xpl" | findstr "win.xpl"
    echo.
)

echo 提示：如果需要复制到 X-Plane，请先关闭 X-Plane
echo.
pause
