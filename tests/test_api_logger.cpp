/**
 * @file test_api_logger.cpp
 * @brief 测试API日志功能
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include "../api/c_api.h"

int main() {
    std::cout << "=== API Logger Test ===" << std::endl;
    
    // 1. 设置自定义日志文件
    std::cout << "Setting log file to: test_api.log" << std::endl;
    SetAPILogFile("test_api.log");
    
    // 2. 测试版本信息接口（会记录日志）
    std::cout << "Testing GetLibraryVersion..." << std::endl;
    const char* version = GetLibraryVersion();
    std::cout << "Version: " << version << std::endl;
    
    std::cout << "Testing GetBuildInfo..." << std::endl;
    const char* build_info = GetBuildInfo();
    std::cout << "Build Info: " << build_info << std::endl;
    
    // 3. 测试检测器创建（会记录日志）
    std::cout << "Creating detector..." << std::endl;
    void* detector = CreateDetector();
    if (!detector) {
        std::cout << "Failed to create detector!" << std::endl;
        return 1;
    }
    std::cout << "Detector created: " << detector << std::endl;
    
    // 4. 测试获取ROI数量（会记录日志）
    std::cout << "Getting ROI count..." << std::endl;
    int roi_count = GetROICount(detector);
    std::cout << "ROI count: " << roi_count << std::endl;
    
    // 5. 测试添加ROI（会记录日志）
    std::cout << "Adding ROI..." << std::endl;
    int roi_id = AddROI(detector, 100, 100, 200, 150, 0.85f);
    std::cout << "ROI ID: " << roi_id << std::endl;
    
    // 6. 测试获取ROI信息（会记录日志）
    std::cout << "Getting ROI info..." << std::endl;
    ROIInfoC roi_info;
    int ret = GetROIInfo(detector, 0, &roi_info);
    std::cout << "GetROIInfo returned: " << ret << std::endl;
    if (ret == 0) {
        std::cout << "ROI Info: id=" << roi_info.id 
                  << ", x=" << roi_info.x 
                  << ", y=" << roi_info.y 
                  << ", w=" << roi_info.width 
                  << ", h=" << roi_info.height 
                  << ", threshold=" << roi_info.similarity_threshold << std::endl;
    }
    
    // 7. 测试设置参数（会记录日志）
    std::cout << "Setting parameters..." << std::endl;
    SetParameter(detector, "binary_threshold", 128.0f);
    SetParameter(detector, "min_defect_size", 50.0f);
    
    // 8. 测试获取参数（会记录日志）
    std::cout << "Getting parameters..." << std::endl;
    float threshold = GetParameter(detector, "binary_threshold");
    std::cout << "binary_threshold: " << threshold << std::endl;
    
    // 9. 测试对齐模式设置（会记录日志）
    std::cout << "Setting alignment mode..." << std::endl;
    SetAlignmentMode(detector, 1); // FULL_IMAGE
    int mode = GetAlignmentMode(detector);
    std::cout << "Alignment mode: " << mode << std::endl;
    
    // 10. 销毁检测器（会记录日志）
    std::cout << "Destroying detector..." << std::endl;
    DestroyDetector(detector);
    
    // 等待日志写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 11. 检查日志文件
    std::cout << std::endl << "=== Checking Log File ===" << std::endl;
    std::ifstream log_file("test_api.log");
    if (log_file.is_open()) {
        std::string line;
        int line_count = 0;
        std::cout << "Log content:" << std::endl;
        while (std::getline(log_file, line) && line_count < 30) {
            std::cout << line << std::endl;
            line_count++;
        }
        log_file.close();
        std::cout << std::endl << "Total lines in log: " << line_count << "+" << std::endl;
    } else {
        std::cout << "ERROR: Could not open log file!" << std::endl;
        return 1;
    }
    
    std::cout << std::endl << "=== Test Completed ===" << std::endl;
    return 0;
}
