@echo off
chcp 65001 >nul
echo ========================================
echo ToLiss FCU Monitor - Clean Script
echo ========================================
echo.
echo 警告：此操作将删除所有构建文件
echo.
set /p confirm="确认清理？(Y/N): "
if /i not "%confirm%"=="Y" (
    echo 已取消清理
    pause
    exit /b 0
)

echo.
echo 正在清理构建文件...

if exist "build" (
    echo [删除] build 目录
    rmdir /s /q build
)

if exist "dist" (
    echo [删除] dist 目录
    rmdir /s /q dist
)

echo.
echo ========================================
echo 清理完成！
echo ========================================
echo.
pause
