/**
 * @file defect_detector.cpp
 * @brief 瑕疵检测器主类实现
 */

#include "defect_detector.h"
#include <chrono>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace defect_detection {

DefectDetector::DefectDetector()
    : auto_localization_(false)
    , template_loaded_(false)
    , alignment_mode_(AlignmentMode::ROI_ONLY)  // 默认使用ROI校正模式（性能优先）
{
    // 初始化模块关联
    template_learner_.setROIManager(&roi_manager_);
    template_learner_.setLocalization(&localization_);
}

DefectDetector::~DefectDetector() {
}

// ==================== 模板管理 ====================

bool DefectDetector::importTemplate(const cv::Mat& image) {
    if (image.empty()) {
        return false;
    }
    
    template_image_ = image.clone();
    original_template_ = image.clone();
    template_learner_.setOriginalTemplate(original_template_);
    
    // 提取所有ROI的模板
    roi_manager_.extractTemplates(template_image_);
    
    template_loaded_ = true;
    return true;
}

bool DefectDetector::importTemplateFromFile(const std::string& filepath) {
    cv::Mat image = cv::imread(filepath);
    if (image.empty()) {
        return false;
    }
    return importTemplate(image);
}

const cv::Mat& DefectDetector::getTemplate() const {
    return template_image_;
}

// ==================== ROI管理 ====================

int DefectDetector::addROI(int x, int y, int width, int height, float threshold) {
    int id = roi_manager_.addROI(Rect(x, y, width, height), threshold, global_params_);
    
    // 如果已加载模板，立即提取该ROI的模板图像
    if (template_loaded_) {
        ROIRegion* roi = roi_manager_.getROI(id);
        if (roi) {
            cv::Rect cv_rect(x, y, width, height);
            cv_rect.x = std::max(0, cv_rect.x);
            cv_rect.y = std::max(0, cv_rect.y);
            cv_rect.width = std::min(cv_rect.width, template_image_.cols - cv_rect.x);
            cv_rect.height = std::min(cv_rect.height, template_image_.rows - cv_rect.y);
            
            if (cv_rect.width > 0 && cv_rect.height > 0) {
                roi->template_image = template_image_(cv_rect).clone();
            }
        }
    }
    
    return id;
}

bool DefectDetector::removeROI(int roi_id) {
    return roi_manager_.removeROI(roi_id);
}

bool DefectDetector::setROIThreshold(int roi_id, float threshold) {
    return roi_manager_.setThreshold(roi_id, threshold);
}

void DefectDetector::clearROIs() {
    roi_manager_.clear();
}

size_t DefectDetector::getROICount() const {
    return roi_manager_.getROICount();
}

ROIRegion DefectDetector::getROI(int index) const {
    const auto& rois = roi_manager_.getAllROIs();
    if (index >= 0 && index < static_cast<int>(rois.size())) {
        return rois[index];
    }
    return ROIRegion();
}

// ==================== 定位设置 ====================

void DefectDetector::setLocalizationPoints(const std::vector<Point>& points) {
    localization_.setReferencePoints(points);
}

void DefectDetector::enableAutoLocalization(bool enable) {
    auto_localization_ = enable;
}

void DefectDetector::setAlignmentMode(AlignmentMode mode) {
    alignment_mode_ = mode;
}

AlignmentMode DefectDetector::getAlignmentMode() const {
    return alignment_mode_;
}

void DefectDetector::setExternalTransform(float offset_x, float offset_y, float rotation) {
    external_transform_.offset_x = offset_x;
    external_transform_.offset_y = offset_y;
    external_transform_.rotation = rotation;
    external_transform_.is_set = true;
}

void DefectDetector::clearExternalTransform() {
    external_transform_ = ExternalTransform();
}

ExternalTransform DefectDetector::getExternalTransform() const {
    return external_transform_;
}

LocalizationInfo DefectDetector::buildLocalizationFromExternal(const ExternalTransform& ext,
                                                                const cv::Size& image_size) {
    LocalizationInfo info;

    if (!ext.is_set) {
        info.success = false;
        return info;
    }

    info.offset_x = ext.offset_x;
    info.offset_y = ext.offset_y;
    info.rotation_angle = ext.rotation;
    info.scale = 1.0f;
    info.from_external = true;

    // 构建仿射变换矩阵
    // 变换顺序: 先旋转，再平移
    float angle_rad = ext.rotation * static_cast<float>(CV_PI / 180.0);
    float cos_a = std::cos(angle_rad);
    float sin_a = std::sin(angle_rad);

    // 以图像中心为旋转中心
    float cx = image_size.width / 2.0f;
    float cy = image_size.height / 2.0f;

    // 构建仿射变换矩阵 (2x3)
    // 变换公式:
    // x' = cos(a) * (x - cx) - sin(a) * (y - cy) + cx + tx
    // y' = sin(a) * (x - cx) + cos(a) * (y - cy) + cy + ty
    cv::Mat transform = cv::Mat::zeros(2, 3, CV_64F);
    transform.at<double>(0, 0) = cos_a;
    transform.at<double>(0, 1) = -sin_a;
    transform.at<double>(0, 2) = (1 - cos_a) * cx + sin_a * cy + ext.offset_x;
    transform.at<double>(1, 0) = sin_a;
    transform.at<double>(1, 1) = cos_a;
    transform.at<double>(1, 2) = -sin_a * cx + (1 - cos_a) * cy + ext.offset_y;

    info.transform_matrix = transform;
    info.success = true;
    info.match_quality = 1.0f;  // 外部传入的变换质量设为最高
    info.inlier_count = 0;

    return info;
}

// ==================== 检测功能 ====================

DetectionResult DefectDetector::detect(const cv::Mat& test_image) {
    DetectionResult result;
    result.localization_time_ms = 0.0;
    result.roi_comparison_time_ms = 0.0;

    auto start_time = std::chrono::high_resolution_clock::now();

    if (test_image.empty()) {
        result.overall_passed = false;
        return result;
    }

    // Step 1: 图像预处理
    cv::Mat processed_image = preprocessImage(test_image);

    // Step 2: 计算定位信息
    // 优先使用外部传入的变换参数，否则使用自动定位
    auto loc_start = std::chrono::high_resolution_clock::now();

    bool use_alignment = false;

    if (external_transform_.is_set) {
        // 使用外部传入的变换参数（优先级最高）
        result.localization = buildLocalizationFromExternal(external_transform_,
                                                             test_image.size());
        use_alignment = result.localization.success &&
                        alignment_mode_ != AlignmentMode::NONE;
    }
    else if (auto_localization_ && template_loaded_ &&
             alignment_mode_ != AlignmentMode::NONE) {
        // 使用自动定位（保留兼容性）
        result.localization = localization_.autoDetectLocalization(template_image_,
                                                                    processed_image);
        use_alignment = result.localization.success;
    }

    auto loc_end = std::chrono::high_resolution_clock::now();
    result.localization_time_ms = std::chrono::duration<double, std::milli>(loc_end - loc_start).count();

    // Step 3: 根据对齐模式处理ROI
    auto roi_start = std::chrono::high_resolution_clock::now();

    const auto& rois = roi_manager_.getAllROIs();
    result.roi_results.resize(rois.size());
    result.overall_passed = true;
    result.total_defect_count = 0;

    if (use_alignment && alignment_mode_ == AlignmentMode::FULL_IMAGE) {
        // 整图校正模式：先对整张图应用变换，再提取ROI
        cv::Mat aligned_image = localization_.applyCorrection(processed_image, result.localization);

        #ifdef _OPENMP
        #pragma omp parallel for
        #endif
        for (int i = 0; i < static_cast<int>(rois.size()); ++i) {
            result.roi_results[i] = processROI(rois[i], aligned_image);
        }
    }
    else if (use_alignment && alignment_mode_ == AlignmentMode::ROI_ONLY) {
        // ROI区域校正模式：只对每个ROI区域单独应用变换（性能优化）
        #ifdef _OPENMP
        #pragma omp parallel for
        #endif
        for (int i = 0; i < static_cast<int>(rois.size()); ++i) {
            result.roi_results[i] = processROIWithAlignment(rois[i], processed_image,
                                                             result.localization);
        }
    }
    else {
        // 无对齐模式：直接处理ROI
        #ifdef _OPENMP
        #pragma omp parallel for
        #endif
        for (int i = 0; i < static_cast<int>(rois.size()); ++i) {
            result.roi_results[i] = processROI(rois[i], processed_image);
        }
    }

    auto roi_end = std::chrono::high_resolution_clock::now();
    result.roi_comparison_time_ms = std::chrono::duration<double, std::milli>(roi_end - roi_start).count();

    // 汇总结果
    for (const auto& roi_result : result.roi_results) {
        if (!roi_result.passed) {
            result.overall_passed = false;
        }
        result.total_defect_count += static_cast<int>(roi_result.defects.size());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.processing_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return result;
}

DetectionResult DefectDetector::detectFromFile(const std::string& filepath) {
    cv::Mat image = cv::imread(filepath);
    return detect(image);
}

cv::Mat DefectDetector::detectAndVisualize(const cv::Mat& test_image, DetectionResult& result) {
    result = detect(test_image);

    cv::Mat visualization = test_image.clone();

    // 绘制ROI和瑕疵
    const auto& rois = roi_manager_.getAllROIs();

    for (size_t i = 0; i < rois.size() && i < result.roi_results.size(); ++i) {
        const auto& roi = rois[i];
        const auto& roi_result = result.roi_results[i];

        // 绘制ROI边界
        cv::Scalar color = roi_result.passed ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
        cv::rectangle(visualization, roi.bounds.toCvRect(), color, 2);

        // 绘制相似度
        std::string text = "ROI" + std::to_string(roi.id) + ": " +
                          std::to_string(static_cast<int>(roi_result.similarity * 100)) + "%";
        cv::putText(visualization, text,
                    cv::Point(roi.bounds.x, roi.bounds.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);

        // 绘制瑕疵
        for (const auto& defect : roi_result.defects) {
            cv::Rect defect_rect = defect.location.toCvRect();
            defect_rect.x += roi.bounds.x;
            defect_rect.y += roi.bounds.y;
            cv::rectangle(visualization, defect_rect, cv::Scalar(255, 0, 255), 1);
        }
    }

    return visualization;
}

// ==================== 一键学习 ====================

LearningResult DefectDetector::learn(const cv::Mat& new_image) {
    LearningResult result = template_learner_.learnFromImage(new_image);

    if (result.success) {
        // 更新主模板
        template_image_ = new_image.clone();
    }

    return result;
}

void DefectDetector::resetTemplate() {
    if (!original_template_.empty()) {
        template_image_ = original_template_.clone();
        roi_manager_.extractTemplates(template_image_);
        template_learner_.reset();
    }
}

// ==================== 参数配置 ====================

void DefectDetector::setGlobalParams(const DetectionParams& params) {
    global_params_ = params;
    defect_analyzer_.setParams(params);

    // 更新所有ROI的参数
    for (auto& roi : roi_manager_.getAllROIs()) {
        roi.params = params;
    }
}

const DetectionParams& DefectDetector::getGlobalParams() const {
    return global_params_;
}

bool DefectDetector::setParameter(const std::string& name, float value) {
    if (name == "blur_kernel_size") {
        global_params_.blur_kernel_size = static_cast<int>(value);
    } else if (name == "binary_threshold") {
        global_params_.binary_threshold = static_cast<int>(value);
    } else if (name == "min_defect_size") {
        global_params_.min_defect_size = static_cast<int>(value);
    } else if (name == "similarity_threshold") {
        global_params_.similarity_threshold = value;
    } else if (name == "morphology_kernel_size") {
        global_params_.morphology_kernel_size = static_cast<int>(value);
    } else if (name == "detect_black_on_white") {
        global_params_.detect_black_on_white = value > 0.5f;
    } else if (name == "detect_white_on_black") {
        global_params_.detect_white_on_black = value > 0.5f;
    } else {
        return false;
    }

    defect_analyzer_.setParams(global_params_);
    
    // [FIX] 同步更新所有已创建ROI的参数，确保setParameter对已存在的ROI也生效
    for (auto& roi : roi_manager_.getAllROIs()) {
        if (name == "blur_kernel_size") {
            roi.params.blur_kernel_size = global_params_.blur_kernel_size;
        } else if (name == "binary_threshold") {
            roi.params.binary_threshold = global_params_.binary_threshold;
        } else if (name == "min_defect_size") {
            roi.params.min_defect_size = global_params_.min_defect_size;
        } else if (name == "similarity_threshold") {
            roi.params.similarity_threshold = global_params_.similarity_threshold;
        } else if (name == "morphology_kernel_size") {
            roi.params.morphology_kernel_size = global_params_.morphology_kernel_size;
        } else if (name == "detect_black_on_white") {
            roi.params.detect_black_on_white = global_params_.detect_black_on_white;
        } else if (name == "detect_white_on_black") {
            roi.params.detect_white_on_black = global_params_.detect_white_on_black;
        }
    }
    
    return true;
}

float DefectDetector::getParameter(const std::string& name) const {
    if (name == "blur_kernel_size") {
        return static_cast<float>(global_params_.blur_kernel_size);
    } else if (name == "binary_threshold") {
        return static_cast<float>(global_params_.binary_threshold);
    } else if (name == "min_defect_size") {
        return static_cast<float>(global_params_.min_defect_size);
    } else if (name == "similarity_threshold") {
        return global_params_.similarity_threshold;
    } else if (name == "morphology_kernel_size") {
        return static_cast<float>(global_params_.morphology_kernel_size);
    } else if (name == "detect_black_on_white") {
        return global_params_.detect_black_on_white ? 1.0f : 0.0f;
    } else if (name == "detect_white_on_black") {
        return global_params_.detect_white_on_black ? 1.0f : 0.0f;
    }
    return 0.0f;
}

void DefectDetector::setBinaryDetectionParams(const BinaryDetectionParams& params) {
    template_matcher_.setBinaryDetectionParams(params);
}

const BinaryDetectionParams& DefectDetector::getBinaryDetectionParams() const {
    return template_matcher_.getBinaryDetectionParams();
}

// ==================== 内部处理函数 ====================

ROIResult DefectDetector::processROI(const ROIRegion& roi, const cv::Mat& test_image) {
    ROIResult result;
    result.roi_id = roi.id;
    result.threshold = roi.similarity_threshold;

    // 提取ROI区域
    cv::Rect cv_rect = roi.bounds.toCvRect();
    cv_rect.x = std::max(0, cv_rect.x);
    cv_rect.y = std::max(0, cv_rect.y);
    cv_rect.width = std::min(cv_rect.width, test_image.cols - cv_rect.x);
    cv_rect.height = std::min(cv_rect.height, test_image.rows - cv_rect.y);

    if (cv_rect.width <= 0 || cv_rect.height <= 0 || roi.template_image.empty()) {
        result.similarity = 0.0f;
        result.passed = false;
        return result;
    }

    cv::Mat test_roi = test_image(cv_rect);

    // 计算相似度
    result.similarity = template_matcher_.computeSimilarity(roi.template_image, test_roi);

    // 判断是否通过
    result.passed = (result.similarity >= roi.similarity_threshold);

    // 检测瑕疵
    DefectAnalyzer analyzer;
    analyzer.setParams(roi.params);
    result.defects = analyzer.detectAllDefects(test_roi, roi.template_image);

    // 应用边缘容错：仅过滤掉位于ROI外边界附近的小瑕疵（对齐误差主要影响外边界）
    const auto& binary_params = template_matcher_.getBinaryDetectionParams();
    if (binary_params.enabled && binary_params.edge_tolerance_pixels > 0 && !result.defects.empty()) {
        int tolerance = binary_params.edge_tolerance_pixels;
        
        // 创建ROI外边界容错掩码（只针对外边界，不针对内部边缘）
        cv::Mat edge_mask = cv::Mat::zeros(roi.template_image.rows, roi.template_image.cols, CV_8UC1);
        
        // 标记ROI外边界区域
        cv::Rect roi_rect(0, 0, roi.template_image.cols, roi.template_image.rows);
        cv::rectangle(edge_mask, roi_rect, cv::Scalar(255), tolerance);
        
        // 仅过滤位于ROI外边界区域且面积较小（<300）的瑕疵
        std::vector<Defect> filtered_defects;
        int filtered_count = 0;
        for (const auto& defect : result.defects) {
            // 计算瑕疵区域与边缘容错区的重叠
            cv::Rect defect_rect = defect.location.toCvRect();
            
            // 检查瑕疵区域的四个角点和中心点
            std::vector<cv::Point> check_points = {
                cv::Point(defect_rect.x, defect_rect.y),  // 左上
                cv::Point(defect_rect.x + defect_rect.width/2, defect_rect.y + defect_rect.height/2),  // 中心
                cv::Point(defect_rect.x + defect_rect.width, defect_rect.y + defect_rect.height),  // 右下
                cv::Point(defect_rect.x, defect_rect.y + defect_rect.height),  // 左下
                cv::Point(defect_rect.x + defect_rect.width, defect_rect.y)   // 右上
            };
            
            int points_on_edge = 0;
            for (const auto& pt : check_points) {
                if (pt.x >= 0 && pt.x < edge_mask.cols && pt.y >= 0 && pt.y < edge_mask.rows) {
                    if (edge_mask.at<uchar>(pt.y, pt.x) != 0) {
                        points_on_edge++;
                    }
                }
            }
            
            // 如果超过60%的检查点在边缘区，认为是边缘瑕疵
            bool is_on_edge = (points_on_edge >= 3);
            
            // 保留条件：
            // 1. 不在边缘 或 
            // 2. 面积大（>=300） 或 
            // 3. 只有少量点在边缘（<3个）
            if (!is_on_edge || defect.area >= 300) {
                filtered_defects.push_back(defect);
            } else {
                filtered_count++;
            }
        }
        
        if (filtered_count > 0) {
            std::cout << "    [边缘容错] 过滤 " << filtered_count 
                      << " 个ROI边界小瑕疵（面积<300，容错区=" << tolerance << "px）\n";
        }
        
        result.defects = filtered_defects;
    }

    // 如果有瑕疵（过滤后），标记为不通过
    if (!result.defects.empty()) {
        result.passed = false;
    }

    return result;
}

cv::Mat DefectDetector::preprocessImage(const cv::Mat& image) {
    cv::Mat result = image.clone();

    // 应用高斯滤波
    if (global_params_.blur_kernel_size > 1) {
        int ksize = global_params_.blur_kernel_size;
        if (ksize % 2 == 0) ksize++;
        cv::GaussianBlur(result, result, cv::Size(ksize, ksize), 0);
    }

    return result;
}

ROIResult DefectDetector::processROIWithAlignment(const ROIRegion& roi, const cv::Mat& test_image,
                                                   const LocalizationInfo& loc_info) {
    ROIResult result;
    result.roi_id = roi.id;
    result.threshold = roi.similarity_threshold;

    if (roi.template_image.empty()) {
        result.similarity = 0.0f;
        result.passed = false;
        return result;
    }

    // 提取对齐后的ROI区域
    cv::Mat aligned_roi = extractAlignedROI(test_image, roi.bounds, loc_info);

    if (aligned_roi.empty()) {
        result.similarity = 0.0f;
        result.passed = false;
        return result;
    }

    // 计算相似度
    result.similarity = template_matcher_.computeSimilarity(roi.template_image, aligned_roi);

    // 判断是否通过
    result.passed = (result.similarity >= roi.similarity_threshold);

    // 检测瑕疵
    DefectAnalyzer analyzer;
    analyzer.setParams(roi.params);
    result.defects = analyzer.detectAllDefects(aligned_roi, roi.template_image);

    // 应用边缘容错：基于模板图像内容边缘的智能过滤
    // 解决对齐误差导致的黑色内容边缘误报问题
    const auto& binary_params = template_matcher_.getBinaryDetectionParams();
    if (binary_params.enabled && binary_params.edge_tolerance_pixels > 0 && !result.defects.empty()) {
        int tolerance = binary_params.edge_tolerance_pixels;
        
        // 步骤1: 提取模板图像的黑色内容边缘
        cv::Mat templ_gray, templ_binary, templ_edges, edge_tolerance_mask;
        if (roi.template_image.channels() == 3) {
            cv::cvtColor(roi.template_image, templ_gray, cv::COLOR_BGR2GRAY);
        } else {
            templ_gray = roi.template_image.clone();
        }
        
        // 二值化模板（提取黑色内容）
        cv::threshold(templ_gray, templ_binary, binary_params.binary_threshold, 255, cv::THRESH_BINARY_INV);
        
        // 提取边缘（Canny边缘检测）
        cv::Canny(templ_binary, templ_edges, 50, 150);
        
        // 膨胀边缘创建容错区域
        int kernel_size = tolerance * 2 + 1;
        cv::Mat dilate_kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, 
                                                          cv::Size(kernel_size, kernel_size));
        cv::dilate(templ_edges, edge_tolerance_mask, dilate_kernel);
        
        // 步骤2: 智能过滤边缘误报瑕疵
        std::vector<Defect> filtered_defects;
        int filtered_count = 0;
        
        for (const auto& defect : result.defects) {
            cv::Rect defect_rect = defect.location.toCvRect();
            cv::Point defect_center(defect_rect.x + defect_rect.width / 2,
                                   defect_rect.y + defect_rect.height / 2);
            
            // 检查瑕疵中心是否在模板边缘容错区内
            bool center_in_tolerance_zone = false;
            if (defect_center.x >= 0 && defect_center.x < edge_tolerance_mask.cols &&
                defect_center.y >= 0 && defect_center.y < edge_tolerance_mask.rows) {
                center_in_tolerance_zone = (edge_tolerance_mask.at<uchar>(defect_center.y, defect_center.x) > 0);
            }
            
            // 计算瑕疵区域与边缘容错区的重叠比例
            int overlap_pixels = 0;
            int total_pixels = 0;
            for (int y = defect_rect.y; y < defect_rect.y + defect_rect.height; ++y) {
                for (int x = defect_rect.x; x < defect_rect.x + defect_rect.width; ++x) {
                    if (x >= 0 && x < edge_tolerance_mask.cols && y >= 0 && y < edge_tolerance_mask.rows) {
                        total_pixels++;
                        if (edge_tolerance_mask.at<uchar>(y, x) > 0) {
                            overlap_pixels++;
                        }
                    }
                }
            }
            float overlap_ratio = (total_pixels > 0) ? 
                                 static_cast<float>(overlap_pixels) / total_pixels : 0;
            
            // 判断是否为边缘误报（满足以下条件）：
            // 1. 瑕疵中心在模板边缘容错区内
            // 2. 大部分区域（>60%）与边缘容错区重叠
            // 3. 呈现细长条状（长宽比 > 3）或面积较小（< edge_defect_size_threshold）
            bool is_edge_false_positive = false;
            if (center_in_tolerance_zone && overlap_ratio > 0.6f) {
                float aspect_ratio = (defect_rect.height > 0) ? 
                                    static_cast<float>(defect_rect.width) / defect_rect.height : 1.0f;
                if (aspect_ratio > 3.0f || aspect_ratio < 0.33f) {
                    is_edge_false_positive = true;  // 细长条状
                } else if (defect.area < binary_params.edge_defect_size_threshold) {
                    is_edge_false_positive = true;  // 小面积瑕疵
                }
            }
            
            // 保留条件：
            // 1. 不是边缘误报
            // 2. 虽然靠近边缘但面积足够大（>= edge_defect_size_threshold * 1.5）
            // 3. 虽然靠近边缘但形状不是细长条（长宽比接近1）
            bool should_keep = false;
            if (!is_edge_false_positive) {
                should_keep = true;
            } else if (defect.area >= binary_params.edge_defect_size_threshold * 1.5f) {
                should_keep = true;  // 大瑕疵即使靠近边缘也保留
            } else if (center_in_tolerance_zone && overlap_ratio < 0.3f) {
                should_keep = true;  // 中心在容错区但大部分区域在外面
            }
            
            if (should_keep) {
                filtered_defects.push_back(defect);
            } else {
                filtered_count++;
                if (result.roi_id == 0) {
                    float aspect_ratio = (defect_rect.height > 0) ? 
                                        static_cast<float>(defect_rect.width) / defect_rect.height : 1.0f;
                    std::cout << "    [内容边缘容错] 过滤: 面积=" << defect.area 
                              << " 长宽比=" << std::fixed << std::setprecision(1) << aspect_ratio
                              << " 边缘重叠=" << std::setprecision(0) << (overlap_ratio * 100) << "%\n";
                }
            }
        }
        
        if (filtered_count > 0) {
            std::cout << "    [内容边缘容错] 过滤 " << filtered_count 
                      << " 个边缘误报（模板黑色内容边缘 " << tolerance << "px 容错区）\n";
        }
        
        result.defects = filtered_defects;
    }

    // 如果有瑕疵（过滤后），标记为不通过
    if (!result.defects.empty()) {
        result.passed = false;
    }

    return result;
}

cv::Mat DefectDetector::extractAlignedROI(const cv::Mat& image, const Rect& roi_bounds,
                                           const LocalizationInfo& loc_info) {
    if (image.empty() || loc_info.transform_matrix.empty()) {
        // 无变换信息，直接提取ROI
        cv::Rect cv_rect = roi_bounds.toCvRect();
        cv_rect.x = std::max(0, cv_rect.x);
        cv_rect.y = std::max(0, cv_rect.y);
        cv_rect.width = std::min(cv_rect.width, image.cols - cv_rect.x);
        cv_rect.height = std::min(cv_rect.height, image.rows - cv_rect.y);
        if (cv_rect.width <= 0 || cv_rect.height <= 0) {
            return cv::Mat();
        }
        return image(cv_rect).clone();
    }

    // 计算ROI四个角点在模板坐标系中的位置
    std::vector<cv::Point2f> template_corners = {
        cv::Point2f((float)roi_bounds.x, (float)roi_bounds.y),
        cv::Point2f((float)(roi_bounds.x + roi_bounds.width), (float)roi_bounds.y),
        cv::Point2f((float)(roi_bounds.x + roi_bounds.width), (float)(roi_bounds.y + roi_bounds.height)),
        cv::Point2f((float)roi_bounds.x, (float)(roi_bounds.y + roi_bounds.height))
    };

    // 使用变换矩阵将模板ROI角点映射到测试图像坐标系
    // transform_matrix: 模板 -> 测试图
    cv::Mat transform = loc_info.transform_matrix;
    if (transform.type() != CV_64F) {
        transform.convertTo(transform, CV_64F);
    }

    std::vector<cv::Point2f> test_corners;
    cv::transform(template_corners, test_corners, transform);

    // 计算变换后区域的边界框
    float min_x = test_corners[0].x, max_x = test_corners[0].x;
    float min_y = test_corners[0].y, max_y = test_corners[0].y;
    for (const auto& pt : test_corners) {
        min_x = std::min(min_x, pt.x);
        max_x = std::max(max_x, pt.x);
        min_y = std::min(min_y, pt.y);
        max_y = std::max(max_y, pt.y);
    }

    // 扩展边界以容纳整个变换区域（增加边距以保证完整性）
    int expand = 5;  // 增加扩展像素数以确保完整覆盖
    int src_x = static_cast<int>(std::floor(min_x)) - expand;
    int src_y = static_cast<int>(std::floor(min_y)) - expand;
    int src_w = static_cast<int>(std::ceil(max_x - min_x)) + 2 * expand;
    int src_h = static_cast<int>(std::ceil(max_y - min_y)) + 2 * expand;

    // 检查变换后的区域是否在图像边界内
    // 如果大部分区域在边界外，返回空结果
    if (src_x + src_w < 0 || src_y + src_h < 0 ||
        src_x >= image.cols || src_y >= image.rows) {
        return cv::Mat();
    }

    // 裁剪到图像边界
    int crop_x = std::max(0, src_x);
    int crop_y = std::max(0, src_y);
    int crop_w = std::min(src_w - (crop_x - src_x), image.cols - crop_x);
    int crop_h = std::min(src_h - (crop_y - src_y), image.rows - crop_y);

    // 检查裁剪后的区域是否足够大
    if (crop_w < roi_bounds.width / 2 || crop_h < roi_bounds.height / 2) {
        return cv::Mat();  // 区域太小，无法进行可靠的变换
    }

    // 提取扩展的源区域
    cv::Mat src_region = image(cv::Rect(crop_x, crop_y, crop_w, crop_h));

    // 计算从源区域到目标ROI的变换
    // 目标是一个与模板ROI同尺寸的区域
    int dst_w = roi_bounds.width;
    int dst_h = roi_bounds.height;

    // 定义目标角点（输出图像的角点）
    std::vector<cv::Point2f> dst_corners = {
        cv::Point2f(0, 0),
        cv::Point2f((float)dst_w, 0),
        cv::Point2f((float)dst_w, (float)dst_h),
        cv::Point2f(0, (float)dst_h)
    };

    // 调整测试角点到裁剪区域的局部坐标
    std::vector<cv::Point2f> local_corners;
    for (const auto& pt : test_corners) {
        local_corners.push_back(cv::Point2f(pt.x - crop_x, pt.y - crop_y));
    }

    // 检查局部角点是否有效（全部非负且在合理范围内）
    bool valid_corners = true;
    for (const auto& pt : local_corners) {
        if (pt.x < -expand || pt.y < -expand ||
            pt.x > crop_w + expand || pt.y > crop_h + expand) {
            valid_corners = false;
            break;
        }
    }

    if (!valid_corners) {
        // 角点超出预期范围，使用简单的仿射变换替代
        cv::Mat affine = cv::getAffineTransform(
            std::vector<cv::Point2f>{local_corners[0], local_corners[1], local_corners[2]}.data(),
            std::vector<cv::Point2f>{dst_corners[0], dst_corners[1], dst_corners[2]}.data()
        );
        cv::Mat aligned_roi;
        cv::warpAffine(src_region, aligned_roi, affine, cv::Size(dst_w, dst_h),
                       cv::INTER_LINEAR, cv::BORDER_REFLECT_101);
        return aligned_roi;
    }

    // 计算透视变换（从测试图像区域到对齐的ROI）
    cv::Mat perspective = cv::getPerspectiveTransform(local_corners, dst_corners);

    // 应用透视变换 - 使用BORDER_REFLECT_101减少边缘伪影
    cv::Mat aligned_roi;
    cv::warpPerspective(src_region, aligned_roi, perspective, cv::Size(dst_w, dst_h),
                        cv::INTER_LINEAR, cv::BORDER_REFLECT_101);

    // 移除内缩操作，因为：
    // 1. BORDER_REFLECT_101已经减少了边缘伪影
    // 2. 内缩+resize会引入额外的插值误差，降低相似度
    // 3. 原始尺寸的对齐更准确

    return aligned_roi;
}

} // namespace defect_detection

