/**
 * @file defect_detector.h
 * @brief 瑕疵检测器主类
 */

#ifndef DEFECT_DETECTION_DEFECT_DETECTOR_H
#define DEFECT_DETECTION_DEFECT_DETECTOR_H

#include "types.h"
#include "roi_manager.h"
#include "template_matcher.h"
#include "defect_analyzer.h"
#include "localization.h"
#include "template_learner.h"
#include <opencv2/opencv.hpp>
#include <memory>
#include <string>

namespace defect_detection {

/**
 * @brief 图像对齐模式
 */
enum class AlignmentMode {
    NONE,           ///< 不进行对齐
    FULL_IMAGE,     ///< 整图校正模式（对整张图像应用变换）
    ROI_ONLY        ///< ROI区域校正模式（仅对ROI区域应用变换，性能优化）
};

/**
 * @brief 瑕疵检测器主类
 *
 * 整合所有模块，提供完整的瑕疵检测功能
 */
class DefectDetector {
public:
    DefectDetector();
    ~DefectDetector();
    
    // ==================== 模板管理 ====================
    
    /**
     * @brief 导入模板图像
     * @param image 模板图像 (BGR格式)
     * @return 是否成功
     */
    bool importTemplate(const cv::Mat& image);
    
    /**
     * @brief 从文件导入模板图像
     * @param filepath 文件路径
     * @return 是否成功
     */
    bool importTemplateFromFile(const std::string& filepath);
    
    /**
     * @brief 获取模板图像
     * @return 模板图像
     */
    const cv::Mat& getTemplate() const;
    
    // ==================== ROI管理 ====================
    
    /**
     * @brief 添加ROI区域
     * @param x 左上角X坐标
     * @param y 左上角Y坐标
     * @param width 宽度
     * @param height 高度
     * @param threshold 相似度阈值
     * @return ROI的ID
     */
    int addROI(int x, int y, int width, int height, float threshold = 0.85f);
    
    /**
     * @brief 移除ROI区域
     * @param roi_id ROI的ID
     * @return 是否成功
     */
    bool removeROI(int roi_id);
    
    /**
     * @brief 设置ROI的阈值
     * @param roi_id ROI的ID
     * @param threshold 阈值
     * @return 是否成功
     */
    bool setROIThreshold(int roi_id, float threshold);
    
    /**
     * @brief 清除所有ROI
     */
    void clearROIs();
    
    /**
     * @brief 获取ROI数量
     * @return ROI数量
     */
    size_t getROICount() const;

    /**
     * @brief 获取指定ROI
     * @param index ROI索引
     * @return ROI区域
     */
    ROIRegion getROI(int index) const;

    // ==================== 定位设置 ====================

    /**
     * @brief 设置定位点（保留兼容性）
     * @param points 定位点列表
     */
    void setLocalizationPoints(const std::vector<Point>& points);

    /**
     * @brief 启用/禁用自动定位（保留兼容性）
     * @param enable 是否启用
     */
    void enableAutoLocalization(bool enable);

    /**
     * @brief 设置对齐模式
     * @param mode 对齐模式
     *   - NONE: 不进行对齐
     *   - FULL_IMAGE: 整图校正（精度优先）
     *   - ROI_ONLY: ROI区域校正（性能优先，推荐）
     */
    void setAlignmentMode(AlignmentMode mode);

    /**
     * @brief 获取当前对齐模式
     * @return 对齐模式
     */
    AlignmentMode getAlignmentMode() const;

    // ==================== 外部变换设置（新API） ====================

    /**
     * @brief 设置外部变换矩阵（X+Y+R）
     * @param offset_x X方向偏移
     * @param offset_y Y方向偏移
     * @param rotation 旋转角度（度）
     * @note 此接口用于外部系统传入已计算好的变换参数
     */
    void setExternalTransform(float offset_x, float offset_y, float rotation);

    /**
     * @brief 清除外部变换设置
     */
    void clearExternalTransform();

    /**
     * @brief 获取当前外部变换参数
     * @return 外部变换参数
     */
    ExternalTransform getExternalTransform() const;

    // ==================== 检测功能 ====================
    
    /**
     * @brief 检测图像瑕疵
     * @param test_image 测试图像
     * @return 检测结果
     */
    DetectionResult detect(const cv::Mat& test_image);
    
    /**
     * @brief 从文件检测图像瑕疵
     * @param filepath 文件路径
     * @return 检测结果
     */
    DetectionResult detectFromFile(const std::string& filepath);
    
    /**
     * @brief 检测并返回带标注的图像
     * @param test_image 测试图像
     * @param result 检测结果
     * @return 带标注的图像
     */
    cv::Mat detectAndVisualize(const cv::Mat& test_image, DetectionResult& result);
    
    // ==================== 一键学习 ====================
    
    /**
     * @brief 一键学习新模板
     * @param new_image 新图像
     * @return 学习结果
     */
    LearningResult learn(const cv::Mat& new_image);
    
    /**
     * @brief 重置到原始模板
     */
    void resetTemplate();
    
    // ==================== 参数配置 ====================
    
    /**
     * @brief 设置全局检测参数
     * @param params 检测参数
     */
    void setGlobalParams(const DetectionParams& params);
    
    /**
     * @brief 获取全局检测参数
     * @return 检测参数
     */
    const DetectionParams& getGlobalParams() const;
    
    /**
     * @brief 设置单个参数
     * @param name 参数名称
     * @param value 参数值
     * @return 是否成功
     */
    bool setParameter(const std::string& name, float value);
    
    /**
     * @brief 获取单个参数
     * @param name 参数名称
     * @return 参数值
     */
    float getParameter(const std::string& name) const;

    /**
     * @brief 设置二值图像检测参数
     * @param params 二值检测参数
     */
    void setBinaryDetectionParams(const BinaryDetectionParams& params);

    /**
     * @brief 获取二值图像检测参数
     * @return 当前二值检测参数
     */
    const BinaryDetectionParams& getBinaryDetectionParams() const;

    // ==================== 获取子模块 ====================
    
    ROIManager& getROIManager() { return roi_manager_; }
    TemplateMatcher& getTemplateMatcher() { return template_matcher_; }
    DefectAnalyzer& getDefectAnalyzer() { return defect_analyzer_; }
    Localization& getLocalization() { return localization_; }
    TemplateLearner& getTemplateLearner() { return template_learner_; }

private:
    cv::Mat template_image_;
    cv::Mat original_template_;
    ROIManager roi_manager_;
    TemplateMatcher template_matcher_;
    DefectAnalyzer defect_analyzer_;
    Localization localization_;
    TemplateLearner template_learner_;
    DetectionParams global_params_;
    bool auto_localization_;
    bool template_loaded_;
    AlignmentMode alignment_mode_;
    ExternalTransform external_transform_;  ///< 外部传入的变换参数

    // 内部处理函数
    ROIResult processROI(const ROIRegion& roi, const cv::Mat& test_image);
    ROIResult processROIWithAlignment(const ROIRegion& roi, const cv::Mat& test_image,
                                       const LocalizationInfo& loc_info);
    cv::Mat preprocessImage(const cv::Mat& image);
    cv::Mat extractAlignedROI(const cv::Mat& image, const Rect& roi_bounds,
                               const LocalizationInfo& loc_info);

    /**
     * @brief 根据外部变换参数构建LocalizationInfo
     */
    LocalizationInfo buildLocalizationFromExternal(const ExternalTransform& ext,
                                                    const cv::Size& image_size);
};

} // namespace defect_detection

#endif // DEFECT_DETECTION_DEFECT_DETECTOR_H

