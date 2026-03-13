/**
 * @file defect_analyzer.h
 * @brief 瑕疵分析引擎
 */

#ifndef DEFECT_DETECTION_DEFECT_ANALYZER_H
#define DEFECT_DETECTION_DEFECT_ANALYZER_H

#include "types.h"
#include <opencv2/opencv.hpp>
#include <vector>

namespace defect_detection {

/**
 * @brief 瑕疵分析引擎类
 * 
 * 负责检测黑色墨点、白色瑕疵和位置偏移
 */
class DefectAnalyzer {
public:
    DefectAnalyzer();
    ~DefectAnalyzer();
    
    /**
     * @brief 设置检测参数
     * @param params 检测参数
     */
    void setParams(const DetectionParams& params);
    
    /**
     * @brief 获取当前检测参数
     * @return 检测参数
     */
    const DetectionParams& getParams() const;
    
    /**
     * @brief 检测白底上的黑色脏污
     * @param test_img 测试图像
     * @param template_img 模板图像
     * @return 检测到的瑕疵列表
     */
    std::vector<Defect> detectBlackOnWhite(const cv::Mat& test_img, 
                                            const cv::Mat& template_img);
    
    /**
     * @brief 检测黑底上的白色瑕疵
     * @param test_img 测试图像
     * @param template_img 模板图像
     * @return 检测到的瑕疵列表
     */
    std::vector<Defect> detectWhiteOnBlack(const cv::Mat& test_img, 
                                            const cv::Mat& template_img);
    
    /**
     * @brief 检测所有类型的瑕疵
     * @param test_img 测试图像
     * @param template_img 模板图像
     * @return 检测到的瑕疵列表
     */
    std::vector<Defect> detectAllDefects(const cv::Mat& test_img, 
                                          const cv::Mat& template_img);
    
    /**
     * @brief 检测位置偏移
     * @param test_img 测试图像
     * @param template_img 模板图像
     * @param max_offset 最大允许偏移 (像素)
     * @return 位置偏移信息，超过阈值时返回瑕疵
     */
    std::vector<Defect> detectPositionShift(const cv::Mat& test_img, 
                                             const cv::Mat& template_img,
                                             float max_offset = 5.0f);
    
    /**
     * @brief 计算差分图像
     * @param test_img 测试图像
     * @param template_img 模板图像
     * @return 差分图像
     */
    cv::Mat computeDifference(const cv::Mat& test_img, const cv::Mat& template_img);
    
    /**
     * @brief 获取二值化差分图像
     * @param diff_img 差分图像
     * @return 二值化图像
     */
    cv::Mat getBinaryDifference(const cv::Mat& diff_img);
    
    /**
     * @brief 设置最小瑕疵尺寸
     * @param size 最小尺寸 (像素)
     */
    void setMinDefectSize(int size);
    
    /**
     * @brief 设置二值化阈值
     * @param threshold 阈值 (0-255)
     */
    void setBinaryThreshold(int threshold);
    
    /**
     * @brief 从二值图像中提取瑕疵
     * @param binary_img 二值图像
     * @param type 瑕疵类型
     * @return 瑕疵列表
     */
    std::vector<Defect> extractDefectsFromBinary(const cv::Mat& binary_img,
                                                  const std::string& type);

private:
    DetectionParams params_;
    
    // 形态学处理
    cv::Mat applyMorphology(const cv::Mat& binary_img);
    
    // 计算瑕疵严重程度
    float computeSeverity(const Defect& defect, const cv::Mat& diff_img);
};

} // namespace defect_detection

#endif // DEFECT_DETECTION_DEFECT_ANALYZER_H

