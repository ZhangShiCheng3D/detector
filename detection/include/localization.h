/**
 * @file localization.h
 * @brief 定位校正模块
 */

#ifndef DEFECT_DETECTION_LOCALIZATION_H
#define DEFECT_DETECTION_LOCALIZATION_H

#include "types.h"
#include <opencv2/opencv.hpp>
#include <vector>

namespace defect_detection {

/**
 * @brief 定位校正模块类
 * 
 * 根据外部定位点计算仿射变换，修正ROI位置
 */
class Localization {
public:
    Localization();
    ~Localization();
    
    /**
     * @brief 设置参考定位点（模板上的点）
     * @param points 参考点列表 (至少3个点)
     */
    void setReferencePoints(const std::vector<Point>& points);
    
    /**
     * @brief 获取参考定位点
     * @return 参考点列表
     */
    const std::vector<Point>& getReferencePoints() const;
    
    /**
     * @brief 根据测试图像的定位点计算变换
     * @param test_points 测试图像上的定位点
     * @return 定位信息
     */
    LocalizationInfo computeTransform(const std::vector<Point>& test_points);
    
    /**
     * @brief 对图像应用定位校正
     * @param image 输入图像
     * @param info 定位信息
     * @return 校正后的图像
     */
    cv::Mat applyCorrection(const cv::Mat& image, const LocalizationInfo& info);
    
    /**
     * @brief 对ROI坐标应用变换
     * @param roi 输入ROI
     * @param info 定位信息
     * @return 变换后的ROI
     */
    Rect transformROI(const Rect& roi, const LocalizationInfo& info);
    
    /**
     * @brief 验证定位是否成功
     * @param info 定位信息
     * @param max_offset 最大允许偏移
     * @param max_rotation 最大允许旋转角度 (度)
     * @return 是否在允许范围内
     */
    bool verifyLocalization(const LocalizationInfo& info,
                            float max_offset = 10.0f,
                            float max_rotation = 5.0f);
    
    /**
     * @brief 自动检测定位点（使用特征匹配）
     * @param template_img 模板图像
     * @param test_img 测试图像
     * @return 定位信息
     */
    LocalizationInfo autoDetectLocalization(const cv::Mat& template_img,
                                             const cv::Mat& test_img);
    
    /**
     * @brief 设置最大允许偏移
     * @param offset 最大偏移 (像素)
     */
    void setMaxOffset(float offset);
    
    /**
     * @brief 设置最大允许旋转
     * @param rotation 最大旋转 (度)
     */
    void setMaxRotation(float rotation);
    
    /**
     * @brief 设置定位容差
     * @param tolerance 容差值 (像素)
     */
    void setTolerance(float tolerance);

private:
    std::vector<Point> reference_points_;
    float max_offset_;
    float max_rotation_;
    float tolerance_;
    
    // 从变换矩阵分解参数
    void decomposeTransform(const cv::Mat& matrix, LocalizationInfo& info);
    
    // 使用ORB特征匹配
    std::vector<cv::Point2f> detectFeaturePoints(const cv::Mat& template_img,
                                                  const cv::Mat& test_img);
};

} // namespace defect_detection

#endif // DEFECT_DETECTION_LOCALIZATION_H

