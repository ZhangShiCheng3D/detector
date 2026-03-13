/**
 * @file defect_analyzer.cpp
 * @brief 瑕疵分析引擎实现
 */

#include "defect_analyzer.h"
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace defect_detection {

DefectAnalyzer::DefectAnalyzer() {
}

DefectAnalyzer::~DefectAnalyzer() {
}

void DefectAnalyzer::setParams(const DetectionParams& params) {
    params_ = params;
}

const DetectionParams& DefectAnalyzer::getParams() const {
    return params_;
}

std::vector<Defect> DefectAnalyzer::detectBlackOnWhite(const cv::Mat& test_img,
                                                        const cv::Mat& template_img) {
    if (!params_.detect_black_on_white) {
        return {};
    }

    // 计算差分图像
    cv::Mat diff = computeDifference(test_img, template_img);

    // 检测暗色区域（测试图比模板暗的区域）
    cv::Mat test_gray, templ_gray;
    if (test_img.channels() == 3) {
        cv::cvtColor(test_img, test_gray, cv::COLOR_BGR2GRAY);
    } else {
        test_gray = test_img;
    }
    if (template_img.channels() == 3) {
        cv::cvtColor(template_img, templ_gray, cv::COLOR_BGR2GRAY);
    } else {
        templ_gray = template_img;
    }

    // 测试图更暗的区域（黑色脏污）- 这实际上是漏喷（本应喷印的地方没有喷印，显示底色）
    cv::Mat dark_regions;
    cv::subtract(templ_gray, test_gray, dark_regions);

    // 调试输出：统计暗区像素值分布
    cv::Scalar mean_val = cv::mean(dark_regions);
    double min_val, max_val;
    cv::minMaxLoc(dark_regions, &min_val, &max_val);

    // 二值化
    cv::Mat binary;
    cv::threshold(dark_regions, binary, params_.binary_threshold, 255, cv::THRESH_BINARY);
    
    // 调试输出：二值化后的前景像素数
    int fg_pixels = cv::countNonZero(binary);

    // 形态学处理
    cv::Mat binary_morph = applyMorphology(binary);
    int fg_pixels_after = cv::countNonZero(binary_morph);

    // 提取瑕疵 - 漏喷类型
    auto defects = extractDefectsFromBinary(binary_morph, "spray_missing");

    // 调试输出
    if (!defects.empty() || fg_pixels > 1000) {
        std::cout << "    [detectBlackOnWhite] 阈值=" << params_.binary_threshold 
                  << ", 暗区均值=" << mean_val[0] << ", 范围[" << min_val << "-" << max_val 
                  << "], 二值前景=" << fg_pixels << "->" << fg_pixels_after 
                  << ", 瑕疵数=" << defects.size() << "\n";
    }

    return defects;
}

std::vector<Defect> DefectAnalyzer::detectWhiteOnBlack(const cv::Mat& test_img,
                                                        const cv::Mat& template_img) {
    if (!params_.detect_white_on_black) {
        return {};
    }

    cv::Mat test_gray, templ_gray;
    if (test_img.channels() == 3) {
        cv::cvtColor(test_img, test_gray, cv::COLOR_BGR2GRAY);
    } else {
        test_gray = test_img;
    }
    if (template_img.channels() == 3) {
        cv::cvtColor(template_img, templ_gray, cv::COLOR_BGR2GRAY);
    } else {
        templ_gray = template_img;
    }

    // 测试图更亮的区域 - 这是漏墨（本应有墨的地方没有墨，显示白色/浅色）
    cv::Mat bright_regions;
    cv::subtract(test_gray, templ_gray, bright_regions);

    // 调试输出：统计亮区像素值分布
    cv::Scalar mean_val = cv::mean(bright_regions);
    double min_val, max_val;
    cv::minMaxLoc(bright_regions, &min_val, &max_val);

    // 二值化
    cv::Mat binary;
    cv::threshold(bright_regions, binary, params_.binary_threshold, 255, cv::THRESH_BINARY);
    
    // 调试输出：二值化后的前景像素数
    int fg_pixels = cv::countNonZero(binary);

    // 形态学处理
    cv::Mat binary_morph = applyMorphology(binary);
    int fg_pixels_after = cv::countNonZero(binary_morph);

    // 提取瑕疵 - 漏墨类型
    auto defects = extractDefectsFromBinary(binary_morph, "ink_missing");

    // 调试输出
    if (!defects.empty() || fg_pixels > 1000) {
        std::cout << "    [detectWhiteOnBlack] 阈值=" << params_.binary_threshold 
                  << ", 亮区均值=" << mean_val[0] << ", 范围[" << min_val << "-" << max_val 
                  << "], 二值前景=" << fg_pixels << "->" << fg_pixels_after 
                  << ", 瑕疵数=" << defects.size() << "\n";
    }

    return defects;
}

std::vector<Defect> DefectAnalyzer::detectAllDefects(const cv::Mat& test_img,
                                                      const cv::Mat& template_img) {
    std::vector<Defect> all_defects;

    // 检测漏喷（测试图比模板暗）
    auto spray_defects = detectBlackOnWhite(test_img, template_img);

    // 检测漏墨（测试图比模板亮）
    auto ink_defects = detectWhiteOnBlack(test_img, template_img);

    // 检测混合瑕疵：如果同一区域同时有漏墨和漏喷，标记为混合瑕疵
    for (auto& spray_def : spray_defects) {
        bool is_mixed = false;
        for (auto& ink_def : ink_defects) {
            // 检查是否有重叠
            cv::Rect r1 = spray_def.location.toCvRect();
            cv::Rect r2 = ink_def.location.toCvRect();
            cv::Rect intersection = r1 & r2;

            if (intersection.area() > 0) {
                // 有重叠，标记为混合瑕疵
                is_mixed = true;
                ink_def.defect_type = DefectType::MIXED_DEFECT;
                ink_def.type = "mixed_defect";
            }
        }
        if (is_mixed) {
            spray_def.defect_type = DefectType::MIXED_DEFECT;
            spray_def.type = "mixed_defect";
        }
    }

    all_defects.insert(all_defects.end(), spray_defects.begin(), spray_defects.end());
    all_defects.insert(all_defects.end(), ink_defects.begin(), ink_defects.end());

    return all_defects;
}

std::vector<Defect> DefectAnalyzer::detectPositionShift(const cv::Mat& test_img, 
                                                         const cv::Mat& template_img,
                                                         float max_offset) {
    std::vector<Defect> defects;
    
    // 使用特征点匹配检测位置偏移
    cv::Ptr<cv::ORB> orb = cv::ORB::create(500);
    std::vector<cv::KeyPoint> kp1, kp2;
    cv::Mat desc1, desc2;
    
    cv::Mat test_gray, templ_gray;
    if (test_img.channels() == 3) {
        cv::cvtColor(test_img, test_gray, cv::COLOR_BGR2GRAY);
    } else {
        test_gray = test_img;
    }
    if (template_img.channels() == 3) {
        cv::cvtColor(template_img, templ_gray, cv::COLOR_BGR2GRAY);
    } else {
        templ_gray = template_img;
    }
    
    orb->detectAndCompute(templ_gray, cv::noArray(), kp1, desc1);
    orb->detectAndCompute(test_gray, cv::noArray(), kp2, desc2);
    
    if (desc1.empty() || desc2.empty()) {
        return defects;
    }
    
    // 特征匹配
    cv::BFMatcher matcher(cv::NORM_HAMMING);
    std::vector<cv::DMatch> matches;
    matcher.match(desc1, desc2, matches);
    
    if (matches.empty()) {
        return defects;
    }
    
    // 计算平均偏移
    float total_dx = 0, total_dy = 0;
    int count = 0;
    
    // 排序并取前30%的好匹配
    std::sort(matches.begin(), matches.end(), 
              [](const cv::DMatch& a, const cv::DMatch& b) { return a.distance < b.distance; });
    
    int good_count = std::max(10, (int)(matches.size() * 0.3));
    for (int i = 0; i < good_count && i < (int)matches.size(); ++i) {
        const auto& m = matches[i];
        float dx = kp2[m.trainIdx].pt.x - kp1[m.queryIdx].pt.x;
        float dy = kp2[m.trainIdx].pt.y - kp1[m.queryIdx].pt.y;
        total_dx += dx;
        total_dy += dy;
        count++;
    }

    if (count > 0) {
        float avg_dx = total_dx / count;
        float avg_dy = total_dy / count;
        float offset = std::sqrt(avg_dx * avg_dx + avg_dy * avg_dy);

        if (offset > max_offset) {
            Defect defect;
            defect.type = "position_shift";
            defect.severity = std::min(1.0f, offset / (max_offset * 3));
            defect.area = offset;
            defect.location = Rect(0, 0, test_img.cols, test_img.rows);
            defects.push_back(defect);
        }
    }

    return defects;
}

cv::Mat DefectAnalyzer::computeDifference(const cv::Mat& test_img, const cv::Mat& template_img) {
    cv::Mat test_gray, templ_gray;

    if (test_img.channels() == 3) {
        cv::cvtColor(test_img, test_gray, cv::COLOR_BGR2GRAY);
    } else {
        test_gray = test_img;
    }

    if (template_img.channels() == 3) {
        cv::cvtColor(template_img, templ_gray, cv::COLOR_BGR2GRAY);
    } else {
        templ_gray = template_img;
    }

    // 确保尺寸匹配
    if (test_gray.size() != templ_gray.size()) {
        cv::resize(test_gray, test_gray, templ_gray.size());
    }

    // 可选预处理
    if (params_.blur_kernel_size > 1) {
        int ksize = params_.blur_kernel_size;
        if (ksize % 2 == 0) ksize++;
        cv::GaussianBlur(test_gray, test_gray, cv::Size(ksize, ksize), 0);
        cv::GaussianBlur(templ_gray, templ_gray, cv::Size(ksize, ksize), 0);
    }

    cv::Mat diff;
    cv::absdiff(test_gray, templ_gray, diff);
    return diff;
}

cv::Mat DefectAnalyzer::getBinaryDifference(const cv::Mat& diff_img) {
    cv::Mat binary;
    cv::threshold(diff_img, binary, params_.binary_threshold, 255, cv::THRESH_BINARY);
    return applyMorphology(binary);
}

void DefectAnalyzer::setMinDefectSize(int size) {
    params_.min_defect_size = size;
}

void DefectAnalyzer::setBinaryThreshold(int threshold) {
    params_.binary_threshold = std::clamp(threshold, 0, 255);
}

std::vector<Defect> DefectAnalyzer::extractDefectsFromBinary(const cv::Mat& binary_img,
                                                              const std::string& type) {
    std::vector<Defect> defects;

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary_img.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 调试统计
    int filtered_by_size = 0;
    double max_area_found = 0;

    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        max_area_found = std::max(max_area_found, area);

        // 过滤小于最小尺寸的区域
        if (area >= params_.min_defect_size) {
            Defect defect;
            cv::Rect bbox = cv::boundingRect(contour);
            defect.location = Rect::fromCvRect(bbox);
            defect.area = area;
            defect.type = type;
            defect.severity = std::min(1.0f, static_cast<float>(area) / 1000.0f);

            // 设置瑕疵中心点坐标
            defect.x = static_cast<float>(bbox.x + bbox.width / 2);
            defect.y = static_cast<float>(bbox.y + bbox.height / 2);

            // 设置瑕疵类型枚举
            if (type == "ink_missing") {
                defect.defect_type = DefectType::INK_MISSING;
            } else if (type == "spray_missing") {
                defect.defect_type = DefectType::SPRAY_MISSING;
            } else if (type == "mixed_defect") {
                defect.defect_type = DefectType::MIXED_DEFECT;
            } else {
                defect.defect_type = DefectType::UNKNOWN;
            }

            defects.push_back(defect);
        } else {
            filtered_by_size++;
        }
    }

    // 调试输出
    if (!defects.empty() || max_area_found > 500) {
        std::cout << "    [extractDefects] 类型=" << type 
                  << ", 轮廓总数=" << contours.size() 
                  << ", 通过=" << defects.size() 
                  << ", 过滤(" << params_.min_defect_size << ")=" << filtered_by_size
                  << ", 最大面积=" << max_area_found << "\n";
    }

    return defects;
}

cv::Mat DefectAnalyzer::applyMorphology(const cv::Mat& binary_img) {
    cv::Mat result = binary_img.clone();

    int ksize = params_.morphology_kernel_size;
    if (ksize < 1) ksize = 3;

    // 限制核大小不超过5，避免过度处理
    ksize = std::min(ksize, 5);
    
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(ksize, ksize));

    // 先做闭运算连接断裂的区域（保护瑕疵完整性）
    cv::morphologyEx(result, result, cv::MORPH_CLOSE, kernel);
    
    // 再做小核开运算去除孤立噪点（核大小减半，保护小瑕疵）
    if (ksize > 1) {
        cv::Mat small_kernel = cv::getStructuringElement(
            cv::MORPH_ELLIPSE, cv::Size(ksize/2 + 1, ksize/2 + 1));
        cv::morphologyEx(result, result, cv::MORPH_OPEN, small_kernel);
    }

    return result;
}

float DefectAnalyzer::computeSeverity(const Defect& defect, const cv::Mat& diff_img) {
    cv::Rect roi = defect.location.toCvRect();

    // 确保ROI在图像范围内
    roi.x = std::max(0, roi.x);
    roi.y = std::max(0, roi.y);
    roi.width = std::min(roi.width, diff_img.cols - roi.x);
    roi.height = std::min(roi.height, diff_img.rows - roi.y);

    if (roi.width <= 0 || roi.height <= 0) {
        return 0.0f;
    }

    cv::Mat region = diff_img(roi);
    cv::Scalar mean_val = cv::mean(region);

    // 基于面积和强度计算严重程度
    float area_factor = std::min(1.0f, static_cast<float>(defect.area) / 500.0f);
    float intensity_factor = static_cast<float>(mean_val[0]) / 255.0f;

    return std::min(1.0f, area_factor * 0.5f + intensity_factor * 0.5f);
}

} // namespace defect_detection

