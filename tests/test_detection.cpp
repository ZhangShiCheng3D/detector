/**
 * @file test_detection.cpp
 * @brief 瑕疵检测功能测试
 */

#include "defect_detector.h"
#include <iostream>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

using namespace defect_detection;

void printResult(const DetectionResult& result) {
    std::cout << "========================================\n";
    std::cout << "Detection Result:\n";
    std::cout << "  Overall Passed: " << (result.overall_passed ? "YES" : "NO") << "\n";
    std::cout << "  Total Defects: " << result.total_defect_count << "\n";
    std::cout << "  Processing Time: " << std::fixed << std::setprecision(2)
              << result.processing_time_ms << " ms\n";

    // 显示定位信息
    if (result.localization.success) {
        std::cout << "  Localization: offset=(" << result.localization.offset_x << ", "
                  << result.localization.offset_y << "), rotation="
                  << result.localization.rotation_angle << " deg";
        if (result.localization.from_external) {
            std::cout << " [EXTERNAL]";
        }
        std::cout << "\n";
    }

    std::cout << "\nROI Results:\n";
    for (const auto& roi_result : result.roi_results) {
        std::cout << "  ROI " << roi_result.roi_id << ": ";
        std::cout << "Similarity = " << std::fixed << std::setprecision(4)
                  << roi_result.similarity;
        std::cout << ", Threshold = " << roi_result.threshold;
        std::cout << ", Passed = " << (roi_result.passed ? "YES" : "NO");
        std::cout << ", Defects = " << roi_result.defects.size() << "\n";

        for (const auto& defect : roi_result.defects) {
            // 新格式：显示瑕疵类型（漏墨/漏喷/混合瑕疵）和位置(X+Y)
            std::cout << "    - " << defectTypeToString(defect.defect_type)
                      << " [" << defect.type << "]"
                      << " at pos=(" << defect.x << "," << defect.y << ")"
                      << " rect=(" << defect.location.x << "," << defect.location.y << ")"
                      << " size=" << defect.location.width << "x" << defect.location.height
                      << " area=" << defect.area
                      << " severity=" << std::setprecision(2) << defect.severity << "\n";
        }
    }
    std::cout << "========================================\n\n";
}

int main(int argc, char** argv) {
    std::string data_dir = "data";
    if (argc > 1) {
        data_dir = argv[1];
    }

    std::cout << "=== Defect Detection System Test ===\n\n" << std::flush;
    
    // 检查数据目录
    if (!fs::exists(data_dir)) {
        std::cerr << "Error: Data directory not found: " << data_dir << "\n";
        return 1;
    }
    
    // 获取所有BMP文件
    std::vector<std::string> image_files;
    for (const auto& entry : fs::directory_iterator(data_dir)) {
        if (entry.path().extension() == ".bmp") {
            image_files.push_back(entry.path().string());
        }
    }
    
    if (image_files.empty()) {
        std::cerr << "Error: No BMP files found in " << data_dir << "\n";
        return 1;
    }
    
    std::sort(image_files.begin(), image_files.end());
    std::cout << "Found " << image_files.size() << " images\n\n";
    
    // 创建检测器
    DefectDetector detector;
    
    // 使用第一张图片作为模板
    std::string template_file = image_files[0];
    std::cout << "Loading template: " << template_file << "\n";
    
    if (!detector.importTemplateFromFile(template_file)) {
        std::cerr << "Error: Failed to load template\n";
        return 1;
    }
    
    // 获取模板尺寸
    const cv::Mat& templ = detector.getTemplate();
    std::cout << "Template size: " << templ.cols << "x" << templ.rows << "\n";
    std::cout << "Total pixels: " << templ.cols * templ.rows << "\n\n";
    
    // 设置ROI区域（覆盖整个图像）
    int roi_id1 = detector.addROI(0, 0, templ.cols, templ.rows, 0.85f);
    std::cout << "Added ROI " << roi_id1 << " (full image)\n";
    
    // 添加几个子区域的ROI用于测试
    int roi_w = templ.cols / 4;
    int roi_h = templ.rows / 4;
    
    int roi_id2 = detector.addROI(100, 100, roi_w, roi_h, 0.90f);
    std::cout << "Added ROI " << roi_id2 << " (top-left region)\n";
    
    int roi_id3 = detector.addROI(templ.cols - roi_w - 100, 100, roi_w, roi_h, 0.90f);
    std::cout << "Added ROI " << roi_id3 << " (top-right region)\n";
    
    int roi_id4 = detector.addROI(templ.cols/2 - roi_w/2, templ.rows/2 - roi_h/2, roi_w, roi_h, 0.90f);
    std::cout << "Added ROI " << roi_id4 << " (center region)\n";
    
    std::cout << "\nTotal ROIs: " << detector.getROICount() << "\n\n";
    
    // 设置检测参数（可调阈值参数）
    detector.setParameter("binary_threshold", 25);  // 瑕疵检测阈值
    detector.setParameter("min_defect_size", 10);    // 最小瑕疵尺寸
    detector.setParameter("blur_kernel_size", 3);

    std::cout << "\n=== 测试新API: 外部变换参数 ===\n";
    // 测试外部变换API（X+Y+R）
    std::cout << "Setting external transform: offset_x=2.0, offset_y=1.5, rotation=0.5\n";
    detector.setExternalTransform(2.0f, 1.5f, 0.5f);
    detector.setAlignmentMode(AlignmentMode::ROI_ONLY);

    ExternalTransform ext = detector.getExternalTransform();
    std::cout << "External transform set: is_set=" << ext.is_set
              << ", x=" << ext.offset_x << ", y=" << ext.offset_y
              << ", r=" << ext.rotation << "\n";

    // 测试前几张图片（使用外部变换）
    int test_count = std::min(5, (int)image_files.size());
    int passed_count = 0;
    double total_time = 0;

    std::cout << "\nTesting " << test_count << " images with external transform...\n\n";

    for (int i = 0; i < test_count; ++i) {
        std::cout << "Processing: " << image_files[i] << "\n";

        DetectionResult result = detector.detectFromFile(image_files[i]);
        printResult(result);

        if (result.overall_passed) {
            passed_count++;
        }
        total_time += result.processing_time_ms;
    }

    // 清除外部变换，再测试
    std::cout << "\n=== 测试无变换模式 ===\n";
    detector.clearExternalTransform();
    detector.setAlignmentMode(AlignmentMode::NONE);

    for (int i = 0; i < test_count; ++i) {
        std::cout << "Processing (no transform): " << image_files[i] << "\n";

        DetectionResult result = detector.detectFromFile(image_files[i]);
        printResult(result);

        if (result.overall_passed) {
            passed_count++;
        }
        total_time += result.processing_time_ms;
    }
    
    // 统计结果
    std::cout << "\n=== Summary ===\n";
    std::cout << "Images tested: " << test_count << "\n";
    std::cout << "Passed: " << passed_count << "\n";
    std::cout << "Failed: " << (test_count - passed_count) << "\n";
    std::cout << "Average processing time: " << std::fixed << std::setprecision(2) 
              << (total_time / test_count) << " ms\n";
    
    // 性能检查
    double avg_time = total_time / test_count;
    if (avg_time <= 20.0) {
        std::cout << "\n✓ Performance requirement met: " << avg_time << "ms <= 20ms\n";
    } else {
        std::cout << "\n✗ Performance requirement NOT met: " << avg_time << "ms > 20ms\n";
    }
    
    return 0;
}

