/**
 * @file c_api.cpp
 * @brief C语言接口实现
 */

#define DEFECT_DETECTION_EXPORTS
#include "c_api.h"
#include "defect_detector.h"
#include "template_learner.h"
#include <opencv2/opencv.hpp>
#include <cstring>

using namespace defect_detection;

// 保存最后一次检测的处理时间
static double g_last_processing_time = 0.0;
static double g_last_localization_time = 0.0;
static double g_last_roi_comparison_time = 0.0;

// 保存最后一次检测的结果用于GetDefectDetails和其他查询
static DetectionResult g_last_detection_result;

// 库版本信息
static const char* g_library_version = "1.0.0";
static const char* g_build_info = "Built with OpenCV " CV_VERSION;

// 辅助函数：将C++的AlignmentMode转换为C的int
static int alignmentModeToCInt(AlignmentMode mode) {
    switch (mode) {
        case AlignmentMode::NONE: return ALIGNMENT_NONE;
        case AlignmentMode::FULL_IMAGE: return ALIGNMENT_FULL_IMAGE;
        case AlignmentMode::ROI_ONLY: return ALIGNMENT_ROI_ONLY;
        default: return ALIGNMENT_NONE;
    }
}

// 辅助函数：将C的int转换为C++的AlignmentMode
static AlignmentMode alignmentModeFromCInt(int mode) {
    switch (mode) {
        case ALIGNMENT_NONE: return AlignmentMode::NONE;
        case ALIGNMENT_FULL_IMAGE: return AlignmentMode::FULL_IMAGE;
        case ALIGNMENT_ROI_ONLY: return AlignmentMode::ROI_ONLY;
        default: return AlignmentMode::NONE;
    }
}

// 辅助函数：将C++的MatchMethod转换为C的int
static int matchMethodToCInt(MatchMethod method) {
    switch (method) {
        case MatchMethod::NCC: return MATCH_METHOD_NCC;
        case MatchMethod::SSIM: return MATCH_METHOD_SSIM;
        case MatchMethod::NCC_SSIM: return MATCH_METHOD_NCC_SSIM;
        case MatchMethod::BINARY: return MATCH_METHOD_BINARY;
        default: return MATCH_METHOD_NCC;
    }
}

// 辅助函数：将C的int转换为C++的MatchMethod
static MatchMethod matchMethodFromCInt(int method) {
    switch (method) {
        case MATCH_METHOD_NCC: return MatchMethod::NCC;
        case MATCH_METHOD_SSIM: return MatchMethod::SSIM;
        case MATCH_METHOD_NCC_SSIM: return MatchMethod::NCC_SSIM;
        case MATCH_METHOD_BINARY: return MatchMethod::BINARY;
        default: return MatchMethod::NCC;
    }
}

// 辅助函数：将C++的BinaryDetectionParams转换为C结构
static void binaryParamsToCStruct(const BinaryDetectionParams& cpp_params, BinaryDetectionParamsC* c_params) {
    c_params->enabled = cpp_params.enabled ? 1 : 0;
    c_params->auto_detect_binary = cpp_params.auto_detect_binary ? 1 : 0;
    c_params->noise_filter_size = cpp_params.noise_filter_size;
    c_params->edge_tolerance_pixels = cpp_params.edge_tolerance_pixels;
    c_params->edge_diff_ignore_ratio = cpp_params.edge_diff_ignore_ratio;
    c_params->min_significant_area = cpp_params.min_significant_area;
    c_params->area_diff_threshold = cpp_params.area_diff_threshold;
    c_params->overall_similarity_threshold = cpp_params.overall_similarity_threshold;
}

// 辅助函数：将C结构转换为C++的BinaryDetectionParams
static BinaryDetectionParams binaryParamsFromCStruct(const BinaryDetectionParamsC* c_params) {
    BinaryDetectionParams cpp_params;
    cpp_params.enabled = c_params->enabled != 0;
    cpp_params.auto_detect_binary = c_params->auto_detect_binary != 0;
    cpp_params.noise_filter_size = c_params->noise_filter_size;
    cpp_params.edge_tolerance_pixels = c_params->edge_tolerance_pixels;
    cpp_params.edge_diff_ignore_ratio = c_params->edge_diff_ignore_ratio;
    cpp_params.min_significant_area = c_params->min_significant_area;
    cpp_params.area_diff_threshold = c_params->area_diff_threshold;
    cpp_params.overall_similarity_threshold = c_params->overall_similarity_threshold;
    return cpp_params;
}

// 辅助函数：将C++的LocalizationInfo转换为C结构
static void localizationInfoToCStruct(const LocalizationInfo& cpp_info, LocalizationInfoC* c_info) {
    c_info->success = cpp_info.success ? 1 : 0;
    c_info->offset_x = cpp_info.offset_x;
    c_info->offset_y = cpp_info.offset_y;
    c_info->rotation_angle = cpp_info.rotation_angle;
    c_info->scale = cpp_info.scale;
    c_info->match_quality = cpp_info.match_quality;
    c_info->inlier_count = cpp_info.inlier_count;
    c_info->from_external = cpp_info.from_external ? 1 : 0;
}

void* CreateDetector() {
    try {
        return new DefectDetector();
    } catch (...) {
        return nullptr;
    }
}

void DestroyDetector(void* detector) {
    if (detector) {
        delete static_cast<DefectDetector*>(detector);
    }
}

int ImportTemplate(void* detector, unsigned char* bitmap_data,
                   int width, int height, int channels) {
    if (!detector || !bitmap_data) {
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        
        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, bitmap_data);
        
        return det->importTemplate(image.clone()) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

int ImportTemplateFromFile(void* detector, const char* filepath) {
    if (!detector || !filepath) {
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->importTemplateFromFile(filepath) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

int AddROI(void* detector, int x, int y, int width, int height, float threshold) {
    if (!detector) {
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->addROI(x, y, width, height, threshold);
    } catch (...) {
        return -1;
    }
}

int RemoveROI(void* detector, int roi_id) {
    if (!detector) {
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->removeROI(roi_id) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

int SetROIThreshold(void* detector, int roi_id, float threshold) {
    if (!detector) {
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->setROIThreshold(roi_id, threshold) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

void ClearROIs(void* detector) {
    if (detector) {
        static_cast<DefectDetector*>(detector)->clearROIs();
    }
}

int GetROICount(void* detector) {
    if (!detector) {
        return 0;
    }
    return static_cast<int>(static_cast<DefectDetector*>(detector)->getROICount());
}

int SetLocalizationPoints(void* detector, float* points, int point_count) {
    if (!detector || !points || point_count < 3) {
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        
        std::vector<Point> pts;
        for (int i = 0; i < point_count; ++i) {
            pts.push_back(Point(points[i * 2], points[i * 2 + 1]));
        }
        
        det->setLocalizationPoints(pts);
        return 0;
    } catch (...) {
        return -1;
    }
}

void EnableAutoLocalization(void* detector, int enable) {
    if (detector) {
        static_cast<DefectDetector*>(detector)->enableAutoLocalization(enable != 0);
    }
}

int SetExternalTransform(void* detector, float offset_x, float offset_y, float rotation) {
    if (!detector) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        det->setExternalTransform(offset_x, offset_y, rotation);
        return 0;
    } catch (...) {
        return -1;
    }
}

void ClearExternalTransform(void* detector) {
    if (detector) {
        static_cast<DefectDetector*>(detector)->clearExternalTransform();
    }
}

int DetectDefects(void* detector, unsigned char* test_image,
                  int width, int height, int channels,
                  float* similarity_results, int* defect_count) {
    if (!detector || !test_image) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, test_image);

        DetectionResult result = det->detect(image);
        g_last_processing_time = result.processing_time_ms;
        g_last_localization_time = result.localization_time_ms;
        g_last_roi_comparison_time = result.roi_comparison_time_ms;
        g_last_detection_result = result;

        // 输出相似度结果
        if (similarity_results) {
            for (size_t i = 0; i < result.roi_results.size(); ++i) {
                similarity_results[i] = result.roi_results[i].similarity;
            }
        }

        // 输出瑕疵数量
        if (defect_count) {
            *defect_count = result.total_defect_count;
        }

        return result.overall_passed ? 0 : 1;
    } catch (...) {
        return -1;
    }
}

int DetectDefectsFromFile(void* detector, const char* filepath,
                          float* similarity_results, int* defect_count) {
    if (!detector || !filepath) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        DetectionResult result = det->detectFromFile(filepath);
        g_last_processing_time = result.processing_time_ms;
        g_last_localization_time = result.localization_time_ms;
        g_last_roi_comparison_time = result.roi_comparison_time_ms;
        g_last_detection_result = result;

        if (similarity_results) {
            for (size_t i = 0; i < result.roi_results.size(); ++i) {
                similarity_results[i] = result.roi_results[i].similarity;
            }
        }

        if (defect_count) {
            *defect_count = result.total_defect_count;
        }

        return result.overall_passed ? 0 : 1;
    } catch (...) {
        return -1;
    }
}

int LearnTemplate(void* detector, unsigned char* new_template,
                  int width, int height, int channels) {
    if (!detector || !new_template) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, new_template);

        LearningResult result = det->learn(image.clone());
        return result.success ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

void ResetTemplate(void* detector) {
    if (detector) {
        static_cast<DefectDetector*>(detector)->resetTemplate();
    }
}

int SetParameter(void* detector, const char* param_name, float value) {
    if (!detector || !param_name) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->setParameter(param_name, value) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

float GetParameter(void* detector, const char* param_name) {
    if (!detector || !param_name) {
        return 0.0f;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->getParameter(param_name);
    } catch (...) {
        return 0.0f;
    }
}

double GetLastProcessingTime(void* detector) {
    (void)detector;
    return g_last_processing_time;
}

int DetectDefectsEx(void* detector, unsigned char* test_image,
                    int width, int height, int channels,
                    DefectInfo* defect_infos, int max_defects,
                    int* actual_defect_count) {
    if (!detector || !test_image) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, test_image);

        DetectionResult result = det->detect(image);
        g_last_processing_time = result.processing_time_ms;
        g_last_localization_time = result.localization_time_ms;
        g_last_roi_comparison_time = result.roi_comparison_time_ms;
        g_last_detection_result = result;

        // 收集所有瑕疵信息
        int defect_idx = 0;
        for (const auto& roi_result : result.roi_results) {
            for (const auto& defect : roi_result.defects) {
                if (defect_idx < max_defects && defect_infos) {
                    DefectInfo& info = defect_infos[defect_idx];
                    info.x = defect.x;
                    info.y = defect.y;
                    info.width = static_cast<float>(defect.location.width);
                    info.height = static_cast<float>(defect.location.height);
                    info.area = static_cast<float>(defect.area);
                    info.defect_type = static_cast<int>(defect.defect_type);
                    info.severity = defect.severity;
                    info.roi_id = roi_result.roi_id;
                }
                defect_idx++;
            }
        }

        if (actual_defect_count) {
            *actual_defect_count = result.total_defect_count;
        }

        return result.overall_passed ? 0 : 1;
    } catch (...) {
        return -1;
    }
}

int DetectDefectsFromFileEx(void* detector, const char* filepath,
                            DefectInfo* defect_infos, int max_defects,
                            int* actual_defect_count) {
    if (!detector || !filepath) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        DetectionResult result = det->detectFromFile(filepath);
        g_last_processing_time = result.processing_time_ms;
        g_last_localization_time = result.localization_time_ms;
        g_last_roi_comparison_time = result.roi_comparison_time_ms;
        g_last_detection_result = result;

        // 收集所有瑕疵信息
        int defect_idx = 0;
        for (const auto& roi_result : result.roi_results) {
            for (const auto& defect : roi_result.defects) {
                if (defect_idx < max_defects && defect_infos) {
                    DefectInfo& info = defect_infos[defect_idx];
                    info.x = defect.x;
                    info.y = defect.y;
                    info.width = static_cast<float>(defect.location.width);
                    info.height = static_cast<float>(defect.location.height);
                    info.area = static_cast<float>(defect.area);
                    info.defect_type = static_cast<int>(defect.defect_type);
                    info.severity = defect.severity;
                    info.roi_id = roi_result.roi_id;
                }
                defect_idx++;
            }
        }

        if (actual_defect_count) {
            *actual_defect_count = result.total_defect_count;
        }

        return result.overall_passed ? 0 : 1;
    } catch (...) {
        return -1;
    }
}

int SetDefectThreshold(void* detector, int threshold) {
    if (!detector) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->setParameter("binary_threshold", static_cast<float>(threshold)) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

int SetMinDefectSize(void* detector, int min_size) {
    if (!detector) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->setParameter("min_defect_size", static_cast<float>(min_size)) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

// ==================== 对齐模式接口实现 ====================

int SetAlignmentMode(void* detector, int mode) {
    if (!detector) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        det->setAlignmentMode(alignmentModeFromCInt(mode));
        return 0;
    } catch (...) {
        return -1;
    }
}

int GetAlignmentMode(void* detector) {
    if (!detector) {
        return ALIGNMENT_NONE;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return alignmentModeToCInt(det->getAlignmentMode());
    } catch (...) {
        return ALIGNMENT_NONE;
    }
}

int GetExternalTransform(void* detector, float* offset_x, float* offset_y, float* rotation, int* is_set) {
    if (!detector) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        ExternalTransform ext = det->getExternalTransform();

        if (offset_x) *offset_x = ext.offset_x;
        if (offset_y) *offset_y = ext.offset_y;
        if (rotation) *rotation = ext.rotation;
        if (is_set) *is_set = ext.is_set ? 1 : 0;

        return 0;
    } catch (...) {
        return -1;
    }
}

// ==================== 二值图像检测参数接口实现 ====================

int SetBinaryDetectionParams(void* detector, const BinaryDetectionParamsC* params) {
    if (!detector || !params) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        BinaryDetectionParams cpp_params = binaryParamsFromCStruct(params);
        det->setBinaryDetectionParams(cpp_params);
        return 0;
    } catch (...) {
        return -1;
    }
}

int GetBinaryDetectionParams(void* detector, BinaryDetectionParamsC* params) {
    if (!detector || !params) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        const BinaryDetectionParams& cpp_params = det->getBinaryDetectionParams();
        binaryParamsToCStruct(cpp_params, params);
        return 0;
    } catch (...) {
        return -1;
    }
}

void GetDefaultBinaryDetectionParams(BinaryDetectionParamsC* params) {
    if (!params) {
        return;
    }

    BinaryDetectionParams default_params;
    binaryParamsToCStruct(default_params, params);
}

// ==================== 模板匹配器配置接口实现 ====================

int SetMatchMethod(void* detector, int method) {
    if (!detector) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        det->getTemplateMatcher().setMethod(matchMethodFromCInt(method));
        return 0;
    } catch (...) {
        return -1;
    }
}

int GetMatchMethod(void* detector) {
    if (!detector) {
        return MATCH_METHOD_NCC;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return matchMethodToCInt(det->getTemplateMatcher().getMethod());
    } catch (...) {
        return MATCH_METHOD_NCC;
    }
}

int SetBinaryThreshold(void* detector, int threshold) {
    if (!detector) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        det->getTemplateMatcher().setBinaryThreshold(threshold);
        return 0;
    } catch (...) {
        return -1;
    }
}

int GetBinaryThreshold(void* detector) {
    if (!detector) {
        return 128; // 默认值
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->getTemplateMatcher().getBinaryThreshold();
    } catch (...) {
        return 128;
    }
}

// ==================== ROI信息获取接口实现 ====================

int GetROIInfo(void* detector, int index, ROIInfoC* roi_info) {
    if (!detector || !roi_info) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        // Validate index before accessing
        if (index < 0 || index >= static_cast<int>(det->getROICount())) {
            return -1;
        }

        ROIRegion roi = det->getROI(index);

        roi_info->id = roi.id;
        roi_info->x = roi.bounds.x;
        roi_info->y = roi.bounds.y;
        roi_info->width = roi.bounds.width;
        roi_info->height = roi.bounds.height;
        roi_info->similarity_threshold = roi.similarity_threshold;
        roi_info->enabled = 1; // ROIRegion doesn't have 'enabled' field in current API

        return 0;
    } catch (...) {
        return -1;
    }
}

// ==================== 模板信息接口实现 ====================

int GetTemplateSize(void* detector, int* width, int* height, int* channels) {
    if (!detector) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        const cv::Mat& tpl = det->getTemplate();

        if (tpl.empty()) {
            return -1;
        }

        if (width) *width = tpl.cols;
        if (height) *height = tpl.rows;
        if (channels) *channels = tpl.channels();

        return 0;
    } catch (...) {
        return -1;
    }
}

int IsTemplateLoaded(void* detector) {
    if (!detector) {
        return 0;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        return det->getTemplate().empty() ? 0 : 1;
    } catch (...) {
        return 0;
    }
}

// ==================== 完整检测结果接口实现 ====================

int DetectWithFullResult(void* detector, unsigned char* test_image,
                         int width, int height, int channels,
                         DetectionResultC* result) {
    if (!detector || !test_image || !result) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, test_image);

        DetectionResult cpp_result = det->detect(image);
        g_last_processing_time = cpp_result.processing_time_ms;
        g_last_localization_time = cpp_result.localization_time_ms;
        g_last_roi_comparison_time = cpp_result.roi_comparison_time_ms;
        g_last_detection_result = cpp_result;

        // 填充C结构体
        result->overall_passed = cpp_result.overall_passed ? 1 : 0;
        result->total_defect_count = cpp_result.total_defect_count;
        result->processing_time_ms = cpp_result.processing_time_ms;
        result->localization_time_ms = cpp_result.localization_time_ms;
        result->roi_comparison_time_ms = cpp_result.roi_comparison_time_ms;
        result->roi_count = static_cast<int>(cpp_result.roi_results.size());
        localizationInfoToCStruct(cpp_result.localization, &result->localization);

        return cpp_result.overall_passed ? 0 : 1;
    } catch (...) {
        return -1;
    }
}

int DetectFromFileWithFullResult(void* detector, const char* filepath,
                                  DetectionResultC* result) {
    if (!detector || !filepath || !result) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        DetectionResult cpp_result = det->detectFromFile(filepath);
        g_last_processing_time = cpp_result.processing_time_ms;
        g_last_localization_time = cpp_result.localization_time_ms;
        g_last_roi_comparison_time = cpp_result.roi_comparison_time_ms;
        g_last_detection_result = cpp_result;

        // 填充C结构体
        result->overall_passed = cpp_result.overall_passed ? 1 : 0;
        result->total_defect_count = cpp_result.total_defect_count;
        result->processing_time_ms = cpp_result.processing_time_ms;
        result->localization_time_ms = cpp_result.localization_time_ms;
        result->roi_comparison_time_ms = cpp_result.roi_comparison_time_ms;
        result->roi_count = static_cast<int>(cpp_result.roi_results.size());
        localizationInfoToCStruct(cpp_result.localization, &result->localization);

        return cpp_result.overall_passed ? 0 : 1;
    } catch (...) {
        return -1;
    }
}

int GetLastROIResults(void* detector, ROIResultC* roi_results, int max_count, int* actual_count) {
    (void)detector; // 使用静态保存的结果

    if (!roi_results) {
        return -1;
    }

    try {
        int count = static_cast<int>(g_last_detection_result.roi_results.size());
        int copy_count = (count < max_count) ? count : max_count;

        for (int i = 0; i < copy_count; ++i) {
            const auto& cpp_roi = g_last_detection_result.roi_results[i];
            ROIResultC& c_roi = roi_results[i];

            c_roi.roi_id = cpp_roi.roi_id;
            c_roi.passed = cpp_roi.passed ? 1 : 0;
            c_roi.similarity = cpp_roi.similarity;
            c_roi.threshold = cpp_roi.threshold;
            c_roi.defect_count = static_cast<int>(cpp_roi.defects.size());
        }

        if (actual_count) {
            *actual_count = count;
        }

        return 0;
    } catch (...) {
        return -1;
    }
}

int GetLastLocalizationInfo(void* detector, LocalizationInfoC* loc_info) {
    (void)detector; // 使用静态保存的结果

    if (!loc_info) {
        return -1;
    }

    try {
        localizationInfoToCStruct(g_last_detection_result.localization, loc_info);
        return 0;
    } catch (...) {
        return -1;
    }
}

double GetLastLocalizationTime(void* detector) {
    (void)detector;
    return g_last_localization_time;
}

double GetLastROIComparisonTime(void* detector) {
    (void)detector;
    return g_last_roi_comparison_time;
}

// ==================== 一键学习扩展接口实现 ====================

int LearnTemplateEx(void* detector, unsigned char* new_template,
                    int width, int height, int channels,
                    float* quality_score, char* message, int message_size) {
    if (!detector || !new_template) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, new_template);

        LearningResult result = det->learn(image.clone());

        if (quality_score) {
            *quality_score = result.quality_score;
        }

        if (message && message_size > 0) {
            strncpy(message, result.message.c_str(), message_size - 1);
            message[message_size - 1] = '\0';
        }

        return result.success ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

int LearnTemplateFromFile(void* detector, const char* filepath) {
    if (!detector || !filepath) {
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        cv::Mat image = cv::imread(filepath);

        if (image.empty()) {
            return -1;
        }

        LearningResult result = det->learn(image);
        return result.success ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

// ==================== 版本信息接口实现 ====================

const char* GetLibraryVersion(void) {
    return g_library_version;
}

const char* GetBuildInfo(void) {
    return g_build_info;
}

