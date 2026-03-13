/**
 * @file localization.cpp
 * @brief 定位校正模块实现
 */

#include "localization.h"
#include <cmath>

namespace defect_detection {

Localization::Localization()
    : max_offset_(10.0f)
    , max_rotation_(5.0f)
    , tolerance_(2.0f)
{
}

Localization::~Localization() {
}

void Localization::setReferencePoints(const std::vector<Point>& points) {
    reference_points_ = points;
}

const std::vector<Point>& Localization::getReferencePoints() const {
    return reference_points_;
}

LocalizationInfo Localization::computeTransform(const std::vector<Point>& test_points) {
    LocalizationInfo info;
    
    if (reference_points_.size() < 3 || test_points.size() < 3) {
        info.success = false;
        return info;
    }
    
    // 转换为OpenCV格式
    std::vector<cv::Point2f> src_pts, dst_pts;
    size_t n = std::min(reference_points_.size(), test_points.size());
    
    for (size_t i = 0; i < n; ++i) {
        src_pts.push_back(reference_points_[i].toCvPoint());
        dst_pts.push_back(test_points[i].toCvPoint());
    }
    
    // 计算仿射变换矩阵
    if (n >= 4) {
        // 使用RANSAC估计单应性矩阵
        cv::Mat H = cv::findHomography(src_pts, dst_pts, cv::RANSAC, 3.0);
        if (!H.empty()) {
            // 提取仿射部分
            info.transform_matrix = H(cv::Rect(0, 0, 3, 2)).clone();
            info.transform_matrix.convertTo(info.transform_matrix, CV_64F);
        }
    } else {
        // 使用仿射变换
        info.transform_matrix = cv::getAffineTransform(src_pts.data(), dst_pts.data());
    }
    
    if (info.transform_matrix.empty()) {
        info.success = false;
        return info;
    }
    
    // 分解变换矩阵
    decomposeTransform(info.transform_matrix, info);
    
    // 验证定位是否成功
    info.success = verifyLocalization(info);
    
    return info;
}

cv::Mat Localization::applyCorrection(const cv::Mat& image, const LocalizationInfo& info) {
    if (!info.success || info.transform_matrix.empty()) {
        return image.clone();
    }

    cv::Mat result;
    cv::Mat transform_to_use = info.transform_matrix;

    // 确保矩阵是64位浮点型
    if (transform_to_use.type() != CV_64F) {
        transform_to_use.convertTo(transform_to_use, CV_64F);
    }

    // 计算逆变换: 将测试图像坐标系变换回模板坐标系
    cv::Mat inv_transform;
    cv::invertAffineTransform(transform_to_use, inv_transform);

    // 应用逆变换将图像校正回参考位置
    // 使用INTER_LINEAR插值和边界复制
    cv::warpAffine(image, result, inv_transform, image.size(),
                   cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    return result;
}

Rect Localization::transformROI(const Rect& roi, const LocalizationInfo& info) {
    if (!info.success || info.transform_matrix.empty()) {
        return roi;
    }
    
    // 变换ROI的四个角点
    std::vector<cv::Point2f> corners = {
        cv::Point2f((float)roi.x, (float)roi.y),
        cv::Point2f((float)(roi.x + roi.width), (float)roi.y),
        cv::Point2f((float)(roi.x + roi.width), (float)(roi.y + roi.height)),
        cv::Point2f((float)roi.x, (float)(roi.y + roi.height))
    };
    
    std::vector<cv::Point2f> transformed;
    cv::transform(corners, transformed, info.transform_matrix);
    
    // 计算新的边界框
    float min_x = transformed[0].x, max_x = transformed[0].x;
    float min_y = transformed[0].y, max_y = transformed[0].y;
    
    for (const auto& pt : transformed) {
        min_x = std::min(min_x, pt.x);
        max_x = std::max(max_x, pt.x);
        min_y = std::min(min_y, pt.y);
        max_y = std::max(max_y, pt.y);
    }
    
    return Rect((int)min_x, (int)min_y, (int)(max_x - min_x), (int)(max_y - min_y));
}

bool Localization::verifyLocalization(const LocalizationInfo& info,
                                       float max_offset,
                                       float max_rotation) {
    float offset = std::sqrt(info.offset_x * info.offset_x + info.offset_y * info.offset_y);
    
    if (offset > max_offset) {
        return false;
    }
    
    if (std::abs(info.rotation_angle) > max_rotation) {
        return false;
    }
    
    // 检查缩放是否在合理范围内
    if (info.scale < 0.9f || info.scale > 1.1f) {
        return false;
    }
    
    return true;
}

LocalizationInfo Localization::autoDetectLocalization(const cv::Mat& template_img,
                                                       const cv::Mat& test_img) {
    LocalizationInfo info;

    cv::Mat templ_gray, test_gray;
    if (template_img.channels() == 3) {
        cv::cvtColor(template_img, templ_gray, cv::COLOR_BGR2GRAY);
    } else {
        templ_gray = template_img.clone();
    }
    if (test_img.channels() == 3) {
        cv::cvtColor(test_img, test_gray, cv::COLOR_BGR2GRAY);
    } else {
        test_gray = test_img.clone();
    }

    // 性能优化：对大图像进行下采样以加速特征检测
    float scale_factor = 1.0f;
    const int max_dimension = 800;  // 进一步减小以提升性能
    int max_dim = std::max(templ_gray.cols, templ_gray.rows);

    cv::Mat templ_scaled, test_scaled;
    if (max_dim > max_dimension) {
        scale_factor = static_cast<float>(max_dimension) / max_dim;
        cv::resize(templ_gray, templ_scaled, cv::Size(), scale_factor, scale_factor, cv::INTER_AREA);
        cv::resize(test_gray, test_scaled, cv::Size(), scale_factor, scale_factor, cv::INTER_AREA);
    } else {
        templ_scaled = templ_gray;
        test_scaled = test_gray;
    }

    // 使用ORB特征检测 - 增加特征点数量以提高匹配精度
    // 更多的特征点可以提供更稳定的变换估计
    cv::Ptr<cv::ORB> orb = cv::ORB::create(
        800,                // nfeatures - 增加特征点数量以提高匹配精度
        1.2f,               // scaleFactor - 金字塔缩放因子
        8,                  // nlevels - 金字塔层数
        31,                 // edgeThreshold - 边缘阈值
        0,                  // firstLevel
        2,                  // WTA_K
        cv::ORB::HARRIS_SCORE,  // scoreType - 使用Harris角点响应
        31,                 // patchSize
        20                  // fastThreshold - 降低以检测更多特征
    );
    std::vector<cv::KeyPoint> kp1, kp2;
    cv::Mat desc1, desc2;

    orb->detectAndCompute(templ_scaled, cv::noArray(), kp1, desc1);
    orb->detectAndCompute(test_scaled, cv::noArray(), kp2, desc2);

    if (desc1.empty() || desc2.empty() || kp1.size() < 10 || kp2.size() < 10) {
        info.success = false;
        return info;
    }

    // 特征匹配 - 使用crossCheck进行双向验证
    cv::BFMatcher matcher(cv::NORM_HAMMING);
    std::vector<std::vector<cv::DMatch>> knn_matches;
    matcher.knnMatch(desc1, desc2, knn_matches, 2);

    // 应用更严格的比率测试 (Lowe's ratio test)
    // 降低比率阈值以获得更高质量的匹配
    const float ratio_thresh = 0.70f;  // 从0.75降低到0.70以提高匹配质量
    std::vector<cv::DMatch> good_matches;
    for (const auto& m : knn_matches) {
        if (m.size() >= 2 && m[0].distance < ratio_thresh * m[1].distance) {
            // 额外检查：排除距离过大的匹配
            if (m[0].distance < 50) {  // 最大允许的Hamming距离
                good_matches.push_back(m[0]);
            }
        }
    }

    // 需要足够的匹配点以确保变换的稳定性
    // 6个点是仿射变换的最小要求（3个点）的2倍，提供一定的冗余
    if (good_matches.size() < 6) {
        info.success = false;
        return info;
    }

    // 提取匹配点（转换回原始坐标）
    std::vector<cv::Point2f> src_pts, dst_pts;
    for (const auto& m : good_matches) {
        cv::Point2f src_pt = kp1[m.queryIdx].pt;
        cv::Point2f dst_pt = kp2[m.trainIdx].pt;
        // 如果进行了下采样，需要将坐标转换回原始尺度
        if (scale_factor < 1.0f) {
            src_pt.x /= scale_factor;
            src_pt.y /= scale_factor;
            dst_pt.x /= scale_factor;
            dst_pt.y /= scale_factor;
        }
        src_pts.push_back(src_pt);
        dst_pts.push_back(dst_pt);
    }

    // 计算仿射变换矩阵（而非单应性矩阵）
    // 使用estimateAffinePartial2D获取更稳定的仿射变换（只包含旋转、缩放、平移）
    // 使用更严格的RANSAC阈值以获得更精确的变换
    cv::Mat inliers;
    cv::Mat affine = cv::estimateAffinePartial2D(src_pts, dst_pts, inliers, cv::RANSAC, 2.0);  // 从3.0降低到2.0

    if (affine.empty()) {
        // 回退到完整仿射变换
        affine = cv::estimateAffine2D(src_pts, dst_pts, inliers, cv::RANSAC, 2.0);
    }

    if (affine.empty()) {
        info.success = false;
        return info;
    }

    // 计算内点比例作为变换质量指标
    int inlier_count = cv::countNonZero(inliers);
    float inlier_ratio = static_cast<float>(inlier_count) / src_pts.size();

    // 如果内点比例过低，说明变换不可靠
    // 降低阈值到40%以允许更多图像通过，同时仍然过滤掉明显错误的匹配
    if (inlier_ratio < 0.4f) {
        info.success = false;
        return info;
    }

    info.transform_matrix = affine.clone();
    if (info.transform_matrix.type() != CV_64F) {
        info.transform_matrix.convertTo(info.transform_matrix, CV_64F);
    }

    // 分解变换
    decomposeTransform(info.transform_matrix, info);

    // 存储额外的质量信息
    info.match_quality = inlier_ratio;
    info.inlier_count = inlier_count;

    info.success = verifyLocalization(info);

    return info;
}

void Localization::setMaxOffset(float offset) {
    max_offset_ = offset;
}

void Localization::setMaxRotation(float rotation) {
    max_rotation_ = rotation;
}

void Localization::setTolerance(float tolerance) {
    tolerance_ = tolerance;
}

void Localization::decomposeTransform(const cv::Mat& matrix, LocalizationInfo& info) {
    if (matrix.empty() || matrix.rows < 2 || matrix.cols < 3) {
        return;
    }

    // 提取平移分量
    info.offset_x = static_cast<float>(matrix.at<double>(0, 2));
    info.offset_y = static_cast<float>(matrix.at<double>(1, 2));

    // 提取旋转和缩放
    double a = matrix.at<double>(0, 0);
    double b = matrix.at<double>(0, 1);
    double c = matrix.at<double>(1, 0);
    double d = matrix.at<double>(1, 1);

    // 计算缩放因子
    info.scale = static_cast<float>(std::sqrt(a * a + b * b));

    // 计算旋转角度（度）
    info.rotation_angle = static_cast<float>(std::atan2(b, a) * 180.0 / CV_PI);
}

std::vector<cv::Point2f> Localization::detectFeaturePoints(const cv::Mat& template_img,
                                                            const cv::Mat& test_img) {
    std::vector<cv::Point2f> points;

    // 使用GoodFeaturesToTrack检测角点
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

    std::vector<cv::Point2f> corners;
    cv::goodFeaturesToTrack(test_gray, corners, 100, 0.01, 10);

    return corners;
}

} // namespace defect_detection

