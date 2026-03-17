/**
 * @file c_api.cpp
 * @brief C语言接口实现
 */

#define DEFECT_DETECTION_EXPORTS
#include "c_api.h"
#include "defect_detector.h"
#include "template_learner.h"
#include "api_logger.h"
#include <opencv2/opencv.hpp>
#include <cstring>
#include <sstream>

using namespace defect_detection;

// 辅助宏：将数值转换为字符串
#define STR(x) #x
#define TO_STR(x) STR(x)

// 辅助函数：格式化参数
static std::string fmtParams(const char* name, int value) {
    std::stringstream ss; ss << name << "=" << value; return ss.str();
}
static std::string fmtParams(const char* name, float value) {
    std::stringstream ss; ss << name << "=" << value; return ss.str();
}
static std::string fmtParams(const char* name, const char* value) {
    std::stringstream ss; ss << name << "=" << (value ? value : "null"); return ss.str();
}
static std::string fmtParams(const char* name, void* value) {
    std::stringstream ss; ss << name << "=" << value; return ss.str();
}

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
    // 版本2.0新增字段
    c_params->edge_defect_size_threshold = cpp_params.edge_defect_size_threshold;
    c_params->edge_distance_multiplier = cpp_params.edge_distance_multiplier;
    c_params->binary_threshold = cpp_params.binary_threshold;
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
    // 版本2.0新增字段（向后兼容：如果传入的结构体没有这些字段，使用默认值）
    cpp_params.edge_defect_size_threshold = c_params->edge_defect_size_threshold > 0 ? 
                                           c_params->edge_defect_size_threshold : 500;
    cpp_params.edge_distance_multiplier = c_params->edge_distance_multiplier > 0 ? 
                                         c_params->edge_distance_multiplier : 2.0f;
    cpp_params.binary_threshold = c_params->binary_threshold > 0 ? 
                                  c_params->binary_threshold : 128;
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
    API_LOG_CALL("CreateDetector", "");
    try {
        DefectDetector* det = new DefectDetector();
        API_LOG_RESULT("CreateDetector", "success, detector=" + std::to_string(reinterpret_cast<uintptr_t>(det)));
        return det;
    } catch (const std::exception& e) {
        API_LOG_RESULT("CreateDetector", std::string("exception: ") + e.what());
        return nullptr;
    } catch (...) {
        API_LOG_RESULT("CreateDetector", "unknown exception");
        return nullptr;
    }
}

void DestroyDetector(void* detector) {
    API_LOG_CALL("DestroyDetector", fmtParams("detector", detector));
    if (detector) {
        delete static_cast<DefectDetector*>(detector);
        API_LOG_RESULT("DestroyDetector", "success");
    } else {
        API_LOG_RESULT("DestroyDetector", "skipped (null detector)");
    }
}

int ImportTemplate(void* detector, unsigned char* bitmap_data,
                   int width, int height, int channels) {
    std::stringstream params;
    params << "detector=" << detector << ", width=" << width 
           << ", height=" << height << ", channels=" << channels;
    API_LOG_CALL("ImportTemplate", params.str());
    
    if (!detector || !bitmap_data) {
        API_LOG_RESULT("ImportTemplate", "failed: invalid parameter");
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        
        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, bitmap_data);
        
        bool success = det->importTemplate(image.clone());
        API_LOG_RESULT("ImportTemplate", success ? "success" : "failed");
        return success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("ImportTemplate", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("ImportTemplate", "unknown exception");
        return -1;
    }
}

int ImportTemplateFromFile(void* detector, const char* filepath) {
    std::stringstream params;
    params << "detector=" << detector << ", filepath=" << (filepath ? filepath : "null");
    API_LOG_CALL("ImportTemplateFromFile", params.str());
    
    if (!detector || !filepath) {
        API_LOG_RESULT("ImportTemplateFromFile", "failed: invalid parameter");
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        bool success = det->importTemplateFromFile(filepath);
        std::stringstream result;
        result << (success ? "success" : "failed") << ", filepath=" << filepath;
        API_LOG_RESULT("ImportTemplateFromFile", result.str());
        return success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("ImportTemplateFromFile", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("ImportTemplateFromFile", "unknown exception");
        return -1;
    }
}

int AddROI(void* detector, int x, int y, int width, int height, float threshold) {
    std::stringstream params;
    params << "detector=" << detector << ", x=" << x << ", y=" << y 
           << ", width=" << width << ", height=" << height << ", threshold=" << threshold;
    API_LOG_CALL("AddROI", params.str());
    
    if (!detector) {
        API_LOG_RESULT("AddROI", "failed: null detector");
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        int roi_id = det->addROI(x, y, width, height, threshold);
        std::stringstream result;
        result << (roi_id >= 0 ? "success" : "failed") << ", roi_id=" << roi_id;
        API_LOG_RESULT("AddROI", result.str());
        API_LOG_CONFIG("ROI", params.str() + ", result=" + result.str());
        return roi_id;
    } catch (const std::exception& e) {
        API_LOG_RESULT("AddROI", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("AddROI", "unknown exception");
        return -1;
    }
}

int RemoveROI(void* detector, int roi_id) {
    std::stringstream params;
    params << "detector=" << detector << ", roi_id=" << roi_id;
    API_LOG_CALL("RemoveROI", params.str());
    
    if (!detector) {
        API_LOG_RESULT("RemoveROI", "failed: null detector");
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        bool success = det->removeROI(roi_id);
        API_LOG_RESULT("RemoveROI", success ? "success" : "failed");
        return success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("RemoveROI", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("RemoveROI", "unknown exception");
        return -1;
    }
}

int SetROIThreshold(void* detector, int roi_id, float threshold) {
    std::stringstream params;
    params << "detector=" << detector << ", roi_id=" << roi_id << ", threshold=" << threshold;
    API_LOG_CALL("SetROIThreshold", params.str());
    
    if (!detector) {
        API_LOG_RESULT("SetROIThreshold", "failed: null detector");
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        bool success = det->setROIThreshold(roi_id, threshold);
        API_LOG_RESULT("SetROIThreshold", success ? "success" : "failed");
        API_LOG_CONFIG("ROI Threshold", params.str() + ", result=" + std::string(success ? "success" : "failed"));
        return success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetROIThreshold", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetROIThreshold", "unknown exception");
        return -1;
    }
}

void ClearROIs(void* detector) {
    API_LOG_CALL("ClearROIs", fmtParams("detector", detector));
    if (detector) {
        static_cast<DefectDetector*>(detector)->clearROIs();
        API_LOG_RESULT("ClearROIs", "success");
    } else {
        API_LOG_RESULT("ClearROIs", "skipped (null detector)");
    }
}

int GetROICount(void* detector) {
    API_LOG_CALL("GetROICount", fmtParams("detector", detector));
    if (!detector) {
        API_LOG_RESULT("GetROICount", "failed: null detector");
        return 0;
    }
    int count = static_cast<int>(static_cast<DefectDetector*>(detector)->getROICount());
    API_LOG_RESULT("GetROICount", "count=" + std::to_string(count));
    return count;
}

int SetLocalizationPoints(void* detector, float* points, int point_count) {
    std::stringstream params;
    params << "detector=" << detector << ", point_count=" << point_count;
    API_LOG_CALL("SetLocalizationPoints", params.str());
    
    if (!detector || !points || point_count < 3) {
        API_LOG_RESULT("SetLocalizationPoints", "failed: invalid parameter");
        return -1;
    }
    
    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        
        std::vector<Point> pts;
        std::stringstream points_str;
        points_str << "points=[";
        for (int i = 0; i < point_count; ++i) {
            pts.push_back(Point(points[i * 2], points[i * 2 + 1]));
            if (i > 0) points_str << ", ";
            points_str << "(" << points[i * 2] << "," << points[i * 2 + 1] << ")";
        }
        points_str << "]";
        
        det->setLocalizationPoints(pts);
        API_LOG_RESULT("SetLocalizationPoints", "success");
        API_LOG_CONFIG("Localization", points_str.str());
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetLocalizationPoints", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetLocalizationPoints", "unknown exception");
        return -1;
    }
}

void EnableAutoLocalization(void* detector, int enable) {
    std::stringstream params;
    params << "detector=" << detector << ", enable=" << (enable != 0 ? "true" : "false");
    API_LOG_CALL("EnableAutoLocalization", params.str());
    
    if (detector) {
        static_cast<DefectDetector*>(detector)->enableAutoLocalization(enable != 0);
        API_LOG_RESULT("EnableAutoLocalization", "success");
        API_LOG_CONFIG("Localization", "auto_localization=" + std::string(enable != 0 ? "enabled" : "disabled"));
    } else {
        API_LOG_RESULT("EnableAutoLocalization", "skipped (null detector)");
    }
}

int SetExternalTransform(void* detector, float offset_x, float offset_y, float rotation) {
    std::stringstream params;
    params << "detector=" << detector << ", offset_x=" << offset_x 
           << ", offset_y=" << offset_y << ", rotation=" << rotation;
    API_LOG_CALL("SetExternalTransform", params.str());
    
    if (!detector) {
        API_LOG_RESULT("SetExternalTransform", "failed: null detector");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        det->setExternalTransform(offset_x, offset_y, rotation);
        API_LOG_RESULT("SetExternalTransform", "success");
        API_LOG_CONFIG("Transform", params.str());
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetExternalTransform", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetExternalTransform", "unknown exception");
        return -1;
    }
}

void ClearExternalTransform(void* detector) {
    API_LOG_CALL("ClearExternalTransform", fmtParams("detector", detector));
    if (detector) {
        static_cast<DefectDetector*>(detector)->clearExternalTransform();
        API_LOG_RESULT("ClearExternalTransform", "success");
    } else {
        API_LOG_RESULT("ClearExternalTransform", "skipped (null detector)");
    }
}

int DetectDefects(void* detector, unsigned char* test_image,
                  int width, int height, int channels,
                  float* similarity_results, int* defect_count) {
    std::stringstream params;
    params << "detector=" << detector << ", image=" << width << "x" << height 
           << "x" << channels;
    API_LOG_CALL("DetectDefects", params.str());
    
    if (!detector || !test_image) {
        API_LOG_RESULT("DetectDefects", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, test_image);

        API_LOG_DETECTION("Starting detection...");
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

        std::stringstream det_info;
        det_info << "Detection completed: roi_count=" << result.roi_results.size()
                 << ", total_defects=" << result.total_defect_count
                 << ", overall_passed=" << (result.overall_passed ? "true" : "false")
                 << ", processing_time=" << result.processing_time_ms << "ms"
                 << ", localization_time=" << result.localization_time_ms << "ms"
                 << ", roi_comparison_time=" << result.roi_comparison_time_ms << "ms";
        
        // 记录每个ROI的检测结果
        for (size_t i = 0; i < result.roi_results.size(); ++i) {
            const auto& roi = result.roi_results[i];
            std::stringstream roi_info;
            roi_info << "  ROI[" << roi.roi_id << "]: similarity=" << roi.similarity
                     << ", threshold=" << roi.threshold
                     << ", passed=" << (roi.passed ? "true" : "false")
                     << ", defect_count=" << roi.defects.size();
            API_LOG_DETECTION(roi_info.str());
            
            // 记录每个瑕疵的详细信息
            for (size_t j = 0; j < roi.defects.size(); ++j) {
                const auto& defect = roi.defects[j];
                std::stringstream defect_info;
                defect_info << "    Defect[" << j << "]: type=" << defectTypeToString(defect.defect_type)
                           << "(" << static_cast<int>(defect.defect_type) << ")"
                           << ", pos=[" << defect.x << "," << defect.y << "]"
                           << ", size=[" << defect.location.width << "x" << defect.location.height << "]"
                           << ", area=" << defect.area
                           << ", severity=" << defect.severity;
                API_LOG_DETECTION(defect_info.str());
            }
        }
        
        API_LOG_DETECTION(det_info.str());
        
        int ret = result.overall_passed ? 0 : 1;
        std::stringstream ret_str;
        ret_str << (ret == 0 ? "PASS" : "FAIL") << " (code=" << ret << ")";
        API_LOG_RESULT("DetectDefects", ret_str.str());
        
        return ret;
    } catch (const std::exception& e) {
        API_LOG_RESULT("DetectDefects", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("DetectDefects", "unknown exception");
        return -1;
    }
}

int DetectDefectsFromFile(void* detector, const char* filepath,
                          float* similarity_results, int* defect_count) {
    std::stringstream params;
    params << "detector=" << detector << ", filepath=" << (filepath ? filepath : "null");
    API_LOG_CALL("DetectDefectsFromFile", params.str());
    
    if (!detector || !filepath) {
        API_LOG_RESULT("DetectDefectsFromFile", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        API_LOG_DETECTION("Starting detection from file...");
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

        // 记录每个ROI的详细检测结果
        for (const auto& roi_result : result.roi_results) {
            std::stringstream roi_info;
            roi_info << "  ROI[" << roi_result.roi_id << "]: similarity=" << roi_result.similarity
                     << ", passed=" << (roi_result.passed ? "true" : "false")
                     << ", defect_count=" << roi_result.defects.size();
            API_LOG_DETECTION(roi_info.str());
            
            // 记录每个瑕疵的详细信息
            for (size_t j = 0; j < roi_result.defects.size(); ++j) {
                const auto& defect = roi_result.defects[j];
                std::stringstream defect_info;
                defect_info << "    Defect[" << j << "]: type=" << defectTypeToString(defect.defect_type)
                           << "(" << static_cast<int>(defect.defect_type) << ")"
                           << ", pos=[" << defect.x << "," << defect.y << "]"
                           << ", size=[" << defect.location.width << "x" << defect.location.height << "]"
                           << ", area=" << defect.area
                           << ", severity=" << defect.severity;
                API_LOG_DETECTION(defect_info.str());
            }
        }

        std::stringstream det_info;
        det_info << "Detection from file completed: roi_count=" << result.roi_results.size()
                 << ", total_defects=" << result.total_defect_count
                 << ", overall_passed=" << (result.overall_passed ? "true" : "false")
                 << ", processing_time=" << result.processing_time_ms << "ms";
        API_LOG_DETECTION(det_info.str());
        
        int ret = result.overall_passed ? 0 : 1;
        API_LOG_RESULT("DetectDefectsFromFile", ret == 0 ? "PASS" : "FAIL");
        
        return ret;
    } catch (const std::exception& e) {
        API_LOG_RESULT("DetectDefectsFromFile", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("DetectDefectsFromFile", "unknown exception");
        return -1;
    }
}

int LearnTemplate(void* detector, unsigned char* new_template,
                  int width, int height, int channels) {
    std::stringstream params;
    params << "detector=" << detector << ", template=" << width << "x" << height << "x" << channels;
    API_LOG_CALL("LearnTemplate", params.str());
    
    if (!detector || !new_template) {
        API_LOG_RESULT("LearnTemplate", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, new_template);

        LearningResult result = det->learn(image.clone());
        std::stringstream ret;
        ret << (result.success ? "success" : "failed") << ", quality_score=" << result.quality_score;
        API_LOG_RESULT("LearnTemplate", ret.str());
        return result.success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("LearnTemplate", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("LearnTemplate", "unknown exception");
        return -1;
    }
}

void ResetTemplate(void* detector) {
    API_LOG_CALL("ResetTemplate", fmtParams("detector", detector));
    if (detector) {
        static_cast<DefectDetector*>(detector)->resetTemplate();
        API_LOG_RESULT("ResetTemplate", "success");
    } else {
        API_LOG_RESULT("ResetTemplate", "skipped (null detector)");
    }
}

int SetParameter(void* detector, const char* param_name, float value) {
    std::stringstream params;
    params << "detector=" << detector << ", param=" << (param_name ? param_name : "null")
           << ", value=" << value;
    API_LOG_CALL("SetParameter", params.str());
    
    if (!detector || !param_name) {
        API_LOG_RESULT("SetParameter", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        bool success = det->setParameter(param_name, value);
        API_LOG_RESULT("SetParameter", success ? "success" : "failed");
        API_LOG_CONFIG("Parameter", params.str() + ", result=" + std::string(success ? "success" : "failed"));
        return success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetParameter", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetParameter", "unknown exception");
        return -1;
    }
}

float GetParameter(void* detector, const char* param_name) {
    std::stringstream params;
    params << "detector=" << detector << ", param=" << (param_name ? param_name : "null");
    API_LOG_CALL("GetParameter", params.str());
    
    if (!detector || !param_name) {
        API_LOG_RESULT("GetParameter", "failed: invalid parameter");
        return 0.0f;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        float value = det->getParameter(param_name);
        std::stringstream ret;
        ret << "value=" << value;
        API_LOG_RESULT("GetParameter", ret.str());
        return value;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetParameter", std::string("exception: ") + e.what());
        return 0.0f;
    } catch (...) {
        API_LOG_RESULT("GetParameter", "unknown exception");
        return 0.0f;
    }
}

double GetLastProcessingTime(void* detector) {
    (void)detector;
    API_LOG_CALL("GetLastProcessingTime", "");
    std::stringstream ret;
    ret << "time=" << g_last_processing_time << "ms";
    API_LOG_RESULT("GetLastProcessingTime", ret.str());
    return g_last_processing_time;
}

int DetectDefectsEx(void* detector, unsigned char* test_image,
                    int width, int height, int channels,
                    DefectInfo* defect_infos, int max_defects,
                    int* actual_defect_count) {
    std::stringstream params;
    params << "detector=" << detector << ", image=" << width << "x" << height 
           << "x" << channels << ", max_defects=" << max_defects;
    API_LOG_CALL("DetectDefectsEx", params.str());
    
    if (!detector || !test_image) {
        API_LOG_RESULT("DetectDefectsEx", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, test_image);

        API_LOG_DETECTION("Starting detection (extended)...");
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

        // 记录每个ROI的详细检测结果
        for (const auto& roi_result : result.roi_results) {
            std::stringstream roi_info;
            roi_info << "  ROI[" << roi_result.roi_id << "]: similarity=" << roi_result.similarity
                     << ", passed=" << (roi_result.passed ? "true" : "false")
                     << ", defect_count=" << roi_result.defects.size();
            API_LOG_DETECTION(roi_info.str());
            
            // 记录每个瑕疵的详细信息
            for (size_t j = 0; j < roi_result.defects.size(); ++j) {
                const auto& defect = roi_result.defects[j];
                std::stringstream defect_info;
                defect_info << "    Defect[" << j << "]: type=" << defectTypeToString(defect.defect_type)
                           << "(" << static_cast<int>(defect.defect_type) << ")"
                           << ", pos=[" << defect.x << "," << defect.y << "]"
                           << ", size=[" << defect.location.width << "x" << defect.location.height << "]"
                           << ", area=" << defect.area
                           << ", severity=" << defect.severity;
                API_LOG_DETECTION(defect_info.str());
            }
        }
        
        std::stringstream det_info;
        det_info << "Detection (extended) completed: roi_count=" << result.roi_results.size()
                 << ", total_defects=" << result.total_defect_count
                 << ", returned_defects=" << defect_idx
                 << ", overall_passed=" << (result.overall_passed ? "true" : "false")
                 << ", processing_time=" << result.processing_time_ms << "ms";
        API_LOG_DETECTION(det_info.str());
        
        int ret = result.overall_passed ? 0 : 1;
        API_LOG_RESULT("DetectDefectsEx", ret == 0 ? "PASS" : "FAIL");
        
        return ret;
    } catch (const std::exception& e) {
        API_LOG_RESULT("DetectDefectsEx", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("DetectDefectsEx", "unknown exception");
        return -1;
    }
}

int DetectDefectsFromFileEx(void* detector, const char* filepath,
                            DefectInfo* defect_infos, int max_defects,
                            int* actual_defect_count) {
    std::stringstream params;
    params << "detector=" << detector << ", filepath=" << (filepath ? filepath : "null")
           << ", max_defects=" << max_defects;
    API_LOG_CALL("DetectDefectsFromFileEx", params.str());
    
    if (!detector || !filepath) {
        API_LOG_RESULT("DetectDefectsFromFileEx", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        API_LOG_DETECTION("Starting detection from file (extended)...");
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

        // 记录每个ROI的详细检测结果
        for (const auto& roi_result : result.roi_results) {
            std::stringstream roi_info;
            roi_info << "  ROI[" << roi_result.roi_id << "]: similarity=" << roi_result.similarity
                     << ", passed=" << (roi_result.passed ? "true" : "false")
                     << ", defect_count=" << roi_result.defects.size();
            API_LOG_DETECTION(roi_info.str());
            
            // 记录每个瑕疵的详细信息
            for (size_t j = 0; j < roi_result.defects.size(); ++j) {
                const auto& defect = roi_result.defects[j];
                std::stringstream defect_info;
                defect_info << "    Defect[" << j << "]: type=" << defectTypeToString(defect.defect_type)
                           << "(" << static_cast<int>(defect.defect_type) << ")"
                           << ", pos=[" << defect.x << "," << defect.y << "]"
                           << ", size=[" << defect.location.width << "x" << defect.location.height << "]"
                           << ", area=" << defect.area
                           << ", severity=" << defect.severity;
                API_LOG_DETECTION(defect_info.str());
            }
        }

        std::stringstream det_info;
        det_info << "Detection from file (extended) completed: roi_count=" << result.roi_results.size()
                 << ", total_defects=" << result.total_defect_count
                 << ", returned_defects=" << defect_idx
                 << ", overall_passed=" << (result.overall_passed ? "true" : "false")
                 << ", processing_time=" << result.processing_time_ms << "ms";
        API_LOG_DETECTION(det_info.str());
        
        int ret = result.overall_passed ? 0 : 1;
        API_LOG_RESULT("DetectDefectsFromFileEx", ret == 0 ? "PASS" : "FAIL");
        
        return ret;
    } catch (const std::exception& e) {
        API_LOG_RESULT("DetectDefectsFromFileEx", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("DetectDefectsFromFileEx", "unknown exception");
        return -1;
    }
}

int SetDefectThreshold(void* detector, int threshold) {
    std::stringstream params;
    params << "detector=" << detector << ", threshold=" << threshold;
    API_LOG_CALL("SetDefectThreshold", params.str());
    
    if (!detector) {
        API_LOG_RESULT("SetDefectThreshold", "failed: null detector");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        bool success = det->setParameter("binary_threshold", static_cast<float>(threshold));
        API_LOG_RESULT("SetDefectThreshold", success ? "success" : "failed");
        API_LOG_CONFIG("Defect Threshold", "binary_threshold=" + std::to_string(threshold));
        return success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetDefectThreshold", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetDefectThreshold", "unknown exception");
        return -1;
    }
}

int SetMinDefectSize(void* detector, int min_size) {
    std::stringstream params;
    params << "detector=" << detector << ", min_size=" << min_size;
    API_LOG_CALL("SetMinDefectSize", params.str());
    
    if (!detector) {
        API_LOG_RESULT("SetMinDefectSize", "failed: null detector");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        bool success = det->setParameter("min_defect_size", static_cast<float>(min_size));
        API_LOG_RESULT("SetMinDefectSize", success ? "success" : "failed");
        API_LOG_CONFIG("Defect Size", "min_defect_size=" + std::to_string(min_size));
        return success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetMinDefectSize", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetMinDefectSize", "unknown exception");
        return -1;
    }
}

// ==================== 对齐模式接口实现 ====================

int SetAlignmentMode(void* detector, int mode) {
    std::stringstream params;
    params << "detector=" << detector << ", mode=" << mode;
    API_LOG_CALL("SetAlignmentMode", params.str());
    
    if (!detector) {
        API_LOG_RESULT("SetAlignmentMode", "failed: null detector");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        det->setAlignmentMode(alignmentModeFromCInt(mode));
        std::string mode_str;
        switch (mode) {
            case 0: mode_str = "NONE"; break;
            case 1: mode_str = "FULL_IMAGE"; break;
            case 2: mode_str = "ROI_ONLY"; break;
            default: mode_str = "UNKNOWN"; break;
        }
        API_LOG_RESULT("SetAlignmentMode", "success, mode=" + mode_str);
        API_LOG_CONFIG("Alignment", "mode=" + mode_str);
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetAlignmentMode", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetAlignmentMode", "unknown exception");
        return -1;
    }
}

int GetAlignmentMode(void* detector) {
    API_LOG_CALL("GetAlignmentMode", fmtParams("detector", detector));
    if (!detector) {
        API_LOG_RESULT("GetAlignmentMode", "failed: null detector");
        return ALIGNMENT_NONE;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        int mode = alignmentModeToCInt(det->getAlignmentMode());
        std::string mode_str;
        switch (mode) {
            case 0: mode_str = "NONE"; break;
            case 1: mode_str = "FULL_IMAGE"; break;
            case 2: mode_str = "ROI_ONLY"; break;
            default: mode_str = "UNKNOWN"; break;
        }
        std::stringstream ret;
        ret << "mode=" << mode << "(" << mode_str << ")";
        API_LOG_RESULT("GetAlignmentMode", ret.str());
        return mode;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetAlignmentMode", std::string("exception: ") + e.what());
        return ALIGNMENT_NONE;
    } catch (...) {
        API_LOG_RESULT("GetAlignmentMode", "unknown exception");
        return ALIGNMENT_NONE;
    }
}

int GetExternalTransform(void* detector, float* offset_x, float* offset_y, float* rotation, int* is_set) {
    API_LOG_CALL("GetExternalTransform", fmtParams("detector", detector));
    if (!detector) {
        API_LOG_RESULT("GetExternalTransform", "failed: null detector");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        ExternalTransform ext = det->getExternalTransform();

        if (offset_x) *offset_x = ext.offset_x;
        if (offset_y) *offset_y = ext.offset_y;
        if (rotation) *rotation = ext.rotation;
        if (is_set) *is_set = ext.is_set ? 1 : 0;

        std::stringstream ret;
        ret << "offset_x=" << ext.offset_x << ", offset_y=" << ext.offset_y 
            << ", rotation=" << ext.rotation << ", is_set=" << (ext.is_set ? "true" : "false");
        API_LOG_RESULT("GetExternalTransform", ret.str());
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetExternalTransform", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("GetExternalTransform", "unknown exception");
        return -1;
    }
}

// ==================== 二值图像检测参数接口实现 ====================

int SetBinaryDetectionParams(void* detector, const BinaryDetectionParamsC* params) {
    std::stringstream call_params;
    call_params << "detector=" << detector << ", params=" << (params ? "valid" : "null");
    API_LOG_CALL("SetBinaryDetectionParams", call_params.str());
    
    if (!detector || !params) {
        API_LOG_RESULT("SetBinaryDetectionParams", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        BinaryDetectionParams cpp_params = binaryParamsFromCStruct(params);
        det->setBinaryDetectionParams(cpp_params);
        
        std::stringstream cfg;
        cfg << "BinaryDetectionParams: enabled=" << (cpp_params.enabled ? "true" : "false")
            << ", auto_detect_binary=" << (cpp_params.auto_detect_binary ? "true" : "false")
            << ", noise_filter_size=" << cpp_params.noise_filter_size
            << ", edge_tolerance_pixels=" << cpp_params.edge_tolerance_pixels
            << ", edge_diff_ignore_ratio=" << cpp_params.edge_diff_ignore_ratio
            << ", min_significant_area=" << cpp_params.min_significant_area
            << ", area_diff_threshold=" << cpp_params.area_diff_threshold
            << ", overall_similarity_threshold=" << cpp_params.overall_similarity_threshold
            << ", edge_defect_size_threshold=" << cpp_params.edge_defect_size_threshold
            << ", edge_distance_multiplier=" << cpp_params.edge_distance_multiplier
            << ", binary_threshold=" << cpp_params.binary_threshold;
        API_LOG_CONFIG("Binary Detection", cfg.str());
        API_LOG_RESULT("SetBinaryDetectionParams", "success");
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetBinaryDetectionParams", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetBinaryDetectionParams", "unknown exception");
        return -1;
    }
}

int GetBinaryDetectionParams(void* detector, BinaryDetectionParamsC* params) {
    std::stringstream call_params;
    call_params << "detector=" << detector << ", params=" << (params ? "valid" : "null");
    API_LOG_CALL("GetBinaryDetectionParams", call_params.str());
    
    if (!detector || !params) {
        API_LOG_RESULT("GetBinaryDetectionParams", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        const BinaryDetectionParams& cpp_params = det->getBinaryDetectionParams();
        binaryParamsToCStruct(cpp_params, params);
        API_LOG_RESULT("GetBinaryDetectionParams", "success");
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetBinaryDetectionParams", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("GetBinaryDetectionParams", "unknown exception");
        return -1;
    }
}

void GetDefaultBinaryDetectionParams(BinaryDetectionParamsC* params) {
    API_LOG_CALL("GetDefaultBinaryDetectionParams", params ? "valid" : "null");
    if (!params) {
        API_LOG_RESULT("GetDefaultBinaryDetectionParams", "skipped (null params)");
        return;
    }

    BinaryDetectionParams default_params;
    binaryParamsToCStruct(default_params, params);
    
    std::stringstream cfg;
    cfg << "Default BinaryDetectionParams: enabled=" << (default_params.enabled ? "true" : "false")
        << ", noise_filter_size=" << default_params.noise_filter_size
        << ", edge_tolerance_pixels=" << default_params.edge_tolerance_pixels
        << ", area_diff_threshold=" << default_params.area_diff_threshold;
    API_LOG_CONFIG("Binary Detection Default", cfg.str());
    API_LOG_RESULT("GetDefaultBinaryDetectionParams", "success");
}

// ==================== 模板匹配器配置接口实现 ====================

int SetMatchMethod(void* detector, int method) {
    std::stringstream params;
    params << "detector=" << detector << ", method=" << method;
    API_LOG_CALL("SetMatchMethod", params.str());
    
    if (!detector) {
        API_LOG_RESULT("SetMatchMethod", "failed: null detector");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        det->getTemplateMatcher().setMethod(matchMethodFromCInt(method));
        std::string method_str;
        switch (method) {
            case 0: method_str = "NCC"; break;
            case 1: method_str = "SSIM"; break;
            case 2: method_str = "NCC_SSIM"; break;
            case 3: method_str = "BINARY"; break;
            default: method_str = "UNKNOWN"; break;
        }
        API_LOG_RESULT("SetMatchMethod", "success, method=" + method_str);
        API_LOG_CONFIG("Match Method", "method=" + method_str);
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetMatchMethod", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetMatchMethod", "unknown exception");
        return -1;
    }
}

int GetMatchMethod(void* detector) {
    API_LOG_CALL("GetMatchMethod", fmtParams("detector", detector));
    if (!detector) {
        API_LOG_RESULT("GetMatchMethod", "failed: null detector");
        return MATCH_METHOD_NCC;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        int method = matchMethodToCInt(det->getTemplateMatcher().getMethod());
        std::string method_str;
        switch (method) {
            case 0: method_str = "NCC"; break;
            case 1: method_str = "SSIM"; break;
            case 2: method_str = "NCC_SSIM"; break;
            case 3: method_str = "BINARY"; break;
            default: method_str = "UNKNOWN"; break;
        }
        std::stringstream ret;
        ret << "method=" << method << "(" << method_str << ")";
        API_LOG_RESULT("GetMatchMethod", ret.str());
        return method;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetMatchMethod", std::string("exception: ") + e.what());
        return MATCH_METHOD_NCC;
    } catch (...) {
        API_LOG_RESULT("GetMatchMethod", "unknown exception");
        return MATCH_METHOD_NCC;
    }
}

int SetBinaryThreshold(void* detector, int threshold) {
    std::stringstream params;
    params << "detector=" << detector << ", threshold=" << threshold;
    API_LOG_CALL("SetBinaryThreshold", params.str());
    
    if (!detector) {
        API_LOG_RESULT("SetBinaryThreshold", "failed: null detector");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        det->getTemplateMatcher().setBinaryThreshold(threshold);
        API_LOG_RESULT("SetBinaryThreshold", "success");
        API_LOG_CONFIG("Binary Threshold", "threshold=" + std::to_string(threshold));
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("SetBinaryThreshold", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("SetBinaryThreshold", "unknown exception");
        return -1;
    }
}

int GetBinaryThreshold(void* detector) {
    API_LOG_CALL("GetBinaryThreshold", fmtParams("detector", detector));
    if (!detector) {
        API_LOG_RESULT("GetBinaryThreshold", "failed: null detector");
        return 128; // 默认值
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        int threshold = det->getTemplateMatcher().getBinaryThreshold();
        std::stringstream ret;
        ret << "threshold=" << threshold;
        API_LOG_RESULT("GetBinaryThreshold", ret.str());
        return threshold;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetBinaryThreshold", std::string("exception: ") + e.what());
        return 128;
    } catch (...) {
        API_LOG_RESULT("GetBinaryThreshold", "unknown exception");
        return 128;
    }
}

// ==================== ROI信息获取接口实现 ====================

int GetROIInfo(void* detector, int index, ROIInfoC* roi_info) {
    std::stringstream params;
    params << "detector=" << detector << ", index=" << index;
    API_LOG_CALL("GetROIInfo", params.str());
    
    if (!detector || !roi_info) {
        API_LOG_RESULT("GetROIInfo", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        // Validate index before accessing
        if (index < 0 || index >= static_cast<int>(det->getROICount())) {
            API_LOG_RESULT("GetROIInfo", "failed: index out of range");
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

        std::stringstream ret;
        ret << "id=" << roi.id << ", bounds=[" << roi.bounds.x << "," << roi.bounds.y 
            << "," << roi.bounds.width << "," << roi.bounds.height << "]"
            << ", threshold=" << roi.similarity_threshold;
        API_LOG_RESULT("GetROIInfo", ret.str());
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetROIInfo", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("GetROIInfo", "unknown exception");
        return -1;
    }
}

// ==================== 模板信息接口实现 ====================

int GetTemplateSize(void* detector, int* width, int* height, int* channels) {
    API_LOG_CALL("GetTemplateSize", fmtParams("detector", detector));
    if (!detector) {
        API_LOG_RESULT("GetTemplateSize", "failed: null detector");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        const cv::Mat& tpl = det->getTemplate();

        if (tpl.empty()) {
            API_LOG_RESULT("GetTemplateSize", "failed: template not loaded");
            return -1;
        }

        if (width) *width = tpl.cols;
        if (height) *height = tpl.rows;
        if (channels) *channels = tpl.channels();

        std::stringstream ret;
        ret << "width=" << tpl.cols << ", height=" << tpl.rows << ", channels=" << tpl.channels();
        API_LOG_RESULT("GetTemplateSize", ret.str());
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetTemplateSize", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("GetTemplateSize", "unknown exception");
        return -1;
    }
}

int IsTemplateLoaded(void* detector) {
    API_LOG_CALL("IsTemplateLoaded", fmtParams("detector", detector));
    if (!detector) {
        API_LOG_RESULT("IsTemplateLoaded", "failed: null detector");
        return 0;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        int loaded = det->getTemplate().empty() ? 0 : 1;
        std::stringstream ret;
        ret << (loaded ? "true" : "false");
        API_LOG_RESULT("IsTemplateLoaded", ret.str());
        return loaded;
    } catch (const std::exception& e) {
        API_LOG_RESULT("IsTemplateLoaded", std::string("exception: ") + e.what());
        return 0;
    } catch (...) {
        API_LOG_RESULT("IsTemplateLoaded", "unknown exception");
        return 0;
    }
}

int ExtractROITemplates(void* detector) {
    API_LOG_CALL("ExtractROITemplates", fmtParams("detector", detector));
    
    if (!detector) {
        API_LOG_RESULT("ExtractROITemplates", "failed: null detector");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        if (det->getTemplate().empty()) {
            API_LOG_RESULT("ExtractROITemplates", "failed: template not loaded");
            return -1;
        }
        
        det->getROIManager().extractTemplates(det->getTemplate());
        API_LOG_RESULT("ExtractROITemplates", "success");
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("ExtractROITemplates", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("ExtractROITemplates", "unknown exception");
        return -1;
    }
}

// ==================== 完整检测结果接口实现 ====================

int DetectWithFullResult(void* detector, unsigned char* test_image,
                         int width, int height, int channels,
                         DetectionResultC* result) {
    std::stringstream params;
    params << "detector=" << detector << ", image=" << width << "x" << height << "x" << channels;
    API_LOG_CALL("DetectWithFullResult", params.str());
    
    if (!detector || !test_image || !result) {
        API_LOG_RESULT("DetectWithFullResult", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        int cv_type = (channels == 1) ? CV_8UC1 : CV_8UC3;
        cv::Mat image(height, width, cv_type, test_image);

        API_LOG_DETECTION("Starting detection with full result...");
        DetectionResult cpp_result = det->detect(image);
        g_last_processing_time = cpp_result.processing_time_ms;
        g_last_localization_time = cpp_result.localization_time_ms;
        g_last_roi_comparison_time = cpp_result.roi_comparison_time_ms;
        g_last_detection_result = cpp_result;

        // 记录每个ROI的详细检测结果
        for (const auto& roi_result : cpp_result.roi_results) {
            std::stringstream roi_info;
            roi_info << "  ROI[" << roi_result.roi_id << "]: similarity=" << roi_result.similarity
                     << ", passed=" << (roi_result.passed ? "true" : "false")
                     << ", defect_count=" << roi_result.defects.size();
            API_LOG_DETECTION(roi_info.str());
            
            // 记录每个瑕疵的详细信息
            for (size_t j = 0; j < roi_result.defects.size(); ++j) {
                const auto& defect = roi_result.defects[j];
                std::stringstream defect_info;
                defect_info << "    Defect[" << j << "]: type=" << defectTypeToString(defect.defect_type)
                           << "(" << static_cast<int>(defect.defect_type) << ")"
                           << ", pos=[" << defect.x << "," << defect.y << "]"
                           << ", size=[" << defect.location.width << "x" << defect.location.height << "]"
                           << ", area=" << defect.area
                           << ", severity=" << defect.severity;
                API_LOG_DETECTION(defect_info.str());
            }
        }

        // 填充C结构体
        result->overall_passed = cpp_result.overall_passed ? 1 : 0;
        result->total_defect_count = cpp_result.total_defect_count;
        result->processing_time_ms = cpp_result.processing_time_ms;
        result->localization_time_ms = cpp_result.localization_time_ms;
        result->roi_comparison_time_ms = cpp_result.roi_comparison_time_ms;
        result->roi_count = static_cast<int>(cpp_result.roi_results.size());
        localizationInfoToCStruct(cpp_result.localization, &result->localization);

        std::stringstream det_info;
        det_info << "Detection with full result completed: roi_count=" << cpp_result.roi_results.size()
                 << ", total_defects=" << cpp_result.total_defect_count
                 << ", overall_passed=" << (cpp_result.overall_passed ? "true" : "false")
                 << ", processing_time=" << cpp_result.processing_time_ms << "ms";
        API_LOG_DETECTION(det_info.str());
        
        int ret = cpp_result.overall_passed ? 0 : 1;
        API_LOG_RESULT("DetectWithFullResult", ret == 0 ? "PASS" : "FAIL");
        
        return ret;
    } catch (const std::exception& e) {
        API_LOG_RESULT("DetectWithFullResult", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("DetectWithFullResult", "unknown exception");
        return -1;
    }
}

int DetectFromFileWithFullResult(void* detector, const char* filepath,
                                  DetectionResultC* result) {
    std::stringstream params;
    params << "detector=" << detector << ", filepath=" << (filepath ? filepath : "null");
    API_LOG_CALL("DetectFromFileWithFullResult", params.str());
    
    if (!detector || !filepath || !result) {
        API_LOG_RESULT("DetectFromFileWithFullResult", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);

        API_LOG_DETECTION("Starting detection from file with full result...");
        DetectionResult cpp_result = det->detectFromFile(filepath);
        g_last_processing_time = cpp_result.processing_time_ms;
        g_last_localization_time = cpp_result.localization_time_ms;
        g_last_roi_comparison_time = cpp_result.roi_comparison_time_ms;
        g_last_detection_result = cpp_result;

        // 记录每个ROI的详细检测结果
        for (const auto& roi_result : cpp_result.roi_results) {
            std::stringstream roi_info;
            roi_info << "  ROI[" << roi_result.roi_id << "]: similarity=" << roi_result.similarity
                     << ", passed=" << (roi_result.passed ? "true" : "false")
                     << ", defect_count=" << roi_result.defects.size();
            API_LOG_DETECTION(roi_info.str());
            
            // 记录每个瑕疵的详细信息
            for (size_t j = 0; j < roi_result.defects.size(); ++j) {
                const auto& defect = roi_result.defects[j];
                std::stringstream defect_info;
                defect_info << "    Defect[" << j << "]: type=" << defectTypeToString(defect.defect_type)
                           << "(" << static_cast<int>(defect.defect_type) << ")"
                           << ", pos=[" << defect.x << "," << defect.y << "]"
                           << ", size=[" << defect.location.width << "x" << defect.location.height << "]"
                           << ", area=" << defect.area
                           << ", severity=" << defect.severity;
                API_LOG_DETECTION(defect_info.str());
            }
        }

        // 填充C结构体
        result->overall_passed = cpp_result.overall_passed ? 1 : 0;
        result->total_defect_count = cpp_result.total_defect_count;
        result->processing_time_ms = cpp_result.processing_time_ms;
        result->localization_time_ms = cpp_result.localization_time_ms;
        result->roi_comparison_time_ms = cpp_result.roi_comparison_time_ms;
        result->roi_count = static_cast<int>(cpp_result.roi_results.size());
        localizationInfoToCStruct(cpp_result.localization, &result->localization);

        std::stringstream det_info;
        det_info << "Detection from file with full result completed: roi_count=" << cpp_result.roi_results.size()
                 << ", total_defects=" << cpp_result.total_defect_count
                 << ", overall_passed=" << (cpp_result.overall_passed ? "true" : "false")
                 << ", processing_time=" << cpp_result.processing_time_ms << "ms";
        API_LOG_DETECTION(det_info.str());
        
        int ret = cpp_result.overall_passed ? 0 : 1;
        API_LOG_RESULT("DetectFromFileWithFullResult", ret == 0 ? "PASS" : "FAIL");
        
        return ret;
    } catch (const std::exception& e) {
        API_LOG_RESULT("DetectFromFileWithFullResult", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("DetectFromFileWithFullResult", "unknown exception");
        return -1;
    }
}

int GetLastROIResults(void* detector, ROIResultC* roi_results, int max_count, int* actual_count) {
    (void)detector; // 使用静态保存的结果
    std::stringstream params;
    params << "max_count=" << max_count;
    API_LOG_CALL("GetLastROIResults", params.str());

    if (!roi_results) {
        API_LOG_RESULT("GetLastROIResults", "failed: null roi_results");
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

        std::stringstream ret;
        ret << "roi_count=" << count << ", copied=" << copy_count;
        API_LOG_RESULT("GetLastROIResults", ret.str());
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetLastROIResults", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("GetLastROIResults", "unknown exception");
        return -1;
    }
}

int GetLastLocalizationInfo(void* detector, LocalizationInfoC* loc_info) {
    (void)detector; // 使用静态保存的结果
    API_LOG_CALL("GetLastLocalizationInfo", "");

    if (!loc_info) {
        API_LOG_RESULT("GetLastLocalizationInfo", "failed: null loc_info");
        return -1;
    }

    try {
        localizationInfoToCStruct(g_last_detection_result.localization, loc_info);
        const auto& loc = g_last_detection_result.localization;
        std::stringstream ret;
        ret << "success=" << (loc.success ? "true" : "false")
            << ", offset=[" << loc.offset_x << "," << loc.offset_y << "]"
            << ", rotation=" << loc.rotation_angle
            << ", match_quality=" << loc.match_quality;
        API_LOG_RESULT("GetLastLocalizationInfo", ret.str());
        return 0;
    } catch (const std::exception& e) {
        API_LOG_RESULT("GetLastLocalizationInfo", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("GetLastLocalizationInfo", "unknown exception");
        return -1;
    }
}

double GetLastLocalizationTime(void* detector) {
    (void)detector;
    API_LOG_CALL("GetLastLocalizationTime", "");
    std::stringstream ret;
    ret << "time=" << g_last_localization_time << "ms";
    API_LOG_RESULT("GetLastLocalizationTime", ret.str());
    return g_last_localization_time;
}

double GetLastROIComparisonTime(void* detector) {
    (void)detector;
    API_LOG_CALL("GetLastROIComparisonTime", "");
    std::stringstream ret;
    ret << "time=" << g_last_roi_comparison_time << "ms";
    API_LOG_RESULT("GetLastROIComparisonTime", ret.str());
    return g_last_roi_comparison_time;
}

// ==================== 一键学习扩展接口实现 ====================

int LearnTemplateEx(void* detector, unsigned char* new_template,
                    int width, int height, int channels,
                    float* quality_score, char* message, int message_size) {
    std::stringstream params;
    params << "detector=" << detector << ", template=" << width << "x" << height << "x" << channels;
    API_LOG_CALL("LearnTemplateEx", params.str());
    
    if (!detector || !new_template) {
        API_LOG_RESULT("LearnTemplateEx", "failed: invalid parameter");
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

        std::stringstream ret;
        ret << (result.success ? "success" : "failed") 
            << ", quality_score=" << result.quality_score;
        API_LOG_RESULT("LearnTemplateEx", ret.str());
        return result.success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("LearnTemplateEx", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("LearnTemplateEx", "unknown exception");
        return -1;
    }
}

int LearnTemplateFromFile(void* detector, const char* filepath) {
    std::stringstream params;
    params << "detector=" << detector << ", filepath=" << (filepath ? filepath : "null");
    API_LOG_CALL("LearnTemplateFromFile", params.str());
    
    if (!detector || !filepath) {
        API_LOG_RESULT("LearnTemplateFromFile", "failed: invalid parameter");
        return -1;
    }

    try {
        DefectDetector* det = static_cast<DefectDetector*>(detector);
        cv::Mat image = cv::imread(filepath);

        if (image.empty()) {
            API_LOG_RESULT("LearnTemplateFromFile", "failed: cannot load image");
            return -1;
        }

        LearningResult result = det->learn(image);
        std::stringstream ret;
        ret << (result.success ? "success" : "failed") 
            << ", quality_score=" << result.quality_score;
        API_LOG_RESULT("LearnTemplateFromFile", ret.str());
        return result.success ? 0 : -1;
    } catch (const std::exception& e) {
        API_LOG_RESULT("LearnTemplateFromFile", std::string("exception: ") + e.what());
        return -1;
    } catch (...) {
        API_LOG_RESULT("LearnTemplateFromFile", "unknown exception");
        return -1;
    }
}

// ==================== 版本信息接口实现 ====================

const char* GetLibraryVersion(void) {
    API_LOG_CALL("GetLibraryVersion", "");
    API_LOG_RESULT("GetLibraryVersion", g_library_version);
    return g_library_version;
}

const char* GetBuildInfo(void) {
    API_LOG_CALL("GetBuildInfo", "");
    API_LOG_RESULT("GetBuildInfo", g_build_info);
    return g_build_info;
}

// ==================== 日志控制接口实现 ====================

void SetAPILogFile(const char* filepath) {
    std::string path = filepath ? filepath : "api_debug.log";
    APILogger::getInstance().setLogFile(path);
    // 注意：这里不能用API_LOG_XXX宏，因为setLogFile会关闭和重新打开文件
}

void EnableAPILog(int enabled) {
    APILogger::getInstance().setEnabled(enabled != 0);
    if (enabled) {
        API_LOG_CALL("EnableAPILog", "enabled=true");
        API_LOG_RESULT("EnableAPILog", "API logging enabled");
    }
}

void ClearAPILog(void) {
    // 获取当前日志文件路径
    APILogger::getInstance().setLogFile("api_debug.log");
    API_LOG_CALL("ClearAPILog", "");
    API_LOG_RESULT("ClearAPILog", "log file cleared");
}

