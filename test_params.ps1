cd build/Release
./demo_dll.exe `
    --template "../../data/20260204/20260204/20260204_1348395638_Bar1_QR1_OCR0_Blob1_Loc1_Dis1_Com0.bmp" `
    --roi 200,420,820,320,0.8 `
    --max-images 1 `
    --binary-thresh 128 `
    "../../data/20260204/20260204" 2>&1
