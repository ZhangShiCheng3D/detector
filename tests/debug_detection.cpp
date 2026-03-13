/**
 * @file debug_detection.cpp
 * @brief 调试瑕疵检测问题
 */

#include "defect_detector.h"
#include <iostream>
#include <opencv2/opencv.hpp>

using namespace defect_detection;

int main() {
    // 创建检测器
    DefectDetector detector;
    
    // 加载模板
    std::string templateFile = "data/20260204/20260204/20260204_1348395638_Bar1_QR1_OCR0_Blob1_Loc1_Dis1_Com0.bmp";
    if (!detector.importTemplateFromFile(templateFile)) {
        std::cerr << "无法加载模板\n";
        return 1;
    }
    
    std::cout << "模板加载成功: " << detector.getTemplate().cols << "x" << detector.getTemplate().rows << "\n";
    
    // 配置检测参数
    detector.getTemplateMatcher().setMethod(MatchMethod::BINARY);
    detector.getTemplateMatcher().setBinaryThreshold(30);
    
    BinaryDetectionParams binaryParams;
    binaryParams.enabled = true;
    binaryParams.noise_filter_size = 3;
    binaryParams.edge_tolerance_pixels = 2;
    binaryParams.min_significant_area = 20;
    binaryParams.overall_similarity_threshold = 0.90f;
    detector.setBinaryDetectionParams(binaryParams);
    
    detector.setParameter("min_defect_size", 100.0f);
    detector.setParameter("blur_kernel_size", 3.0f);
    detector.setParameter("detect_black_on_white", 1.0f);
    detector.setParameter("detect_white_on_black", 1.0f);
    
    // 添加ROI
    DetectionParams params;
    params.binary_threshold = 30;
    params.blur_kernel_size = 3;
    params.min_defect_size = 100;
    params.detect_black_on_white = true;
    params.detect_white_on_black = true;
    
    int roiId = detector.getROIManager().addROI(
        defect_detection::Rect(200, 420, 820, 320), 
        0.8f, 
        params);
    
    // 提取模板ROI
    detector.getROIManager().extractTemplates(detector.getTemplate());
    
    auto* roi = detector.getROIManager().getROI(roiId);
    if (roi) {
        std::cout << "ROI " << roiId << " 模板图像: " 
                  << roi->template_image.cols << "x" << roi->template_image.rows 
                  << " 通道: " << roi->template_image.channels() << "\n";
    }
    
    // 测试相同图像（应该高相似度，无瑕疵）
    std::cout << "\n=== 测试1: 模板自身 ===\n";
    DetectionResult result1 = detector.detectFromFile(templateFile);
    std::cout << "相似度: " << result1.roi_results[0].similarity << "\n";
    std::cout << "瑕疵数: " << result1.roi_results[0].defects.size() << "\n";
    
    // 测试另一张图像
    std::cout << "\n=== 测试2: 另一张图像 ===\n";
    std::string testFile = "data/20260204/20260204/20260204_1357234441_Bar0_QR1_OCR1_Blob1_Loc1_Dis1_Com0.bmp";
    
    // 加载测试图像查看
    cv::Mat testImg = cv::imread(testFile);
    if (!testImg.empty()) {
        std::cout << "测试图像: " << testImg.cols << "x" << testImg.rows 
                  << " 通道: " << testImg.channels() << "\n";
        
        // 手动提取ROI查看
        cv::Mat testRoi = testImg(cv::Rect(200, 420, 820, 320));
        std::cout << "测试ROI: " << testRoi.cols << "x" << testRoi.rows << "\n";
        
        // 转换为灰度
        cv::Mat testGray, templGray;
        cv::cvtColor(testRoi, testGray, cv::COLOR_BGR2GRAY);
        cv::cvtColor(roi->template_image, templGray, cv::COLOR_BGR2GRAY);
        
        // 计算差异
        cv::Mat diff;
        cv::absdiff(templGray, testGray, diff);
        
        cv::Mat binaryDiff;
        cv::threshold(diff, binaryDiff, 30, 255, cv::THRESH_BINARY);
        int diffPixels = cv::countNonZero(binaryDiff);
        std::cout << "差异像素数 (阈值30): " << diffPixels << "\n";
        
        // 找轮廓
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binaryDiff, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        std::cout << "轮廓数: " << contours.size() << "\n";
        for (size_t i = 0; i < contours.size() && i < 5; ++i) {
            double area = cv::contourArea(contours[i]);
            std::cout << "  轮廓 " << i << " 面积: " << area << "\n";
        }
    }
    
    // 使用检测器检测
    DetectionResult result2 = detector.detectFromFile(testFile);
    std::cout << "\n检测器结果:\n";
    std::cout << "  相似度: " << result2.roi_results[0].similarity << "\n";
    std::cout << "  瑕疵数: " << result2.roi_results[0].defects.size() << "\n";
    for (const auto& defect : result2.roi_results[0].defects) {
        std::cout << "  瑕疵: 类型=" << defect.type << " 面积=" << defect.area 
                  << " 位置=(" << defect.x << "," << defect.y << ")\n";
    }
    
    return 0;
}
