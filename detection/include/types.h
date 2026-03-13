/**
 * @file types.h
 * @brief Print Defect Detection System - Data Type Definitions
 */

#ifndef DEFECT_DETECTION_TYPES_H
#define DEFECT_DETECTION_TYPES_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>

namespace defect_detection {

/**
 * @brief 瑕疵类型枚举
 */
enum class DefectType {
    UNKNOWN = 0,            ///< 未知类型
    INK_MISSING = 1,        ///< 漏墨（本应有墨的地方没有墨）
    SPRAY_MISSING = 2,      ///< 漏喷（本应喷印的地方没有喷印）
    MIXED_DEFECT = 3        ///< 混合瑕疵（同时存在漏墨和漏喷）
};

/**
 * @brief 将瑕疵类型转换为字符串
 */
inline const char* defectTypeToString(DefectType type) {
    switch (type) {
        case DefectType::INK_MISSING: return "ink_missing";
        case DefectType::SPRAY_MISSING: return "spray_missing";
        case DefectType::MIXED_DEFECT: return "mixed_defect";
        default: return "unknown";
    }
}

/**
 * @brief Rectangle region structure
 */
struct Rect {
    int x;
    int y;
    int width;
    int height;

    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(int _x, int _y, int _w, int _h) : x(_x), y(_y), width(_w), height(_h) {}

    cv::Rect toCvRect() const { return cv::Rect(x, y, width, height); }
    static Rect fromCvRect(const cv::Rect& r) { return Rect(r.x, r.y, r.width, r.height); }

    int area() const { return width * height; }
    bool isValid() const { return width > 0 && height > 0; }
};

/**
 * @brief Point structure
 */
struct Point {
    float x;
    float y;

    Point() : x(0), y(0) {}
    Point(float _x, float _y) : x(_x), y(_y) {}

    cv::Point2f toCvPoint() const { return cv::Point2f(x, y); }
};

/**
 * @brief Detection parameters structure
 */
struct DetectionParams {
    int blur_kernel_size;
    int binary_threshold;
    int min_defect_size;
    bool detect_black_on_white;
    bool detect_white_on_black;
    float similarity_threshold;
    int morphology_kernel_size;

    DetectionParams()
        : blur_kernel_size(3)
        , binary_threshold(30)
        , min_defect_size(10)
        , detect_black_on_white(true)
        , detect_white_on_black(true)
        , similarity_threshold(0.85f)
        , morphology_kernel_size(3)
    {}
};

/**
 * @brief 二值图像专用检测参数
 *
 * 用于优化黑白二值图像的瑕疵检测，提高容错率，降低误报率。
 */
struct BinaryDetectionParams {
    // 功能开关
    bool enabled;                       ///< 是否启用二值图像优化模式
    bool auto_detect_binary;            ///< 自动检测输入是否为二值图像

    // 预处理参数
    int noise_filter_size;              ///< 噪声过滤核大小，默认2（去除2像素以下噪点）

    // 位置容错参数
    int edge_tolerance_pixels;          ///< 边缘容错像素数，默认2（允许2像素边缘偏移）
    float edge_diff_ignore_ratio;       ///< 忽略的边缘差异比例，默认0.05（5%）

    // 区域差异参数
    int min_significant_area;           ///< 最小显著差异面积，默认20像素
    float area_diff_threshold;          ///< 区域差异阈值，默认0.001（0.1%）

    // 综合判定参数
    float overall_similarity_threshold; ///< 总体相似度阈值，默认0.95（95%）

    BinaryDetectionParams()
        : enabled(false)
        , auto_detect_binary(true)
        , noise_filter_size(2)
        , edge_tolerance_pixels(2)
        , edge_diff_ignore_ratio(0.05f)
        , min_significant_area(20)
        , area_diff_threshold(0.001f)
        , overall_similarity_threshold(0.95f)
    {}
};

/**
 * @brief 二值比较结果
 */
struct BinaryComparisonResult {
    float similarity;                   ///< 相似度 (0.0-1.0)
    int edge_diff_pixels;               ///< 边缘差异像素数（可接受的差异）
    int area_diff_pixels;               ///< 区域差异像素数（可能是瑕疵）
    int total_diff_pixels;              ///< 总差异像素数
    bool is_acceptable;                 ///< 是否可接受（通过检测）
    std::vector<Rect> significant_regions;  ///< 显著差异区域列表

    BinaryComparisonResult()
        : similarity(0.0f)
        , edge_diff_pixels(0)
        , area_diff_pixels(0)
        , total_diff_pixels(0)
        , is_acceptable(false)
    {}
};

/**
 * @brief Defect information structure
 */
struct Defect {
    Rect location;              ///< 瑕疵位置（X+Y）
    double area;                ///< 瑕疵面积
    DefectType defect_type;     ///< 瑕疵类型（漏墨/漏喷/混合瑕疵）
    std::string type;           ///< 瑕疵类型字符串（兼容旧代码）
    float severity;             ///< 严重程度 (0.0-1.0)
    float x;                    ///< 瑕疵中心点X坐标
    float y;                    ///< 瑕疵中心点Y坐标

    Defect() : area(0), defect_type(DefectType::UNKNOWN), type("unknown"), severity(0), x(0), y(0) {}
};

/**
 * @brief ROI region structure
 */
struct ROIRegion {
    int id;
    Rect bounds;
    float similarity_threshold;
    cv::Mat template_image;
    DetectionParams params;

    ROIRegion() : id(-1), similarity_threshold(0.85f) {}
    ROIRegion(int _id, const Rect& _bounds, float _threshold = 0.85f)
        : id(_id), bounds(_bounds), similarity_threshold(_threshold) {}
};

/**
 * @brief Single ROI detection result
 */
struct ROIResult {
    int roi_id;
    float similarity;
    float threshold;
    bool passed;
    std::vector<Defect> defects;

    ROIResult() : roi_id(-1), similarity(0), threshold(0.85f), passed(false) {}
};

/**
 * @brief 外部传入的变换参数（X+Y+R）
 */
struct ExternalTransform {
    float offset_x;         ///< X方向偏移
    float offset_y;         ///< Y方向偏移
    float rotation;         ///< 旋转角度（度）
    bool is_set;            ///< 是否已设置

    ExternalTransform() : offset_x(0), offset_y(0), rotation(0), is_set(false) {}
    ExternalTransform(float x, float y, float r) : offset_x(x), offset_y(y), rotation(r), is_set(true) {}
};

/**
 * @brief Localization information structure
 */
struct LocalizationInfo {
    bool success;
    float offset_x;
    float offset_y;
    float rotation_angle;
    float scale;
    cv::Mat transform_matrix;

    // Quality metrics for alignment validation
    float match_quality;    ///< Inlier ratio from RANSAC (0.0-1.0)
    int inlier_count;       ///< Number of inlier matches
    bool from_external;     ///< 是否来自外部传入的变换

    LocalizationInfo()
        : success(false), offset_x(0), offset_y(0)
        , rotation_angle(0), scale(1.0f)
        , match_quality(0), inlier_count(0), from_external(false) {}
};

/**
 * @brief Complete detection result
 */
struct DetectionResult {
    std::vector<ROIResult> roi_results;
    bool overall_passed;
    int total_defect_count;
    double processing_time_ms;        ///< 总处理时间 (ms)
    double localization_time_ms;      ///< 定位校正时间 (ms)
    double roi_comparison_time_ms;    ///< ROI比对时间 (ms)
    LocalizationInfo localization;

    DetectionResult()
        : overall_passed(true), total_defect_count(0), processing_time_ms(0) {}
};

} // namespace defect_detection

#endif // DEFECT_DETECTION_TYPES_H

