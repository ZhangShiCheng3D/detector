@echo off
chcp 65001 >nul
setlocal

:: 设置 OpenCV 路径
set "OPENCV_PATH=E:\OpenSource\vcpkg\installed\x64-windows\bin"

:: 设置 DLL 路径
set "DLL_PATH=%~dp0build\Release"

:: 更新 PATH
set "PATH=%DLL_PATH%;%OPENCV_PATH%;%PATH%"

echo ========================================
echo 运行 API 日志功能测试
echo ========================================
echo.

:: 运行测试程序
"%DLL_PATH%\test_api_logger.exe"

:: 保存退出代码
set "EXIT_CODE=%ERRORLEVEL%"

echo.
echo ========================================
echo 程序退出代码: %EXIT_CODE%
echo.

:: 检查是否生成了日志文件
if exist "%~dp0test_api.log" (
    echo 日志文件已生成: test_api.log
    echo.
    echo 日志内容预览 (前 20 行):
    echo ----------------------------------------
    head -20 "%~dp0test_api.log" 2>nul || type "%~dp0test_api.log" | more /n /e /c /t 2>nul || type "%~dp0test_api.log"
) else (
    echo 警告: 未找到日志文件 test_api.log
)

echo.
pause
