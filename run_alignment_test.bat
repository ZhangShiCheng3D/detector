@echo off
chcp 65001 >nul
echo =========================================
echo   对齐可视化测试脚本
echo =========================================
echo.
echo 正在设置环境变量...
set PATH=E:\OpenSource\vcpkg\installed\x64-windows\bin;%PATH%

echo 正在运行程序...
echo 提示：请查找标题为 "Alignment Check - Color Overlay" 的窗口
echo.

build\Release\demo_cpp.exe ^
    --template "data/20260204/20260204/20260204_1348395638_Bar1_QR1_OCR0_Blob1_Loc1_Dis1_Com0.bmp" ^
    --roi 200,420,820,320,0.8 ^
    --max-images 1 ^
    --delay 10000

echo.
echo 程序已退出。
pause
