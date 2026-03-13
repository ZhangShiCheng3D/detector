/**
 * @file visualization_config.h
 * @brief Visualization configuration system
 */

#ifndef DEFECT_DETECTION_VISUALIZATION_CONFIG_H
#define DEFECT_DETECTION_VISUALIZATION_CONFIG_H

#include <opencv2/opencv.hpp>
#include <string>
#include <fstream>

namespace defect_detection {

/**
 * @brief Output mode enumeration
 */
enum class OutputMode {
    FILE_ONLY,      ///< Save to file only
    DISPLAY_ONLY,   ///< Display window only
    BOTH            ///< Both file and display
};

/**
 * @brief ROI visualization settings
 */
struct ROIBoxConfig {
    bool enabled = true;
    cv::Scalar pass_color = cv::Scalar(0, 255, 0);
    cv::Scalar fail_color = cv::Scalar(0, 0, 255);
    int line_thickness = 2;
};

/**
 * @brief Defect marker settings
 */
struct DefectMarkerConfig {
    bool enabled = true;
    cv::Scalar color = cv::Scalar(255, 0, 255);           // Default defect color (magenta)
    cv::Scalar black_spot_color = cv::Scalar(0, 0, 255);  // Red for black spots
    cv::Scalar white_spot_color = cv::Scalar(255, 165, 0);// Orange for white spots
    int line_thickness = 2;
    float fill_alpha = 0.3f;
    bool show_labels = true;           // Show defect type labels
    bool show_arrows = true;           // Draw arrows pointing to defects
    bool show_size_info = true;        // Show defect size (area) info
    bool show_severity = true;         // Show severity level
    double label_font_scale = 0.5;
    int arrow_length = 30;             // Arrow length in pixels
};

/**
 * @brief Similarity score display settings
 */
struct SimilarityScoreConfig {
    bool enabled = true;
    double font_scale = 0.6;
    int font_thickness = 1;
    cv::Scalar text_color = cv::Scalar(255, 255, 255);
    bool background_enabled = true;
    cv::Scalar background_color = cv::Scalar(0, 0, 0);
};

/**
 * @brief Processing info display settings
 */
struct ProcessingInfoConfig {
    bool enabled = true;
    bool show_time = true;
    bool show_overall_status = true;
    bool show_defect_count = true;
    std::string position = "top_left";
};

/**
 * @brief Difference map settings
 */
struct DifferenceMapConfig {
    bool enabled = false;
    std::string colormap = "jet";
    float alpha = 0.5f;
};

/**
 * @brief Side-by-side comparison settings
 */
struct SideBySideConfig {
    bool enabled = false;
    std::string template_label = "Template";
    std::string test_label = "Test Image";
};

/**
 * @brief Save options settings
 */
struct SaveOptionsConfig {
    std::string format = "jpg";
    int quality = 95;
    std::string filename_prefix = "result_";
    bool include_timestamp = true;
    bool save_failed_only = false;
};

/**
 * @brief Display window settings
 */
struct DisplayOptionsConfig {
    std::string window_name = "Defect Detection Result";
    bool auto_resize = true;
    int max_display_width = 1920;
    int max_display_height = 1080;
    int wait_key_ms = 0;
};

/**
 * @brief Legend display settings
 */
struct LegendConfig {
    bool enabled = true;
    std::string position = "bottom_right";  // top_left, top_right, bottom_left, bottom_right
    cv::Scalar background_color = cv::Scalar(40, 40, 40);
    float background_alpha = 0.8f;
    double font_scale = 0.5;
};

/**
 * @brief 二值图像检测配置
 *
 * 用于优化黑白二值图像的瑕疵检测，提高容错率，降低误报率。
 */
struct BinaryDetectionConfig {
    bool enabled = false;                   ///< 是否启用二值图像优化模式
    bool auto_detect_binary = true;         ///< 自动检测输入是否为二值图像
    int noise_filter_size = 2;              ///< 噪声过滤核大小
    int edge_tolerance_pixels = 2;          ///< 边缘容错像素数
    float edge_diff_ignore_ratio = 0.05f;   ///< 忽略的边缘差异比例
    int min_significant_area = 20;          ///< 最小显著差异面积
    float area_diff_threshold = 0.001f;     ///< 区域差异阈值
    float overall_similarity_threshold = 0.95f;  ///< 总体相似度阈值
};

/**
 * @brief Complete visualization configuration
 */
struct VisualizationConfig {
    bool enabled = true;
    OutputMode output_mode = OutputMode::BOTH;
    std::string output_directory = "output";

    ROIBoxConfig roi_boxes;
    DefectMarkerConfig defect_markers;
    SimilarityScoreConfig similarity_scores;
    ProcessingInfoConfig processing_info;
    DifferenceMapConfig difference_map;
    SideBySideConfig side_by_side;
    SaveOptionsConfig save_options;
    DisplayOptionsConfig display_options;
    LegendConfig legend;
    BinaryDetectionConfig binary_detection;  ///< 二值图像检测配置
    
    /**
     * @brief Load configuration from JSON file
     * @param filepath Path to JSON file
     * @return true if successful
     */
    bool loadFromFile(const std::string& filepath);
    
    /**
     * @brief Save configuration to JSON file
     * @param filepath Path to JSON file
     * @return true if successful
     */
    bool saveToFile(const std::string& filepath) const;
    
    /**
     * @brief Load from JSON string
     * @param json_str JSON string
     * @return true if successful
     */
    bool loadFromString(const std::string& json_str);
    
    /**
     * @brief Get default configuration
     * @return Default config instance
     */
    static VisualizationConfig getDefault();
};

} // namespace defect_detection

#endif // DEFECT_DETECTION_VISUALIZATION_CONFIG_H

