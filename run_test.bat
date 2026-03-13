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
echo 环境变量已设置
echo OpenCV 路径: %OPENCV_PATH%
echo DLL 路径: %DLL_PATH%
echo ========================================
echo.

:: 检查参数
if "%~1"=="" (
    echo 用法: run_test.bat [test_name]
    echo.
    echo 可用的测试程序:
    echo   - test_api_logger    : API 日志功能测试
    echo   - test_api_v2        : API v2.0 测试
    echo   - test_detection     : 基础检测测试
    echo   - test_performance   : 性能测试
    echo   - demo               : 演示程序
    echo   - demo_cpp           : C++ 演示程序
    echo.
    echo 示例: run_test.bat test_api_logger
    goto :end
)

:: 运行指定的测试程序
set "TEST_EXE=%DLL_PATH%\%~1.exe"

if not exist "%TEST_EXE%" (
    echo 错误: 找不到测试程序 "%TEST_EXE%"
    echo.
    echo 可用的测试程序:
    dir /b "%DLL_PATH%\*.exe" 2>nul
    goto :end
)

echo 正在运行: %~1.exe
echo ========================================
echo.
"%TEST_EXE%"
echo.
echo ========================================
echo 程序退出代码: %ERRORLEVEL%

:end
endlocal
pause
