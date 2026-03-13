/**
 * @file template_matcher.h
 * @brief 模板匹配引擎
 */

#ifndef DEFECT_DETECTION_TEMPLATE_MATCHER_H
#define DEFECT_DETECTION_TEMPLATE_MATCHER_H

#include "types.h"
#include <opencv2/opencv.hpp>

namespace defect_detection {

/**
 * @brief 匹配方法枚举
 */
enum class MatchMethod {
    NCC,        ///< 归一化互相关
    SSIM,       ///< 结构相似性
    NCC_SSIM,   ///< NCC + SSIM组合
    BINARY      ///< 二值化对比 (适用于黑白图像)
};

/**
 * @brief 模板匹配引擎类
 * 
 * 使用NCC和SSIM算法计算图像相似度
 */
class TemplateMatcher {
public:
    TemplateMatcher();
    ~TemplateMatcher();
    
    /**
     * @brief 设置匹配方法
     * @param method 匹配方法
     */
    void setMethod(MatchMethod method);
    
    /**
     * @brief 获取当前匹配方法
     * @return 匹配方法
     */
    MatchMethod getMethod() const;
    
    /**
     * @brief 计算两张图像的相似度
     * @param template_img 模板图像
     * @param test_img 测试图像
     * @return 相似度值 (0.0-1.0)
     */
    float computeSimilarity(const cv::Mat& template_img, const cv::Mat& test_img);
    
    /**
     * @brief 计算NCC相似度
     * @param template_img 模板图像
     * @param test_img 测试图像
     * @return NCC值 (0.0-1.0)
     */
    float computeNCC(const cv::Mat& template_img, const cv::Mat& test_img);
    
    /**
     * @brief 计算SSIM相似度
     * @param template_img 模板图像
     * @param test_img 测试图像
     * @return SSIM值 (0.0-1.0)
     */
    float computeSSIM(const cv::Mat& template_img, const cv::Mat& test_img);

    /**
     * @brief 计算二值化相似度
     * @param template_img 模板图像
     * @param test_img 测试图像
     * @return 相似度值 (0.0-1.0)
     *
     * 将两张图像二值化后，计算像素匹配率。
     * 适用于黑白打印图像（条码、文字、Logo等）。
     */
    float computeBinarySimilarity(const cv::Mat& template_img, const cv::Mat& test_img);

    /**
     * @brief 设置二值化阈值
     * @param threshold 阈值 (0-255)
     */
    void setBinaryThreshold(int threshold);

    /**
     * @brief 获取二值化阈值
     * @return 当前二值化阈值
     */
    int getBinaryThreshold() const;

    // ===== 二值图像专用接口（优化版）=====

    /**
     * @brief 检测图像是否为二值图像
     * @param img 输入图像
     * @param threshold 判定阈值，默认0.95（95%像素为0或255则认为是二值图像）
     * @return true 如果是二值图像
     */
    static bool isBinaryImage(const cv::Mat& img, float threshold = 0.95f);

    /**
     * @brief 智能二值化预处理
     * @param img 输入图像
     * @param force_rebinarize 是否强制重新二值化
     * @return 二值化后的图像
     */
    cv::Mat smartBinarize(const cv::Mat& img, bool force_rebinarize = false);

    /**
     * @brief 二值图像形态学预处理（去噪）
     * @param binary_img 二值图像
     * @param noise_size 噪声过滤核大小
     * @return 预处理后的图像
     */
    cv::Mat binaryMorphologyPreprocess(const cv::Mat& binary_img, int noise_size = 2);

    /**
     * @brief 带位置容错的二值相似度计算（推荐用于二值图像）
     * @param template_img 模板图像
     * @param test_img 测试图像
     * @param tolerance_pixels 边缘容错像素数
     * @return 相似度值 (0.0-1.0)
     */
    float computeTolerantBinarySimilarity(const cv::Mat& template_img,
                                          const cv::Mat& test_img,
                                          int tolerance_pixels = 2);

    /**
     * @brief 基于连通域分析的二值比较（详细结果）
     * @param template_img 模板图像
     * @param test_img 测试图像
     * @param params 二值检测参数
     * @return 详细的比较结果
     */
    BinaryComparisonResult computeConnectedComponentSimilarity(
        const cv::Mat& template_img,
        const cv::Mat& test_img,
        const BinaryDetectionParams& params);

    /**
     * @brief 设置二值检测参数
     * @param params 二值检测参数
     */
    void setBinaryDetectionParams(const BinaryDetectionParams& params);

    /**
     * @brief 获取二值检测参数
     * @return 当前二值检测参数
     */
    const BinaryDetectionParams& getBinaryDetectionParams() const;

    /**
     * @brief 使用图像金字塔加速匹配
     * @param template_img 模板图像
     * @param test_img 测试图像
     * @param levels 金字塔层数
     * @return 相似度值
     */
    float computeSimilarityPyramid(const cv::Mat& template_img, 
                                    const cv::Mat& test_img, 
                                    int levels = 3);
    
    /**
     * @brief 计算差分图像
     * @param template_img 模板图像
     * @param test_img 测试图像
     * @return 差分图像
     */
    cv::Mat computeDifference(const cv::Mat& template_img, const cv::Mat& test_img);
    
    /**
     * @brief 设置SSIM窗口大小
     * @param size 窗口大小 (必须为奇数)
     */
    void setSSIMWindowSize(int size);
    
    /**
     * @brief 设置SSIM的C1, C2参数
     * @param c1 C1参数
     * @param c2 C2参数
     */
    void setSSIMConstants(double c1, double c2);
    
    /**
     * @brief 预处理图像用于匹配
     * @param image 输入图像
     * @param blur_size 高斯滤波核大小
     * @return 预处理后的图像
     */
    cv::Mat preprocess(const cv::Mat& image, int blur_size = 3);

private:
    MatchMethod method_;
    int ssim_window_size_;
    double ssim_c1_;
    double ssim_c2_;
    int binary_threshold_;              ///< 二值化阈值 (默认128)
    BinaryDetectionParams binary_params_;  ///< 二值图像检测参数

    // SSIM内部计算
    float computeSSIMChannel(const cv::Mat& img1, const cv::Mat& img2);

    // 内部辅助函数
    cv::Mat toGray(const cv::Mat& img);
};

} // namespace defect_detection

#endif // DEFECT_DETECTION_TEMPLATE_MATCHER_H

