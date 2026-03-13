/**
 * @file test_performance.cpp
 * @brief 性能基准测试
 */

#include "defect_detector.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>

namespace fs = std::filesystem;

using namespace defect_detection;

struct PerformanceMetrics {
    double min_time;
    double max_time;
    double avg_time;
    double std_dev;
    double median_time;
    int total_images;
    int images_under_20ms;
};

PerformanceMetrics calculateMetrics(const std::vector<double>& times) {
    PerformanceMetrics metrics;
    metrics.total_images = static_cast<int>(times.size());
    
    if (times.empty()) {
        return metrics;
    }
    
    // 排序用于中位数
    std::vector<double> sorted_times = times;
    std::sort(sorted_times.begin(), sorted_times.end());
    
    metrics.min_time = sorted_times.front();
    metrics.max_time = sorted_times.back();
    metrics.median_time = sorted_times[sorted_times.size() / 2];
    
    // 平均值
    metrics.avg_time = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    
    // 标准差
    double sq_sum = 0;
    for (double t : times) {
        sq_sum += (t - metrics.avg_time) * (t - metrics.avg_time);
    }
    metrics.std_dev = std::sqrt(sq_sum / times.size());
    
    // 统计20ms以内的数量
    metrics.images_under_20ms = 0;
    for (double t : times) {
        if (t <= 20.0) {
            metrics.images_under_20ms++;
        }
    }
    
    return metrics;
}

void printMetrics(const std::string& name, const PerformanceMetrics& metrics) {
    std::cout << "\n=== " << name << " ===\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Total images: " << metrics.total_images << "\n";
    std::cout << "  Min time: " << metrics.min_time << " ms\n";
    std::cout << "  Max time: " << metrics.max_time << " ms\n";
    std::cout << "  Avg time: " << metrics.avg_time << " ms\n";
    std::cout << "  Median time: " << metrics.median_time << " ms\n";
    std::cout << "  Std dev: " << metrics.std_dev << " ms\n";
    std::cout << "  Images under 20ms: " << metrics.images_under_20ms 
              << " (" << (100.0 * metrics.images_under_20ms / metrics.total_images) << "%)\n";
}

int main(int argc, char** argv) {
    std::string data_dir = "data";
    int iterations = 1;
    
    if (argc > 1) {
        data_dir = argv[1];
    }
    if (argc > 2) {
        iterations = std::atoi(argv[2]);
    }
    
    std::cout << "=== Performance Benchmark ===\n";
    std::cout << "Data directory: " << data_dir << "\n";
    std::cout << "Iterations per image: " << iterations << "\n\n";
    
    // 获取所有图片
    std::vector<std::string> image_files;
    for (const auto& entry : fs::directory_iterator(data_dir)) {
        if (entry.path().extension() == ".bmp") {
            image_files.push_back(entry.path().string());
        }
    }
    
    if (image_files.empty()) {
        std::cerr << "Error: No BMP files found\n";
        return 1;
    }
    
    std::sort(image_files.begin(), image_files.end());
    std::cout << "Found " << image_files.size() << " images\n";
    
    // 创建检测器
    DefectDetector detector;
    
    // 使用第一张图片作为模板
    if (!detector.importTemplateFromFile(image_files[0])) {
        std::cerr << "Error: Failed to load template\n";
        return 1;
    }
    
    const cv::Mat& templ = detector.getTemplate();
    int total_pixels = templ.cols * templ.rows;
    std::cout << "Image size: " << templ.cols << "x" << templ.rows 
              << " (" << total_pixels << " pixels)\n\n";
    
    // 测试不同ROI配置
    std::cout << "Testing different ROI configurations...\n";
    
    // 配置1: 全图单ROI
    detector.clearROIs();
    detector.addROI(0, 0, templ.cols, templ.rows, 0.85f);
    
    std::vector<double> times_full;
    for (const auto& file : image_files) {
        for (int i = 0; i < iterations; ++i) {
            auto result = detector.detectFromFile(file);
            times_full.push_back(result.processing_time_ms);
        }
    }
    printMetrics("Full Image (1 ROI)", calculateMetrics(times_full));
    
    // 配置2: 4个ROI
    detector.clearROIs();
    int roi_w = templ.cols / 2;
    int roi_h = templ.rows / 2;
    detector.addROI(0, 0, roi_w, roi_h, 0.85f);
    detector.addROI(roi_w, 0, roi_w, roi_h, 0.85f);
    detector.addROI(0, roi_h, roi_w, roi_h, 0.85f);
    detector.addROI(roi_w, roi_h, roi_w, roi_h, 0.85f);
    
    std::vector<double> times_4roi;
    for (const auto& file : image_files) {
        for (int i = 0; i < iterations; ++i) {
            auto result = detector.detectFromFile(file);
            times_4roi.push_back(result.processing_time_ms);
        }
    }
    printMetrics("4 ROIs (Quarter Images)", calculateMetrics(times_4roi));
    
    // 配置3: 小ROI
    detector.clearROIs();
    detector.addROI(100, 100, 500, 400, 0.85f);
    
    std::vector<double> times_small;
    for (const auto& file : image_files) {
        for (int i = 0; i < iterations; ++i) {
            auto result = detector.detectFromFile(file);
            times_small.push_back(result.processing_time_ms);
        }
    }
    printMetrics("Small ROI (500x400)", calculateMetrics(times_small));
    
    // 总结
    std::cout << "\n=== Performance Summary ===\n";
    std::cout << "Target: <= 20ms per detection\n\n";
    
    auto metrics_full = calculateMetrics(times_full);
    auto metrics_4roi = calculateMetrics(times_4roi);
    auto metrics_small = calculateMetrics(times_small);
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Full Image: " << metrics_full.avg_time << "ms avg, " 
              << (metrics_full.avg_time <= 20.0 ? "PASS" : "FAIL") << "\n";
    std::cout << "4 ROIs: " << metrics_4roi.avg_time << "ms avg, "
              << (metrics_4roi.avg_time <= 20.0 ? "PASS" : "FAIL") << "\n";
    std::cout << "Small ROI: " << metrics_small.avg_time << "ms avg, "
              << (metrics_small.avg_time <= 20.0 ? "PASS" : "FAIL") << "\n";
    
    return 0;
}

