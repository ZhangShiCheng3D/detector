/**
 * @file template_learner.h
 * @brief 一键学习模块
 */

#ifndef DEFECT_DETECTION_TEMPLATE_LEARNER_H
#define DEFECT_DETECTION_TEMPLATE_LEARNER_H

#include "types.h"
#include "roi_manager.h"
#include "localization.h"
#include <opencv2/opencv.hpp>

namespace defect_detection {

/**
 * @brief 学习模式枚举
 */
enum class LearningMode {
    REPLACE,        ///< 直接替换模板
    INCREMENTAL,    ///< 增量融合
    AVERAGE         ///< 平均融合
};

/**
 * @brief 学习结果结构
 */
struct LearningResult {
    bool success;               ///< 学习是否成功
    std::string message;        ///< 结果消息
    LocalizationInfo localization; ///< 定位信息
    float quality_score;        ///< 图像质量评分
    
    LearningResult() : success(false), quality_score(0) {}
};

/**
 * @brief 一键学习模块类
 * 
 * 支持从新图像学习并更新模板
 */
class TemplateLearner {
public:
    TemplateLearner();
    ~TemplateLearner();
    
    /**
     * @brief 设置ROI管理器
     * @param roi_manager ROI管理器指针
     */
    void setROIManager(ROIManager* roi_manager);
    
    /**
     * @brief 设置定位模块
     * @param localization 定位模块指针
     */
    void setLocalization(Localization* localization);
    
    /**
     * @brief 从图像学习新模板
     * @param new_image 新图像
     * @return 学习结果
     */
    LearningResult learnFromImage(const cv::Mat& new_image);
    
    /**
     * @brief 设置学习模式
     * @param mode 学习模式
     */
    void setLearningMode(LearningMode mode);
    
    /**
     * @brief 获取当前学习模式
     * @return 学习模式
     */
    LearningMode getLearningMode() const;
    
    /**
     * @brief 设置融合系数（用于增量学习）
     * @param alpha 融合系数 (0.0-1.0)
     */
    void setFusionAlpha(float alpha);
    
    /**
     * @brief 获取融合系数
     * @return 融合系数
     */
    float getFusionAlpha() const;
    
    /**
     * @brief 设置最小质量要求
     * @param threshold 质量阈值 (0.0-1.0)
     */
    void setMinQualityThreshold(float threshold);
    
    /**
     * @brief 设置与旧模板的最小相似度要求
     * @param threshold 相似度阈值
     */
    void setMinSimilarityThreshold(float threshold);
    
    /**
     * @brief 验证新图像是否适合学习
     * @param new_image 新图像
     * @return 验证结果
     */
    LearningResult validateImage(const cv::Mat& new_image);
    
    /**
     * @brief 评估图像质量
     * @param image 输入图像
     * @return 质量评分 (0.0-1.0)
     */
    float evaluateImageQuality(const cv::Mat& image);
    
    /**
     * @brief 重置到原始模板
     */
    void reset();
    
    /**
     * @brief 设置原始模板图像
     * @param template_image 模板图像
     */
    void setOriginalTemplate(const cv::Mat& template_image);

private:
    ROIManager* roi_manager_;
    Localization* localization_;
    LearningMode mode_;
    float fusion_alpha_;
    float min_quality_threshold_;
    float min_similarity_threshold_;
    cv::Mat original_template_;
    
    // 融合两个模板
    cv::Mat fuseTemplates(const cv::Mat& old_template, 
                          const cv::Mat& new_template,
                          float alpha);
    
    // 检查图像清晰度
    float computeSharpness(const cv::Mat& image);
    
    // 检查图像亮度
    float computeBrightness(const cv::Mat& image);
};

} // namespace defect_detection

#endif // DEFECT_DETECTION_TEMPLATE_LEARNER_H

