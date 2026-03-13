/**
 * @file template_matcher.cpp
 * @brief 模板匹配引擎实现
 */

#include "template_matcher.h"
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace defect_detection {

TemplateMatcher::TemplateMatcher()
    : method_(MatchMethod::NCC)
    , ssim_window_size_(11)
    , ssim_c1_(6.5025)   // (0.01 * 255)^2
    , ssim_c2_(58.5225)  // (0.03 * 255)^2
    , binary_threshold_(128)  // 默认二值化阈值
    , binary_params_()   // 使用默认二值检测参数
{
}

TemplateMatcher::~TemplateMatcher() {
}

void TemplateMatcher::setMethod(MatchMethod method) {
    method_ = method;
}

MatchMethod TemplateMatcher::getMethod() const {
    return method_;
}

float TemplateMatcher::computeSimilarity(const cv::Mat& template_img, const cv::Mat& test_img) {
    if (template_img.empty() || test_img.empty()) {
        return 0.0f;
    }
    
    // 确保尺寸匹配
    cv::Mat test_resized;
    if (template_img.size() != test_img.size()) {
        cv::resize(test_img, test_resized, template_img.size());
    } else {
        test_resized = test_img;
    }
    
    switch (method_) {
        case MatchMethod::NCC:
            return computeNCC(template_img, test_resized);
        case MatchMethod::SSIM:
            return computeSSIM(template_img, test_resized);
        case MatchMethod::NCC_SSIM:
            // 组合NCC和SSIM，权重各50%
            return 0.5f * computeNCC(template_img, test_resized) +
                   0.5f * computeSSIM(template_img, test_resized);
        case MatchMethod::BINARY:
            return computeBinarySimilarity(template_img, test_resized);
        default:
            return computeNCC(template_img, test_resized);
    }
}

float TemplateMatcher::computeNCC(const cv::Mat& template_img, const cv::Mat& test_img) {
    cv::Mat templ_gray, test_gray;
    
    // 转换为灰度图
    if (template_img.channels() == 3) {
        cv::cvtColor(template_img, templ_gray, cv::COLOR_BGR2GRAY);
    } else {
        templ_gray = template_img;
    }
    
    if (test_img.channels() == 3) {
        cv::cvtColor(test_img, test_gray, cv::COLOR_BGR2GRAY);
    } else {
        test_gray = test_img;
    }
    
    // 转换为浮点数
    cv::Mat templ_f, test_f;
    templ_gray.convertTo(templ_f, CV_64F);
    test_gray.convertTo(test_f, CV_64F);
    
    // 计算均值
    cv::Scalar templ_mean = cv::mean(templ_f);
    cv::Scalar test_mean = cv::mean(test_f);
    
    // 减去均值
    templ_f -= templ_mean[0];
    test_f -= test_mean[0];
    
    // 计算NCC
    double numerator = templ_f.dot(test_f);
    double templ_norm = cv::norm(templ_f);
    double test_norm = cv::norm(test_f);
    
    if (templ_norm < 1e-10 || test_norm < 1e-10) {
        return 0.0f;
    }
    
    double ncc = numerator / (templ_norm * test_norm);
    
    // 映射到[0, 1]范围
    return static_cast<float>((ncc + 1.0) / 2.0);
}

float TemplateMatcher::computeSSIM(const cv::Mat& template_img, const cv::Mat& test_img) {
    cv::Mat img1, img2;
    
    // 转换为灰度图
    if (template_img.channels() == 3) {
        cv::cvtColor(template_img, img1, cv::COLOR_BGR2GRAY);
    } else {
        img1 = template_img;
    }
    
    if (test_img.channels() == 3) {
        cv::cvtColor(test_img, img2, cv::COLOR_BGR2GRAY);
    } else {
        img2 = test_img;
    }
    
    return computeSSIMChannel(img1, img2);
}

float TemplateMatcher::computeSSIMChannel(const cv::Mat& img1, const cv::Mat& img2) {
    cv::Mat I1, I2;
    img1.convertTo(I1, CV_64F);
    img2.convertTo(I2, CV_64F);
    
    cv::Mat I1_2 = I1.mul(I1);
    cv::Mat I2_2 = I2.mul(I2);
    cv::Mat I1_I2 = I1.mul(I2);
    
    cv::Mat mu1, mu2;
    cv::GaussianBlur(I1, mu1, cv::Size(ssim_window_size_, ssim_window_size_), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(ssim_window_size_, ssim_window_size_), 1.5);
    
    cv::Mat mu1_2 = mu1.mul(mu1);
    cv::Mat mu2_2 = mu2.mul(mu2);
    cv::Mat mu1_mu2 = mu1.mul(mu2);
    
    cv::Mat sigma1_2, sigma2_2, sigma12;
    cv::GaussianBlur(I1_2, sigma1_2, cv::Size(ssim_window_size_, ssim_window_size_), 1.5);
    cv::GaussianBlur(I2_2, sigma2_2, cv::Size(ssim_window_size_, ssim_window_size_), 1.5);
    cv::GaussianBlur(I1_I2, sigma12, cv::Size(ssim_window_size_, ssim_window_size_), 1.5);
    
    sigma1_2 -= mu1_2;
    sigma2_2 -= mu2_2;
    sigma12 -= mu1_mu2;
    
    cv::Mat t1 = 2 * mu1_mu2 + ssim_c1_;
    cv::Mat t2 = 2 * sigma12 + ssim_c2_;
    cv::Mat t3 = t1.mul(t2);
    
    t1 = mu1_2 + mu2_2 + ssim_c1_;
    t2 = sigma1_2 + sigma2_2 + ssim_c2_;
    t1 = t1.mul(t2);
    
    cv::Mat ssim_map;
    cv::divide(t3, t1, ssim_map);
    
    cv::Scalar mssim = cv::mean(ssim_map);
    return static_cast<float>((mssim[0] + 1.0) / 2.0);
}

float TemplateMatcher::computeSimilarityPyramid(const cv::Mat& template_img,
                                                  const cv::Mat& test_img,
                                                  int levels) {
    if (levels <= 1) {
        return computeSimilarity(template_img, test_img);
    }

    // 构建金字塔
    std::vector<cv::Mat> templ_pyramid, test_pyramid;
    templ_pyramid.push_back(template_img);
    test_pyramid.push_back(test_img);

    for (int i = 1; i < levels; ++i) {
        cv::Mat templ_down, test_down;
        cv::pyrDown(templ_pyramid.back(), templ_down);
        cv::pyrDown(test_pyramid.back(), test_down);
        templ_pyramid.push_back(templ_down);
        test_pyramid.push_back(test_down);
    }

    // 从最小分辨率开始匹配
    float similarity = 0.0f;
    for (int i = levels - 1; i >= 0; --i) {
        float level_sim = computeSimilarity(templ_pyramid[i], test_pyramid[i]);
        // 最终层权重最高
        float weight = (i == 0) ? 0.6f : 0.4f / (levels - 1);
        similarity += weight * level_sim;
    }

    return similarity;
}

cv::Mat TemplateMatcher::computeDifference(const cv::Mat& template_img, const cv::Mat& test_img) {
    cv::Mat templ_gray, test_gray;

    if (template_img.channels() == 3) {
        cv::cvtColor(template_img, templ_gray, cv::COLOR_BGR2GRAY);
    } else {
        templ_gray = template_img;
    }

    if (test_img.channels() == 3) {
        cv::cvtColor(test_img, test_gray, cv::COLOR_BGR2GRAY);
    } else {
        test_gray = test_img;
    }

    // 确保尺寸匹配
    if (templ_gray.size() != test_gray.size()) {
        cv::resize(test_gray, test_gray, templ_gray.size());
    }

    cv::Mat diff;
    cv::absdiff(templ_gray, test_gray, diff);
    return diff;
}

void TemplateMatcher::setSSIMWindowSize(int size) {
    if (size % 2 == 0) {
        size += 1;  // 确保为奇数
    }
    ssim_window_size_ = size;
}

void TemplateMatcher::setSSIMConstants(double c1, double c2) {
    ssim_c1_ = c1;
    ssim_c2_ = c2;
}

cv::Mat TemplateMatcher::preprocess(const cv::Mat& image, int blur_size) {
    cv::Mat result;

    if (blur_size > 1) {
        if (blur_size % 2 == 0) {
            blur_size += 1;
        }
        cv::GaussianBlur(image, result, cv::Size(blur_size, blur_size), 0);
    } else {
        result = image.clone();
    }

    return result;
}

void TemplateMatcher::setBinaryThreshold(int threshold) {
    binary_threshold_ = std::clamp(threshold, 0, 255);
}

int TemplateMatcher::getBinaryThreshold() const {
    return binary_threshold_;
}

float TemplateMatcher::computeBinarySimilarity(const cv::Mat& template_img, const cv::Mat& test_img) {
    // 如果启用了二值图像优化模式，使用新的容错算法
    if (binary_params_.enabled) {
        return computeTolerantBinarySimilarity(template_img, test_img,
                                               binary_params_.edge_tolerance_pixels);
    }

    // 原始算法（向后兼容）
    cv::Mat templ_gray = toGray(template_img);
    cv::Mat test_gray = toGray(test_img);

    // 确保尺寸匹配
    if (templ_gray.size() != test_gray.size()) {
        cv::resize(test_gray, test_gray, templ_gray.size());
    }

    // 二值化 - 使用固定阈值
    cv::Mat templ_binary, test_binary;
    cv::threshold(templ_gray, templ_binary, binary_threshold_, 255, cv::THRESH_BINARY);
    cv::threshold(test_gray, test_binary, binary_threshold_, 255, cv::THRESH_BINARY);

    // 计算差异 - 使用异或操作找出不同的像素
    cv::Mat diff;
    cv::bitwise_xor(templ_binary, test_binary, diff);

    // 统计不匹配的像素数
    int mismatch_count = cv::countNonZero(diff);
    int total_pixels = diff.rows * diff.cols;

    if (total_pixels == 0) {
        return 0.0f;
    }

    return 1.0f - (static_cast<float>(mismatch_count) / static_cast<float>(total_pixels));
}

// ===== 二值图像专用接口实现 =====

cv::Mat TemplateMatcher::toGray(const cv::Mat& img) {
    if (img.channels() == 3) {
        cv::Mat gray;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        return gray;
    }
    return img;
}

bool TemplateMatcher::isBinaryImage(const cv::Mat& img, float threshold) {
    cv::Mat gray;
    if (img.channels() == 3) {
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = img;
    }

    // 统计像素值分布
    int histogram[256] = {0};
    for (int i = 0; i < gray.rows; ++i) {
        const uchar* row = gray.ptr<uchar>(i);
        for (int j = 0; j < gray.cols; ++j) {
            histogram[row[j]]++;
        }
    }

    // 计算0和255像素的比例
    int total_pixels = gray.rows * gray.cols;
    int binary_pixels = histogram[0] + histogram[255];
    float binary_ratio = static_cast<float>(binary_pixels) / total_pixels;

    return binary_ratio >= threshold;
}

cv::Mat TemplateMatcher::smartBinarize(const cv::Mat& img, bool force_rebinarize) {
    cv::Mat gray = toGray(img);

    // 检测是否已经是二值图像
    if (!force_rebinarize && isBinaryImage(gray, 0.95f)) {
        // 已经是二值图像，只需清理边缘噪声
        cv::Mat binary;
        cv::threshold(gray, binary, 127, 255, cv::THRESH_BINARY);
        return binary;
    }

    // 非二值图像，使用自适应二值化
    cv::Mat binary;
    cv::adaptiveThreshold(gray, binary, 255,
                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY, 11, 2);
    return binary;
}

cv::Mat TemplateMatcher::binaryMorphologyPreprocess(const cv::Mat& binary_img, int noise_size) {
    if (noise_size < 1) {
        return binary_img.clone();
    }

    cv::Mat result = binary_img.clone();

    // 小核开运算去除孤立噪点
    cv::Mat small_kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, cv::Size(noise_size, noise_size));
    cv::morphologyEx(result, result, cv::MORPH_OPEN, small_kernel);

    // 小核闭运算填补小孔洞
    cv::morphologyEx(result, result, cv::MORPH_CLOSE, small_kernel);

    return result;
}

float TemplateMatcher::computeTolerantBinarySimilarity(const cv::Mat& template_img,
                                                        const cv::Mat& test_img,
                                                        int tolerance_pixels) {
    cv::Mat templ_gray = toGray(template_img);
    cv::Mat test_gray = toGray(test_img);

    // 确保尺寸匹配
    if (templ_gray.size() != test_gray.size()) {
        cv::resize(test_gray, test_gray, templ_gray.size());
    }

    // 智能二值化
    cv::Mat templ_bin = smartBinarize(templ_gray, false);
    cv::Mat test_bin = smartBinarize(test_gray, false);

    // 形态学噪声去除
    int noise_size = binary_params_.noise_filter_size;
    if (noise_size > 0) {
        templ_bin = binaryMorphologyPreprocess(templ_bin, noise_size);
        test_bin = binaryMorphologyPreprocess(test_bin, noise_size);
    }

    // 创建容错掩码：膨胀和腐蚀模板边缘
    int kernel_size = tolerance_pixels * 2 + 1;
    cv::Mat tolerance_kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, cv::Size(kernel_size, kernel_size));

    cv::Mat templ_dilated, templ_eroded;
    cv::dilate(templ_bin, templ_dilated, tolerance_kernel);
    cv::erode(templ_bin, templ_eroded, tolerance_kernel);

    // 容错区域 = 膨胀 - 腐蚀（即边缘附近区域）
    cv::Mat tolerance_zone;
    cv::subtract(templ_dilated, templ_eroded, tolerance_zone);

    // 计算差异
    cv::Mat diff;
    cv::bitwise_xor(templ_bin, test_bin, diff);

    // 排除容错区域内的差异
    cv::Mat significant_diff;
    cv::subtract(diff, tolerance_zone, significant_diff);

    // 额外的形态学过滤：去除小于min_significant_area的差异
    cv::Mat filter_kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(significant_diff, significant_diff, cv::MORPH_OPEN, filter_kernel);

    // 计算有效差异
    int significant_mismatch = cv::countNonZero(significant_diff);
    int total_pixels = diff.rows * diff.cols;

    if (total_pixels == 0) {
        return 0.0f;
    }

    return 1.0f - (static_cast<float>(significant_mismatch) / total_pixels);
}

BinaryComparisonResult TemplateMatcher::computeConnectedComponentSimilarity(
    const cv::Mat& template_img,
    const cv::Mat& test_img,
    const BinaryDetectionParams& params) {

    BinaryComparisonResult result;

    cv::Mat templ_gray = toGray(template_img);
    cv::Mat test_gray = toGray(test_img);

    // 确保尺寸匹配
    if (templ_gray.size() != test_gray.size()) {
        cv::resize(test_gray, test_gray, templ_gray.size());
    }

    // 智能二值化
    cv::Mat templ_bin = smartBinarize(templ_gray, false);
    cv::Mat test_bin = smartBinarize(test_gray, false);

    // 形态学噪声去除
    if (params.noise_filter_size > 0) {
        templ_bin = binaryMorphologyPreprocess(templ_bin, params.noise_filter_size);
        test_bin = binaryMorphologyPreprocess(test_bin, params.noise_filter_size);
    }

    // 计算原始差异
    cv::Mat diff;
    cv::bitwise_xor(templ_bin, test_bin, diff);
    result.total_diff_pixels = cv::countNonZero(diff);

    // 提取模板边缘
    cv::Mat templ_edges;
    cv::Canny(templ_bin, templ_edges, 50, 150);

    // 膨胀边缘以创建边缘容错区域
    int kernel_size = params.edge_tolerance_pixels * 2 + 1;
    cv::Mat edge_kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, cv::Size(kernel_size, kernel_size));
    cv::dilate(templ_edges, templ_edges, edge_kernel);

    // 分离边缘差异和区域差异
    cv::Mat edge_diff, area_diff;
    cv::bitwise_and(diff, templ_edges, edge_diff);
    cv::subtract(diff, edge_diff, area_diff);

    result.edge_diff_pixels = cv::countNonZero(edge_diff);

    // 连通域分析区域差异
    cv::Mat labels, stats, centroids;
    int num_labels = cv::connectedComponentsWithStats(area_diff, labels, stats, centroids);

    for (int i = 1; i < num_labels; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area >= params.min_significant_area) {
            result.area_diff_pixels += area;
            Rect bbox(
                stats.at<int>(i, cv::CC_STAT_LEFT),
                stats.at<int>(i, cv::CC_STAT_TOP),
                stats.at<int>(i, cv::CC_STAT_WIDTH),
                stats.at<int>(i, cv::CC_STAT_HEIGHT)
            );
            result.significant_regions.push_back(bbox);
        }
    }

    // 计算相似度（只考虑区域差异，忽略边缘差异）
    int total_pixels = diff.rows * diff.cols;
    if (total_pixels > 0) {
        result.similarity = 1.0f - (static_cast<float>(result.area_diff_pixels) / total_pixels);
    }

    // 判定是否可接受
    float edge_diff_ratio = static_cast<float>(result.edge_diff_pixels) / total_pixels;
    float area_diff_ratio = static_cast<float>(result.area_diff_pixels) / total_pixels;

    result.is_acceptable = (area_diff_ratio <= params.area_diff_threshold) &&
                           (edge_diff_ratio <= params.edge_diff_ignore_ratio) &&
                           (result.similarity >= params.overall_similarity_threshold);

    return result;
}

void TemplateMatcher::setBinaryDetectionParams(const BinaryDetectionParams& params) {
    binary_params_ = params;
}

const BinaryDetectionParams& TemplateMatcher::getBinaryDetectionParams() const {
    return binary_params_;
}

} // namespace defect_detection

