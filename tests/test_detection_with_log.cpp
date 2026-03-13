/**
 * @file test_detection_with_log.cpp
 * @brief 测试检测功能的详细日志输出
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "../api/c_api.h"

// 简单的 BMP 文件加载（仅用于测试）
struct SimpleImage {
    int width;
    int height;
    int channels;
    std::vector<unsigned char> data;
};

SimpleImage createTestImage(int width, int height, int channels) {
    SimpleImage img;
    img.width = width;
    img.height = height;
    img.channels = channels;
    img.data.resize(width * height * channels, 128); // 灰色背景
    return img;
}

void printLogFile(const char* filename) {
    std::ifstream log_file(filename);
    if (log_file.is_open()) {
        std::string line;
        std::cout << "\n=== Log File Contents ===\n" << std::endl;
        while (std::getline(log_file, line)) {
            // 只打印检测相关的日志
            if (line.find("[DET]") != std::string::npos || 
                line.find("Detect") != std::string::npos) {
                std::cout << line << std::endl;
            }
        }
        log_file.close();
    }
}

int main() {
    std::cout << "=== Detection Logger Test ===" << std::endl;
    
    // 设置日志文件
    SetAPILogFile("detection_test.log");
    std::cout << "Log file set to: detection_test.log" << std::endl;
    
    // 创建检测器
    std::cout << "\n1. Creating detector..." << std::endl;
    void* detector = CreateDetector();
    if (!detector) {
        std::cout << "Failed to create detector!" << std::endl;
        return 1;
    }
    std::cout << "   Detector created successfully" << std::endl;
    
    // 创建测试模板图像
    std::cout << "\n2. Creating test template (100x100 gray)..." << std::endl;
    SimpleImage template_img = createTestImage(100, 100, 1);
    int ret = ImportTemplate(detector, template_img.data.data(), 
                             template_img.width, template_img.height, 
                             template_img.channels);
    std::cout << "   ImportTemplate returned: " << ret << std::endl;
    
    // 添加 ROI
    std::cout << "\n3. Adding ROIs..." << std::endl;
    int roi1 = AddROI(detector, 10, 10, 30, 30, 0.85f);
    int roi2 = AddROI(detector, 50, 50, 40, 40, 0.90f);
    std::cout << "   ROI 1 ID: " << roi1 << std::endl;
    std::cout << "   ROI 2 ID: " << roi2 << std::endl;
    
    // 设置检测参数
    std::cout << "\n4. Setting detection parameters..." << std::endl;
    SetParameter(detector, "binary_threshold", 128.0f);
    SetParameter(detector, "min_defect_size", 10.0f);
    std::cout << "   Parameters set" << std::endl;
    
    // 创建测试图像（与模板相同，应该没有瑕疵）
    std::cout << "\n5. Running detection (no defects expected)..." << std::endl;
    SimpleImage test_img = createTestImage(100, 100, 1);
    float similarities[2] = {0};
    int defect_count = 0;
    
    ret = DetectDefects(detector, test_img.data.data(),
                        test_img.width, test_img.height, test_img.channels,
                        similarities, &defect_count);
    std::cout << "   DetectDefects returned: " << ret << " (0=PASS, 1=FAIL, -1=ERROR)" << std::endl;
    std::cout << "   Defect count: " << defect_count << std::endl;
    std::cout << "   ROI similarities: [" << similarities[0] << ", " << similarities[1] << "]" << std::endl;
    
    // 使用 Ex 接口检测
    std::cout << "\n6. Running detection with Ex interface..." << std::endl;
    DefectInfo defects[10];
    int actual_defects = 0;
    
    ret = DetectDefectsEx(detector, test_img.data.data(),
                          test_img.width, test_img.height, test_img.channels,
                          defects, 10, &actual_defects);
    std::cout << "   DetectDefectsEx returned: " << ret << std::endl;
    std::cout << "   Actual defects: " << actual_defects << std::endl;
    
    // 销毁检测器
    std::cout << "\n7. Destroying detector..." << std::endl;
    DestroyDetector(detector);
    std::cout << "   Done" << std::endl;
    
    // 等待日志写入
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 打印日志
    printLogFile("detection_test.log");
    
    std::cout << "\n=== Test Completed ===" << std::endl;
    std::cout << "\nFull log saved to: detection_test.log" << std::endl;
    
    return 0;
}
